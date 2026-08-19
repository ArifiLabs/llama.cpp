// Computes a per-(kv-head, channel) mean-centering bias for the K cache, by running a plain
// text calibration corpus through the model and averaging the K tensor that is about to be
// written into the cache (see the "k_cache_in" tag added in llm_graph_context::build_attn(),
// src/llama-graph.cpp). The result is written as a GGUF file consumable by
// llama_kv_cache::load_kv_mean_center() via the --kv-mean-center CLI flag.
//
// See docs/kv-mean-center.md for the full picture (what the bias is used for, why it is
// exactly safe to apply, and the current GGML_TYPE_Q4_0-only scope).

#include "arg.h"
#include "common.h"
#include "kv-mean-center.h"
#include "log.h"
#include "llama.h"
#include "gguf.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Accumulates sum(K) and a token count per model layer, from every tensor named
// "k_cache_in-<il>" seen during graph evaluation. The mean (sum / count) is the bias we write out.
class kv_mean_collector {
public:
    bool collect(struct ggml_tensor * t, bool ask);

    std::vector<common_kv_mean_center_layer> finalize() const;

    void enable_probe() { m_probe_enabled = true; }
    bool write_probe(const std::string & output_path, const std::string & calibration_path) const;

    bool saw_k_rot() const { return m_saw_k_rot; }

private:
    std::mutex m_mutex;
    std::vector<uint8_t> m_host_buf;
    std::vector<float>   m_f32_buf; // scratch space for F16/BF16 -> F32 conversion

    std::unordered_map<int32_t, std::vector<double>> m_sum;   // il -> [n_embd_head * n_head]
    std::unordered_map<int32_t, int64_t>              m_count; // il -> total tokens seen

    // whether the captured K tensors were produced with the Hadamard K-cache rotation
    // active, detected from the graph itself: the rotation is a mul_mat against the
    // "attn_inp_k_rot" input, so it shows up in the ancestry of "k_cache_in-<il>".
    // recorded in the output file so the loader can reject a basis mismatch.
    bool m_saw_k_rot = false;

    struct probe_layer {
        std::vector<double> delta_sum;
        int64_t             token_count = 0;
        double              max_abs_delta = 0.0;
        std::string         pre_buffer;
        std::string         post_buffer;
    };

    bool m_probe_enabled = false;
    bool m_probe_failed  = false;
    std::unordered_map<int32_t, std::vector<float>> m_probe_pre;
    std::unordered_map<int32_t, probe_layer>        m_probe_layers;
};

// look for the "attn_inp_k_rot" input within a few links of the captured tensor. matched as a
// substring: views append a suffix ("attn_inp_k_rot (reshaped)") and the backend scheduler
// decorates split inputs with a backend prefix and split index ("MTL0#attn_inp_k_rot#0")
static bool tensor_has_k_rot_ancestor(const struct ggml_tensor * t, int depth = 8) {
    if (t == nullptr || depth < 0) {
        return false;
    }
    if (strstr(t->name, "attn_inp_k_rot") != nullptr) {
        return true;
    }
    for (int i = 0; i < GGML_MAX_SRC; ++i) {
        if (t->src[i] && tensor_has_k_rot_ancestor(t->src[i], depth - 1)) {
            return true;
        }
    }
    return false;
}

// "k_cache_in-<il>" -> il, as formatted by llm_graph_context::cb() (ggml_format_name("%s-%d", ...))
static bool parse_layer_name(const char * name, const char * prefix, int32_t & il) {
    const size_t n = strlen(prefix);

    if (strncmp(name, prefix, n) != 0) {
        return false;
    }

    char * end = nullptr;
    const long v = strtol(name + n, &end, 10);
    if (end == name + n || *end != '\0') {
        return false;
    }

    il = (int32_t) v;
    return true;
}

