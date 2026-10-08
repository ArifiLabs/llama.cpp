// Per-type check of the Vulkan MoE expert cache matvec (moe_cache_mv.comp):
// for every weight type the cache accepts, a cache-hit MUL_MAT_ID must match
// the CPU result. A type that never gets a hit is a FAIL, not a skip.
#include "ggml.h"
#include "ggml-backend.h"
#include "../ggml/src/ggml-backend-moe-cache.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int64_t n_in     = 256;
constexpr int64_t n_out    = 128;
constexpr int64_t n_expert = 8;
constexpr int64_t n_used   = 2;
constexpr int     max_steps = 64;
constexpr int     steps_after_hit = 4;
constexpr double  max_nmse = 5e-4;

std::mutex log_mutex;
std::string log_text;

void log_callback(enum ggml_log_level, const char * text, void *) {
    std::lock_guard<std::mutex> lock(log_mutex);
    log_text += text;
}

std::string log_get() {
    std::lock_guard<std::mutex> lock(log_mutex);
    return log_text;
}

void log_clear() {
    std::lock_guard<std::mutex> lock(log_mutex);
    log_text.clear();
}

void set_env(const char * name, const char * value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value) { setenv(name, value, 1); } else { unsetenv(name); }
#endif
}

long long max_field_value(const std::string & text, const char * field) {
    long long result = -1;
    size_t position = 0;
    while ((position = text.find(field, position)) != std::string::npos) {
        position += strlen(field);
        char * end = nullptr;
        const long long value = strtoll(text.c_str() + position, &end, 10);
        if (end != text.c_str() + position) {
            result = std::max(result, value);
        }
    }
    return result;
}

double nmse(const std::vector<float> & ref, const std::vector<float> & out) {
    double err = 0.0, norm = 0.0;
    for (size_t i = 0; i < ref.size(); i++) {
        if (!std::isfinite(out[i])) {
            return INFINITY;
        }
        const double d = (double) out[i] - ref[i];
        err += d * d;
        norm += (double) ref[i] * ref[i];
    }
    return err / std::max(norm, 1e-12);
}

void configure_cache() {
    set_env("GGML_CUDA_MOE_CACHE", "1");
    set_env("GGML_CUDA_MOE_CACHE_MODE", "on");
    set_env("GGML_CUDA_MOE_CACHE_BUDGET_MB", "64");
    set_env("GGML_CUDA_MOE_CACHE_RESERVE_MB", "0");
    set_env("GGML_CUDA_MOE_CACHE_MIN_EXPERT_KB", "1");
    set_env("GGML_CUDA_MOE_CACHE_MAX_BATCH", "1");
    set_env("GGML_CUDA_MOE_CACHE_INSERTS", "4");
    set_env("GGML_CUDA_MOE_CACHE_ADMIT_AFTER", "1");
    set_env("GGML_CUDA_MOE_CACHE_THROTTLE", "1");
    set_env("GGML_CUDA_MOE_CACHE_QUEUE", "16");
    set_env("GGML_CUDA_MOE_CACHE_STATS", "1");
    set_env("GGML_CUDA_MOE_CACHE_MIN_CC", "0");
    set_env("GGML_CUDA_MOE_CACHE_OVERLAP_CPU_ROWS", "0");
    set_env("GGML_CUDA_MOE_CACHE_FAIL", nullptr);
}

ggml_backend_dev_t find_device(const char * reg_name, bool gpu) {
    for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
        ggml_backend_dev_t dev = ggml_backend_dev_get(i);
        const enum ggml_backend_dev_type type = ggml_backend_dev_type(dev);
        const bool is_gpu = type == GGML_BACKEND_DEVICE_TYPE_GPU || type == GGML_BACKEND_DEVICE_TYPE_IGPU;
        if (gpu ? (is_gpu && strcmp(ggml_backend_reg_name(ggml_backend_dev_backend_reg(dev)), reg_name) == 0)
                : type == GGML_BACKEND_DEVICE_TYPE_CPU) {
            return dev;
        }
    }
    return nullptr;
}

