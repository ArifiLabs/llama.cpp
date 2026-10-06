// Bounded real-provider seam test. Synthetic weights, direct file reads,
// duplicate IDs with different activations, full-pool eviction, both v1/v2.
#include "llama-mmap.h"
#include "ggml-backend-moe-cache.h"
#include "ggml-quants.h"
#include "ggml.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <stdexcept>
#include <vector>

struct batch_reader {
    llama_file file;
    int reads = 0;
    bool fail = false;
    explicit batch_reader(const char * path) : file(path, "rb", true) {
        if (!file.has_direct_io()) { throw std::runtime_error("direct IO required"); }
    }
};
static int batch_read(void * opaque, uint64_t offset, void * out, size_t bytes, uint64_t * physical) {
    auto & r = *static_cast<batch_reader *>(opaque);
    ++r.reads;
    if (r.fail) { return 0; }
    const auto before = r.file.physical_read_bytes();
    r.file.seek(offset, SEEK_SET);
    r.file.read_raw(out, bytes);
    if (physical) { *physical = r.file.physical_read_bytes() - before; }
    return 1;
}
static void batch_free(void * opaque) { delete static_cast<batch_reader *>(opaque); }
static void require(bool ok, const char * message) { if (!ok) { throw std::runtime_error(message); } }