bool kv_mean_collector::collect(struct ggml_tensor * t, bool ask) {
    int32_t il = -1;
    const bool is_pre  = parse_layer_name(t->name, "k_cache_in-", il);
    const bool is_post = !is_pre && m_probe_enabled &&
            parse_layer_name(t->name, "k_cache_centered-", il);
    if (!is_pre && !is_post) {
        return false;
    }

    if (ask) {
        // yes, we want the actual data for this tensor once it's computed
        return true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (is_pre && !m_saw_k_rot && tensor_has_k_rot_ancestor(t)) {
        m_saw_k_rot = true;
    }

    // cpy_k() can be fed an F32, F16, or BF16 Kcur depending on backend/compute settings.
    GGML_ASSERT(t->type == GGML_TYPE_F32 || t->type == GGML_TYPE_F16 || t->type == GGML_TYPE_BF16);
    GGML_ASSERT(ggml_is_contiguous(t));

    const bool is_host = ggml_backend_buffer_is_host(t->buffer);

    const uint8_t * data;
    if (is_host) {
        data = (const uint8_t *) t->data;
    } else {
        m_host_buf.resize(ggml_nbytes(t));
        ggml_backend_tensor_get(t, m_host_buf.data(), 0, ggml_nbytes(t));
        data = m_host_buf.data();
    }

    // k_cache_in is tagged right before cpy_k(), so it still has the pre-merge shape:
    // [n_embd_head, n_head, n_tokens]
    const int64_t n_embd_head = t->ne[0];
    const int64_t n_head      = t->ne[1];
    const int64_t n_tokens    = t->ne[2];
    const int64_t n_elem      = n_embd_head*n_head*n_tokens;

    auto & sum = m_sum[il];
    if (sum.empty()) {
        sum.assign(n_embd_head*n_head, 0.0);
    }
    GGML_ASSERT(sum.size() == (size_t) (n_embd_head*n_head));

    // the raw bytes are only directly reinterpretable as float* for F32; F16/BF16 need to be
    // converted to F32 first, otherwise the mean below is computed over garbage
    const float * f;
    if (t->type == GGML_TYPE_F32) {
        f = (const float *) data;
    } else {
        m_f32_buf.resize(n_elem);
        if (t->type == GGML_TYPE_F16) {
            ggml_fp16_to_fp32_row((const ggml_fp16_t *) data, m_f32_buf.data(), n_elem);
        } else {
            ggml_bf16_to_fp32_row((const ggml_bf16_t *) data, m_f32_buf.data(), n_elem);
        }
        f = m_f32_buf.data();
    }

    if (is_post) {
        auto pre = m_probe_pre.find(il);
        if (pre == m_probe_pre.end() || pre->second.size() != (size_t) n_elem) {
            LOG_ERR("%s: centered tensor for layer %d has no shape-matched pre-centering sample\n",
                    __func__, il);
            m_probe_failed = true;
            return true;
        }

        auto & probe = m_probe_layers[il];
        const size_t n_channel = (size_t) (n_embd_head*n_head);
        if (probe.delta_sum.empty()) {
            probe.delta_sum.assign(n_channel, 0.0);
        }
        if (probe.delta_sum.size() != n_channel) {
            LOG_ERR("%s: probe channel geometry changed for layer %d\n", __func__, il);
            m_probe_failed = true;
            return true;
        }
        probe.post_buffer = t->buffer ? ggml_backend_buffer_name(t->buffer) : "none";
        for (int64_t i2 = 0; i2 < n_tokens; ++i2) {
            for (int64_t i1 = 0; i1 < n_head; ++i1) {
                for (int64_t i0 = 0; i0 < n_embd_head; ++i0) {
                    const size_t channel = (size_t) (i1*n_embd_head + i0);
                    const size_t idx = (size_t) ((i2*n_head + i1)*n_embd_head + i0);
                    const double delta = (double) pre->second[idx] - (double) f[idx];
                    probe.delta_sum[channel] += delta;
                    probe.max_abs_delta = std::max(probe.max_abs_delta, std::abs(delta));
                }
            }
        }
        probe.token_count += n_tokens;
        return true;
    }

    if (m_probe_enabled) {
        m_probe_pre[il].assign(f, f + n_elem);
        m_probe_layers[il].pre_buffer = t->buffer ? ggml_backend_buffer_name(t->buffer) : "none";
    }

    for (int64_t i2 = 0; i2 < n_tokens; ++i2) {
        for (int64_t i1 = 0; i1 < n_head; ++i1) {
            for (int64_t i0 = 0; i0 < n_embd_head; ++i0) {
                sum[i1*n_embd_head + i0] += f[(i2*n_head + i1)*n_embd_head + i0];
            }
        }
    }

    m_count[il] += n_tokens;

    return true;
}

static std::string json_escape(const std::string & value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;       break;
        }
    }
    return out;
}