// Returns true on PASS; prints one receipt line per type.
bool run_type(enum ggml_type wtype, ggml_backend_t vk, ggml_backend_t cpu) {
    const char * name = ggml_type_name(wtype);
    ggml_init_params params = { 16 * ggml_tensor_overhead() + ggml_graph_overhead(), nullptr, true };
    ggml_context * ctx = ggml_init(params);
    ggml_tensor * weights = ggml_new_tensor_3d(ctx, wtype, n_in, n_out, n_expert);
    ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, n_used, 1);
    ggml_tensor * act = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n_in, 1, 1);
    ggml_set_name(weights, "blk.0.ffn_up_exps.weight");
    ggml_tensor * out = ggml_mul_mat_id(ctx, weights, act, ids);
    ggml_set_name(out, "moe_cache_vk_types_out");
    ggml_cgraph * graph = ggml_new_graph(ctx);
    ggml_build_forward_expand(graph, out);
    ggml_backend_buffer_t buffer = ggml_backend_alloc_ctx_tensors(ctx, cpu);
    ggml_backend_buffer_set_usage(buffer, GGML_BACKEND_BUFFER_USAGE_WEIGHTS);

    std::mt19937 rng(1234u + (unsigned) wtype);
    std::normal_distribution<float> dist(0.0f, 0.1f);
    std::vector<float> weights_f32(ggml_nelements(weights));
    for (float & v : weights_f32) { v = dist(rng); }
    std::vector<float> imatrix(n_in, 1.0f);
    std::vector<uint8_t> weights_q(ggml_nbytes(weights));
    ggml_quantize_chunk(wtype, weights_f32.data(), weights_q.data(), 0, n_out * n_expert, n_in, imatrix.data());
    ggml_backend_tensor_set(weights, weights_q.data(), 0, weights_q.size());

    const int32_t ids_data[n_used] = { 3, 5 };
    ggml_backend_tensor_set(ids, ids_data, 0, sizeof(ids_data));
    std::vector<float> act_data(n_in);
    for (float & v : act_data) { v = dist(rng) * 5.0f; }
    ggml_backend_tensor_set(act, act_data.data(), 0, act_data.size() * sizeof(float));

    std::vector<float> reference(ggml_nelements(out));
    std::vector<float> actual(reference.size());
    bool ok = ggml_backend_graph_compute(cpu, graph) == GGML_STATUS_SUCCESS;
    ggml_backend_tensor_get(out, reference.data(), 0, reference.size() * sizeof(float));

    configure_cache();
    log_clear();
    ggml_backend_t backends[] = { vk, cpu };
    ggml_backend_sched_t sched = ggml_backend_sched_new(backends, nullptr, 2, GGML_DEFAULT_GRAPH_SIZE, false, false);
    ggml_backend_sched_set_tensor_backend(sched, out, cpu);
    ok = ok && ggml_backend_sched_alloc_graph(sched, graph) && ggml_backend_sched_get_tensor_backend(sched, out) == cpu;

    double worst = 0.0;
    int hit_step = -1;
    for (int step = 0; ok && step < max_steps; step++) {
        ok = ggml_backend_sched_graph_compute(sched, graph) == GGML_STATUS_SUCCESS;
        ggml_backend_tensor_get(out, actual.data(), 0, actual.size() * sizeof(float));
        // Only steps at or after the first observed hit measure the cache matvec.
        if (hit_step >= 0) {
            worst = std::max(worst, nmse(reference, actual));
            if (step - hit_step >= steps_after_hit) {
                break;
            }
        } else if (max_field_value(log_get(), "hits=") > 0) {
            hit_step = step;
            worst = nmse(reference, actual);
        }
    }
    ggml_backend_sched_free(sched);

    const std::string log = log_get();
    const long long hits = max_field_value(log, "hits=");
    const bool clean = max_field_value(log, "dispatch-fail=") <= 0 && max_field_value(log, "collect-fail=") <= 0;
    const bool pass = ok && hits > 0 && clean && worst <= max_nmse;
    printf("moe-cache-vk-type %-8s nmse=%.3e hits=%lld %s%s\n", name, worst, hits,
           pass ? "PASS" : "FAIL", hits > 0 ? "" : " (no cache hit)");
    if (!pass) {
        fprintf(stderr, "--- %s cache log ---\n%s", name, log.c_str());
    }

    ggml_backend_buffer_free(buffer);
    ggml_free(ctx);
    return pass;
}

} // namespace

int main() {
    ggml_log_set(log_callback, nullptr);
    ggml_backend_load_all();
    ggml_backend_dev_t vk_dev = find_device("Vulkan", true);
    ggml_backend_dev_t cpu_dev = find_device(nullptr, false);
    if (!vk_dev || !cpu_dev) {
        printf("SKIP: Vulkan or CPU backend unavailable\n");
        return 77;
    }
    ggml_backend_t vk = ggml_backend_dev_init(vk_dev, nullptr);
    ggml_backend_t cpu = ggml_backend_dev_init(cpu_dev, nullptr);
    if (!vk || !cpu) {
        fprintf(stderr, "failed to initialize Vulkan and CPU backends\n");
        return 1;
    }

    int failed = 0;
    for (int i = 0; i < ggml_moe_cache_wtype_count(); i++) {
        failed += run_type(ggml_moe_cache_types[i], vk, cpu) ? 0 : 1;
    }
    printf("moe-cache-vk-types: %d of %d types FAIL\n", failed, ggml_moe_cache_wtype_count());

    ggml_backend_free(vk);
    ggml_backend_free(cpu);
    return failed ? 1 : 0;
}