// --kquant: the cache matvec on the K-quant types GLM/Ornith stream, against a
// CPU dequant reference. Relative L2 error per output row-vector < 2%.
static int kquant_check(const char * prefix) {
    const ggml_type types[] = {GGML_TYPE_Q2_K, GGML_TYPE_Q3_K, GGML_TYPE_Q4_K, GGML_TYPE_Q6_K};
    // experts >= 64: a pool needs moe_cache_pool_slots_min slots and is capped at the tensor's entries.
    constexpr int n_in = 2048, n_out = 256, experts = 64;
    struct item { ggml_type type; std::string name, path; std::vector<unsigned char> w; size_t eb; llama_file * src; llama_mmap * map; };
    std::vector<item> items;
    std::vector<float> f(size_t(n_in) * n_out);
    for (ggml_type type : types) {
        item it{type, "blk." + std::to_string(items.size()) + ".ffn_up_exps.weight", std::string(prefix) + "." + ggml_type_name(type), {}, 0, nullptr, nullptr};
        it.eb = ggml_row_size(type, n_in) * n_out;
        it.w.resize(it.eb * experts);
        for (int e = 0; e < experts; ++e) {
            for (size_t i = 0; i < f.size(); ++i) { f[i] = std::sin(0.013f * float(i) + 0.7f * float(e)) * (1.0f + 0.1f * float(e % 5)); }
            ggml_quantize_chunk(type, f.data(), it.w.data() + e * it.eb, 0, n_out, n_in, nullptr);
        }
        { llama_file out(it.path.c_str(), "wb"); out.write_raw(it.w.data(), it.w.size()); }
        it.src = new llama_file(it.path.c_str(), "rb", true);
        it.map = new llama_mmap(it.src, 0);
        items.push_back(std::move(it));
    }
    for (auto & it : items) {
        ggml_moe_cache_tensor_desc desc = {it.name.c_str(), it.map->addr(), it.eb, n_in, n_out, experts, int32_t(it.type)};
        auto * reader = new batch_reader(it.path.c_str());
        if (!ggml_moe_disk_register(&desc, reader, batch_read, batch_free)) { delete reader; throw std::runtime_error("kquant register"); }
    }
    ggml_backend_load_all();
    auto device = ggml_backend_dev_by_name("Vulkan0");
    require(device != nullptr, "Vulkan0 absent");
    auto backend = ggml_backend_dev_init(device, nullptr);
    auto api = ggml_moe_cache_get(ggml_backend_dev_backend_reg(device));
    ggml_moe_cache_config config = {};
    // Census reserves 64 slots per shape: 4 shapes need 71.8 MB, so 256 MiB.
    require(api.query_config(0, 256, &config), "config");
    config.min_expert_bytes = 1; config.min_expert_explicit = 1; config.reserve_bytes = 0; config.minimum_slab_bytes = 0;
    void * backends[] = {backend};
    void * session = api.session_create(backends, 1, &config);
    require(session != nullptr, "session");
    api.session_enter(session);
    int failures = 0;
    for (auto & it : items) {
        const auto * traits = ggml_get_type_traits(it.type);
        for (const std::vector<int32_t> & ids : {std::vector<int32_t>{3, 0, 15, 7, 9, 1, 12, 5}, std::vector<int32_t>{3, 4, 3, 8, 15, 4, 0, 2}}) {
            const int count = int(ids.size());
            const int tokens = ids == std::vector<int32_t>{3, 0, 15, 7, 9, 1, 12, 5} ? 1 : 2;
            void * node = api.begin(it.name.c_str(), it.map->addr(), it.eb, n_in, n_out, it.type, experts, tokens, count);
            require(node != nullptr, "kquant begin");
            int32_t slots[64];
            require(api.plan(node, ids.data(), count, slots) == count, "kquant row unserved");
            std::vector<float> acts(size_t(count) * n_in), output(size_t(count) * n_out);
            const float * act_rows[64]; float * out_rows[64];
            for (int i = 0; i < count; ++i) {
                act_rows[i] = acts.data() + size_t(i) * n_in; out_rows[i] = output.data() + size_t(i) * n_out;
                for (int j = 0; j < n_in; ++j) { acts[size_t(i) * n_in + j] = std::cos(0.021f * float(j) + float(i)); }
            }
            require(api.dispatch(node, it.type, n_in, n_out, count, slots, act_rows), "kquant dispatch");
            require(api.collect(node, count, out_rows, n_out), "kquant collect");
            api.end(node);
            double worst = 0;
            std::vector<float> row(n_in);
            for (int i = 0; i < count; ++i) {
                double num = 0, den = 0;
                for (int r = 0; r < n_out; ++r) {
                    traits->to_float(it.w.data() + ids[i] * it.eb + r * ggml_row_size(it.type, n_in), row.data(), n_in);
                    double ref = 0;
                    for (int j = 0; j < n_in; ++j) { ref += double(row[j]) * act_rows[i][j]; }
                    num += (out_rows[i][r] - ref) * (out_rows[i][r] - ref); den += ref * ref;
                }
                worst = std::max(worst, std::sqrt(num / std::max(den, 1e-30)));
            }
            const bool ok = std::isfinite(worst) && worst < 0.02;
            failures += !ok;
            printf("NVME_KQUANT %s type=%s tokens=%d rows=%d worst_rel_l2=%.6f\n", ok ? "PASS" : "FAIL", ggml_type_name(it.type), tokens, count, worst);
        }
    }
    api.session_leave(session); api.session_destroy(session);
    for (auto & it : items) { ggml_moe_disk_unregister_range(it.map->addr(), it.w.size()); }
    ggml_backend_free(backend);
    printf("NVME_KQUANT_SUMMARY %s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}

int main(int argc, char ** argv) try {
    if (argc == 3 && strcmp(argv[2], "--kquant") == 0) { return kquant_check(argv[1]); }
    const bool capacity_control = argc == 3 && strcmp(argv[2], "--capacity-refusal") == 0;
    if (argc != 2 && !capacity_control) { return 2; }
    constexpr int n_in = 512, n_out = 64, experts = 128;
    const size_t expert_bytes = ggml_row_size(GGML_TYPE_Q8_0, n_in) * n_out;
    std::vector<unsigned char> weights(expert_bytes * experts);
    std::vector<float> values(n_in);
    for (int e = 0; e < experts; ++e) {
        for (int j = 0; j < n_in; ++j) { values[j] = ((e % 17) + 1) * 0.01f; }
        for (int row = 0; row < n_out; ++row) {
            quantize_row_q8_0_ref(values.data(), reinterpret_cast<block_q8_0 *>(weights.data() + e * expert_bytes + row * ggml_row_size(GGML_TYPE_Q8_0, n_in)), n_in);
        }
    }
    { llama_file file(argv[1], "wb"); file.write_raw(weights.data(), weights.size()); }
    llama_file source(argv[1], "rb", true);
    llama_mmap identity(&source, 0);
    ggml_moe_cache_tensor_desc desc = {"blk.0.ffn_up_exps.weight", identity.addr(), expert_bytes, n_in, n_out, experts, GGML_TYPE_Q8_0};
    auto * reader = new batch_reader(argv[1]);
    if (!ggml_moe_disk_register(&desc, reader, batch_read, batch_free)) { delete reader; throw std::runtime_error("register"); }
    unsigned char null_counter_bytes[17];
    require(ggml_moe_disk_read(desc.data, 13, null_counter_bytes, sizeof(null_counter_bytes), nullptr) &&
            memcmp(null_counter_bytes, weights.data() + 13, sizeof(null_counter_bytes)) == 0, "null-counter read");
    puts("NVME_NULL_COUNTER PASS bytes=17 offset=13");
    ggml_backend_load_all();
    auto device = ggml_backend_dev_by_name("Vulkan0");
    require(device != nullptr, "Vulkan0 absent");
    auto backend = ggml_backend_dev_init(device, nullptr);
    require(backend != nullptr, "backend init");
    auto api = ggml_moe_cache_get(ggml_backend_dev_backend_reg(device));
    require(api.query_config && api.session_create && api.begin && api.plan && api.dispatch && api.collect && api.end, "provider API absent");
    ggml_moe_cache_config config = {};
    require(api.query_config(0, capacity_control ? 1 : 3, &config), "config");
    config.min_expert_bytes = 1;
    config.min_expert_explicit = 1;
    config.reserve_bytes = 0;
    config.minimum_slab_bytes = 0;
    void * backends[] = {backend};
    void * session = api.session_create(backends, 1, &config);
    require(session != nullptr, "session");
    api.session_enter(session);
    if (capacity_control) {
        void * node = api.begin(desc.name, desc.data, expert_bytes, n_in, n_out,
                GGML_TYPE_Q8_0, experts, 1, 8);
        if (node) { api.end(node); }
        require(node == nullptr, "undersized disk cache accepted");
        require(reader->reads == 1, "capacity refusal performed expert reads");
        api.session_leave(session); api.session_destroy(session);
        ggml_moe_disk_unregister_range(desc.data, weights.size());
        ggml_backend_free(backend);
        puts("NVME_CAPACITY_CONTROL PASS budget=1MiB required=2176KiB no-expert-read");
        return 0;
    }
    auto run = [&](const std::vector<int32_t> & ids, int expected_reads, bool fail = false) {
        const int count = int(ids.size());
        const int tokens = count % 8 == 0 ? count / 8 : (count % 2 == 0 ? 2 : 1);
        void * node = api.begin(desc.name, desc.data, expert_bytes, n_in, n_out, GGML_TYPE_Q8_0, experts, tokens, count);
        require(node != nullptr, "begin");
        int32_t slots[64];
        const int before = reader->reads;
        reader->fail = fail;
        const int served = api.plan(node, ids.data(), count, slots);
        if (fail) {
            api.end(node);
            reader->fail = false;
            require(served == 0 && slots[0] == -1, "read failure falsely served");
            return;
        }
        require(served == count, "row unserved");
        require(reader->reads - before == expected_reads, "unique-fill read count");
        std::vector<float> acts(count * n_in), output(count * n_out);
        const float * act_rows[64]; float * out_rows[64];
        for (int i = 0; i < count; ++i) {
            act_rows[i] = acts.data() + i * n_in; out_rows[i] = output.data() + i * n_out;
            for (int j = 0; j < n_in; ++j) { acts[i * n_in + j] = (i % 2) + 1; }
        }
        require(api.dispatch(node, GGML_TYPE_Q8_0, n_in, n_out, count, slots, act_rows), "dispatch");
        require(api.collect(node, count, out_rows, n_out), "collect");
        api.end(node);
        for (int i = 0; i < count; ++i) {
            dequantize_row_q8_0(reinterpret_cast<const block_q8_0 *>(weights.data() + ids[i] * expert_bytes), values.data(), n_in);
            const double ref = std::accumulate(values.begin(), values.end(), 0.0) * ((i % 2) + 1);
            for (int row = 0; row < n_out; ++row) {
                require(std::isfinite(out_rows[i][row]) && std::abs(out_rows[i][row] - ref) <= 0.002 * std::abs(ref) + 0.001, "numeric mismatch");
            }
        }
    };
    // 3 MiB / 34816 bytes = 90 slots: fill the entire pool, then demand a
    // missing expert BEFORE a still-needed old LRU hit. The hit must survive.
    std::vector<int32_t> first(64), second(26);
    std::iota(first.begin(), first.end(), 0); std::iota(second.begin(), second.end(), 64);
    const bool hot = getenv("GGML_ARIFI_MOE_NVME_HOT_MODE") && strcmp(getenv("GGML_ARIFI_MOE_NVME_HOT_MODE"), "1") == 0;
    run(first, 1); run(second, 1);
    run({90, 0, 90, 0}, hot ? 0 : 1);
    run({90, 0, 90, 0}, 0);
    run({91}, 0, true);
    run({91, 91}, 1);
    api.session_leave(session); api.session_destroy(session);
    ggml_moe_disk_unregister_range(desc.data, weights.size());
    ggml_backend_free(backend);
    puts("NVME_BATCH_PROVIDER PASS unique-fill duplicate-activation protected-hit repeated-hit read-failure-recovery");
    return 0;
} catch (const std::exception & error) {
    fprintf(stderr, "NVME_BATCH_PROVIDER FAIL: %s\n", error.what());
    return 1;
}