bool kv_mean_collector::write_probe(
        const std::string & output_path,
        const std::string & calibration_path) const {
    if (m_probe_failed || m_probe_layers.empty()) {
        LOG_ERR("%s: tensor probe did not capture a complete pre/post pair\n", __func__);
        return false;
    }

    ggml_context * ctx_data = nullptr;
    gguf_init_params params = {
        /*.no_alloc =*/ false,
        /*.ctx      =*/ &ctx_data,
    };
    gguf_context * ctx_gguf = gguf_init_from_file(calibration_path.c_str(), params);
    if (!ctx_gguf) {
        LOG_ERR("%s: cannot read calibration %s for tensor-probe comparison\n",
                __func__, calibration_path.c_str());
        return false;
    }

    std::vector<int32_t> layer_ids;
    layer_ids.reserve(m_probe_layers.size());
    for (const auto & entry : m_probe_layers) {
        layer_ids.push_back(entry.first);
    }
    std::sort(layer_ids.begin(), layer_ids.end());

    // The calibrated layers are the ones that emit "k_cache_in", which is NOT every layer and is
    // not a dense range: on a 64-layer Qwen3.8-27B the ids are 3,7,11,...,63. Compare the probed
    // ids against the ids the calibration file actually carries, never against 0..n-1.
    const int64_t expected_layers = gguf_get_n_tensors(ctx_gguf);
    std::vector<int32_t> expected_ids;
    expected_ids.reserve((size_t) expected_layers);
    for (int64_t i = 0; i < expected_layers; ++i) {
        const char * name = gguf_get_tensor_name(ctx_gguf, i);
        int32_t il = -1;
        if (!name || sscanf(name, "kv_bar.blk.%d.k", &il) != 1 || il < 0) {
            LOG_ERR("%s: calibration tensor %" PRId64 " has an unexpected name '%s'\n",
                    __func__, i, name ? name : "(null)");
            gguf_free(ctx_gguf);
            ggml_free(ctx_data);
            return false;
        }
        expected_ids.push_back(il);
    }
    std::sort(expected_ids.begin(), expected_ids.end());

    if (layer_ids.size() != expected_ids.size()) {
        LOG_ERR("%s: tensor probe captured %zu layer(s), but calibration contains %zu\n",
                __func__, layer_ids.size(), expected_ids.size());
        gguf_free(ctx_gguf);
        ggml_free(ctx_data);
        return false;
    }
    for (size_t i = 0; i < expected_ids.size(); ++i) {
        if (layer_ids[i] != expected_ids[i]) {
            LOG_ERR("%s: tensor probe is missing calibration layer %d\n", __func__, expected_ids[i]);
            gguf_free(ctx_gguf);
            ggml_free(ctx_data);
            return false;
        }
    }

    bool ok = true;
    double global_max_error = 0.0;
    bool all_vulkan = true;
    std::ostringstream rows;
    rows << std::setprecision(10);
    for (size_t row = 0; row < layer_ids.size(); ++row) {
        const int32_t il = layer_ids[row];
        const auto & probe = m_probe_layers.at(il);
        const std::string tensor_name = "kv_bar.blk." + std::to_string(il) + ".k";
        ggml_tensor * bias = ggml_get_tensor(ctx_data, tensor_name.c_str());
        if (!bias || bias->type != GGML_TYPE_F32 ||
                (size_t) ggml_nelements(bias) != probe.delta_sum.size() || probe.token_count == 0) {
            LOG_ERR("%s: calibration tensor %s does not match captured probe geometry\n",
                    __func__, tensor_name.c_str());
            ok = false;
            break;
        }

        const float * expected = (const float *) bias->data;
        double max_error = 0.0;
        for (size_t i = 0; i < probe.delta_sum.size(); ++i) {
            const double measured = probe.delta_sum[i] / (double) probe.token_count;
            max_error = std::max(max_error, std::abs(measured - (double) expected[i]));
        }
        global_max_error = std::max(global_max_error, max_error);
        const bool layer_vulkan = probe.pre_buffer.find("Vulkan") != std::string::npos &&
                probe.post_buffer.find("Vulkan") != std::string::npos;
        all_vulkan = all_vulkan && layer_vulkan;
        if (row != 0) {
            rows << ",\n";
        }
        rows << "    {\"layer\": " << il
             << ", \"pre_buffer\": \"" << json_escape(probe.pre_buffer)
             << "\", \"post_buffer\": \"" << json_escape(probe.post_buffer)
             << "\", \"token_count\": " << probe.token_count
             << ", \"max_abs_delta\": " << probe.max_abs_delta
             << ", \"max_abs_error_vs_calibration\": " << max_error << "}";
    }

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!ok || !out) {
        LOG_ERR("%s: cannot write tensor probe to %s\n", __func__, output_path.c_str());
        gguf_free(ctx_gguf);
        ggml_free(ctx_data);
        return false;
    }
    out << std::setprecision(10)
        << "{\n"
        << "  \"schema\": \"arifilabs.kv-mean-center.tensor-probe.v1\",\n"
        << "  \"calibration\": \"" << json_escape(calibration_path) << "\",\n"
        << "  \"layer_count\": " << layer_ids.size() << ",\n"
        << "  \"all_pre_post_buffers_vulkan\": " << (all_vulkan ? "true" : "false") << ",\n"
        << "  \"global_max_abs_error_vs_calibration\": " << global_max_error << ",\n"
        << "  \"layers\": [\n" << rows.str() << "\n  ]\n}\n";

    gguf_free(ctx_gguf);
    ggml_free(ctx_data);
    return (bool) out;
}

std::vector<common_kv_mean_center_layer> kv_mean_collector::finalize() const {
    std::vector<common_kv_mean_center_layer> out;

    for (const auto & kv : m_sum) {
        const int32_t il    = kv.first;
        const auto &  sum   = kv.second;
        const int64_t count = m_count.at(il);

        if (count == 0) {
            continue;
        }

        common_kv_mean_center_layer layer;
        layer.il = il;
        layer.bias.resize(sum.size());
        for (size_t i = 0; i < sum.size(); ++i) {
            layer.bias[i] = (float) (sum[i] / (double) count);
        }

        out.push_back(std::move(layer));
    }

    std::sort(out.begin(), out.end(), [](const common_kv_mean_center_layer & a, const common_kv_mean_center_layer & b) {
        return a.il < b.il;
    });

    return out;
}

static kv_mean_collector g_collector;

static bool kv_mean_center_cb_eval(struct ggml_tensor * t, bool ask, void * user_data) {
    GGML_UNUSED(user_data);
    return g_collector.collect(t, ask);
}

static void print_usage(int, char ** argv) {
    LOG("\nexample usage:\n");
    LOG("\n    %s -m model.gguf -f calibration-data.txt -o kv-mean-center.gguf [-c 512] [--chunks N]\n", argv[0]);
    LOG("\n");
    LOG("Computes a per-layer K-cache mean-centering bias file for use with --kv-mean-center\n");
    LOG("(which requires --cache-type-k q4_0). See docs/kv-mean-center.md.\n\n");
    LOG("Tensor proof: add --kv-mean-center existing.gguf --kv-mean-center-probe-output proof.json\n\n");
}

int main(int argc, char ** argv) {
    common_params params;

    params.out_file = "kv-mean-center.gguf";
    params.n_ctx    = 512;
    params.escape   = false;

    common_init();

    if (!common_params_parse(argc, argv, params, LLAMA_EXAMPLE_KV_MEAN_CENTER, print_usage)) {
        return 1;
    }

    if (params.prompt.empty()) {
        LOG_ERR("%s: no calibration text provided (use -f FNAME)\n", __func__);
        return 1;
    }

    const bool probe_mode = !params.kv_mean_center_probe_output.empty();
    if (probe_mode && params.kv_mean_center_path.empty()) {
        LOG_ERR("%s: --kv-mean-center-probe-output requires --kv-mean-center\n", __func__);
        return 1;
    }
    if (probe_mode) {
        g_collector.enable_probe();
    }

    llama_backend_init();
    llama_numa_init(params.numa);

    // pass the callback to the backend scheduler; it fires for every node during graph
    // computation, and we pick out the ones tagged "k_cache_in-<il>"
    params.cb_eval           = kv_mean_center_cb_eval;
    params.cb_eval_user_data = nullptr;
    params.warmup            = false;

    common_init_result_ptr llama_init = common_init_from_params(params);

    llama_model   * model = llama_init->model();
    llama_context * ctx   = llama_init->context();

    if (model == nullptr || ctx == nullptr) {
        LOG_ERR("%s: failed to init\n", __func__);
        return 1;
    }

    const llama_vocab * vocab = llama_model_get_vocab(model);
    const bool add_bos = llama_vocab_get_add_bos(vocab);

    LOG_INF("%s: tokenizing the calibration text ...\n", __func__);
    std::vector<llama_token> tokens = common_tokenize(ctx, params.prompt, add_bos, params.parse_special);

    const int32_t n_ctx   = params.n_ctx;
    const int32_t n_batch = std::min(params.n_batch, n_ctx);

    if ((int32_t) tokens.size() < n_ctx) {
        LOG_ERR("%s: calibration text tokenizes to only %zu tokens, need at least n_ctx=%d\n",
                __func__, tokens.size(), n_ctx);
        return 1;
    }

    const int n_chunk_max = (int) tokens.size() / n_ctx;
    const int n_chunk = params.n_chunks < 0 ? n_chunk_max : std::min(params.n_chunks, n_chunk_max);

    LOG_INF("%s: collecting K-cache statistics over %d chunk(s) of %d tokens\n", __func__, n_chunk, n_ctx);

    llama_batch batch = llama_batch_init(n_batch, 0, 1);

    for (int i = 0; i < n_chunk; ++i) {
        const int start = i*n_ctx;

        // each chunk is scored independently, with a fresh cache
        llama_memory_clear(llama_get_memory(ctx), true);

        for (int j = 0; j < n_ctx; j += n_batch) {
            const int n_tok = std::min(n_batch, n_ctx - j);

            common_batch_clear(batch);
            for (int k = 0; k < n_tok; ++k) {
                common_batch_add(batch, tokens[start + j + k], j + k, { 0 }, false);
            }

            if (llama_decode(ctx, batch)) {
                LOG_ERR("%s: failed to decode chunk %d\n", __func__, i);
                llama_batch_free(batch);
                return 1;
            }
        }

        LOG_INF("%s: processed chunk %d / %d\n", __func__, i + 1, n_chunk);
    }

    llama_batch_free(batch);

    auto layers = g_collector.finalize();
    if (layers.empty()) {
        LOG_ERR("%s: no K-cache activity was captured; this model may not use the standard "
                "attention KV-cache path that k_cache_in is tagged on\n", __func__);
        return 1;
    }

    if (probe_mode) {
        if (!g_collector.write_probe(
                    params.kv_mean_center_probe_output, params.kv_mean_center_path)) {
            return 1;
        }
        LOG_INF("%s: wrote tensor-level Vulkan centering proof for %zu layer(s) to %s\n",
                __func__, layers.size(), params.kv_mean_center_probe_output.c_str());
        llama_backend_free();
        return 0;
    }

    std::string model_sha256;
    if (!common_kv_mean_center_model_sha256(params.model.path, model, model_sha256)) {
        LOG_ERR("%s: refusing to emit an unbound calibration artifact\n", __func__);
        return 1;
    }

    if (!common_kv_mean_center_write(
                params.out_file, layers, model_sha256, g_collector.saw_k_rot())) {
        return 1;
    }

    LOG_INF("%s: wrote K-cache mean-centering bias for %zu layer(s) to %s "
            "(model %s; measured with K rotation %s)\n",
            __func__, layers.size(), params.out_file.c_str(), model_sha256.c_str(),
            g_collector.saw_k_rot() ? "active" : "inactive");

    llama_backend_free();

    return 0;
}
