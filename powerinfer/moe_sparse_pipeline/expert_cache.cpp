#include <fcntl.h>
#include <cstdio>
#include <cstdlib>

#include "powerinfer-perf.hpp"
#include "powerinfer-log.hpp"
#include "moe_sparse_pipeline/config.hpp"
#include "moe_sparse_pipeline/expert_cache.hpp"
#include "az/core/aligned_alloc.hpp"  // lane-110 Windows port: vendor's own portable wrapper (bare C11 aligned_alloc absent on UCRT)
#include "az/core/perfetto_trace.hpp"

namespace moe_sparse_pipeline {

ExpertCache::ExpertCache(
    const std::string &expert_bundle_path,
    size_t n_layers,
    size_t n_experts,
    size_t n_matrices,
    size_t matrix_bytes) :
    iou(expert_bundle_path, io_queue_depth),
    io_queue(io_queue_depth),
    n_layers(n_layers),
    n_experts(n_experts),
    n_matrices(n_matrices),
    matrix_bytes(matrix_bytes),
    // ExpertBundleBuilder pads EVERY matrix it writes up to io_alignment (expert_bundle.cpp:13-15),
    // so the file's stride is the padded size. This used to assert `matrix_bytes % io_alignment == 0`
    // and stride by matrix_bytes, which is only correct when the padding happens to be zero --
    // and llama-model.cpp gated bundle loading on exactly that condition to avoid tripping the
    // assert. The effect was that any MoE model whose expert matrix is not already 4096-aligned
    // could never stream, including this estate's own production brain: gemma4 26B-A4B has
    // n_embd 2816 and n_ff_exp 704, so its matrix is 1,584 * 704 = 1,115,136 B and 1,115,136 % 4096
    // = 1024. The file format already handled it; only the reader did not.
    matrix_stride((matrix_bytes + io_alignment - 1) / io_alignment * io_alignment)
{
    // The STRIDE is what must be aligned -- reads are issued at these offsets and, under
    // FILE_FLAG_NO_BUFFERING, both the offset and the length must be sector multiples.
    POWERINFER_ASSERT(matrix_stride % io_alignment == 0);
    POWERINFER_ASSERT(matrix_stride >= matrix_bytes);

    if (const char * hm = std::getenv("POWERINFER_EXPERT_HEATMAP")) {
        heatmap_enabled = true;
        heatmap_path = hm;
        // std::atomic is not copyable, so the vector is built by resize (value-initialised to 0)
        // rather than by a fill constructor.
        access_counts = std::vector<std::atomic<uint64_t>>(n_layers * n_experts);
    }

    layers.resize(n_layers);
    for (size_t layer_id = 0; layer_id < n_layers; layer_id++) {
        auto &layer = layers[layer_id];
        layer.experts.resize(n_experts);

        for (size_t expert_id = 0; expert_id < n_experts; expert_id++) {
            auto &expert = layer.experts[expert_id];
            expert.matrices.resize(n_matrices);

            for (size_t matrix_id = 0; matrix_id < n_matrices; matrix_id++) {
                auto &matrix = expert.matrices[matrix_id];
                matrix.layer_id = layer_id;
                matrix.expert_id = expert_id;
                matrix.matrix_id = matrix_id;
                matrix.file_offset = matrix_stride * (matrix_id + expert_id * n_matrices + layer_id * n_experts * n_matrices);
            }
        }
    }

    io_worker = std::thread(&ExpertCache::io_worker_main, this);
}

static Matrix *const io_worker_exit_signal = reinterpret_cast<Matrix *>(-1llu);

ExpertCache::~ExpertCache() {
    io_queue.push(io_worker_exit_signal);
    io_worker.join();

    while (lru.size > 0) {
        Matrix *victim = lru.evict().get_owner_ptr(&Matrix::lru_node);
        az::aligned_free(victim->data);  // lane-110: must pair with az::aligned_alloc below (_aligned_malloc on Windows)
        victim->data = nullptr;
    }

    dump_heatmap();  // best-effort: only reached on a clean shutdown, which terminate() is not

    if(debug_print){
        printf(
            "Expert cache: #examined=%zu, #cached=%zu (%.2lf%%)\n",
            stat.n_examined.load(),
            stat.n_cached.load(),
            100.0 * stat.n_cached / stat.n_examined
        );

        printf(
            "Expert cache: #examined=%zu, #prefetched=%zu (%.2lf%%)\n",
            stat.n_examined.load(),
            stat.n_prefetched.load(),
            100.0 * stat.n_prefetched / stat.n_examined
        );
    }
}

// One row per (layer, expert). The question this answers, BEFORE anything is built: is expert
// access SKEWED enough that pinning a hot set would beat the plain LRU we have? A flat histogram
// means it would not, and that closes the row without writing an eviction policy nobody can justify.
// Rewritten in place on every call so the newest complete snapshot always survives a hard kill.
void ExpertCache::dump_heatmap() {
    if (!heatmap_enabled || access_counts.empty()) {
        return;
    }
    FILE * f = fopen(heatmap_path.c_str(), "w");
    if (!f) {
        return;
    }
    fprintf(f, "layer,expert,accesses\n");
    for (size_t l = 0; l < n_layers; l++) {
        for (size_t e = 0; e < n_experts; e++) {
            fprintf(f, "%zu,%zu,%llu\n", l, e,
                    (unsigned long long) access_counts[l * n_experts + e].load(std::memory_order_relaxed));
        }
    }
    fclose(f);
}

void ExpertCache::lru_promote(Matrix &matrix) {
    lru.promote(matrix.lru_node);
}

void ExpertCache::async_fetch(Matrix &matrix) {
    az::TraceEvent _("launch_io");

    POWERINFER_ASSERT(matrix.status == DATA_NOT_PRESENT);
    matrix.status = DATA_LOADING;
    allocate_buffer(matrix);
    io_queue.push(&matrix);
}

void ExpertCache::io_worker_main() {
#if defined(__linux__)
    if (io_worker_cpu_affinity != -1) {
        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(io_worker_cpu_affinity, &cpus);
        int ret = sched_setaffinity(0, sizeof(cpus), &cpus);
        POWERINFER_ASSERT(ret >= 0);
    }
#endif

    // NOTE: Do not manipulate LRU in IO worker

#if !defined(__linux__)
    // Windows IOCP path (POWERINFER_IOCP default ON): fill the bounded request
    // ring before waiting.  The completion callback remains the sole transition
    // from DATA_LOADING to DATA_PRESENT, preserving the cache/task dependency
    // semantics.  POWERINFER_IOCP=0 skips this and falls through to the shared
    // synchronous loop below (pre-M2b behavior; submit_and_wait is a no-op).
    if (iocp_enabled()) {
    bool stopping = false;
    while (!stopping || iou.n_inflight > 0) {
        if (iou.n_inflight >= io_queue_depth) {
            powerinfer_begin_event("iou.submit_and_wait");
            iou.submit_and_wait(1);
            powerinfer_end_event();
            iou.reap();
            continue;
        }

        powerinfer_begin_event("io_queue.pop");
        Matrix *matrix = nullptr;
        auto v = io_queue.pop(!stopping && iou.n_inflight == 0);
        if (v.has_value()) {
            matrix = v.value();
        }
        powerinfer_end_event();

        if (matrix == io_worker_exit_signal) {
            // Destructor appends this after producers have stopped.  Do not
            // abandon requests already accepted by the completion port.
            POWERINFER_ASSERT(io_queue.empty());
            stopping = true;
        } else if (matrix) {
            POWERINFER_ASSERT(matrix->data);
            iou.enqueue_read(matrix->data, matrix->file_offset, matrix_stride, matrix, [](void *user_data) {
                Matrix *matrix = static_cast<Matrix *>(user_data);

                std::unique_lock lock(*matrix->mutex);
                matrix->status = DATA_PRESENT;
                for (auto &task : matrix->pending_tasks) {
                    task->on_prev_task_finished();
                }
                matrix->pending_tasks.clear();
            });
        }

        // An empty producer queue, or shutdown, is the point at which we
        // require forward progress.  Otherwise keep accepting independent
        // reads so storage latency overlaps.
        if (iou.n_inflight > 0 && (stopping || matrix == nullptr)) {
            powerinfer_begin_event("iou.submit_and_wait");
            iou.submit_and_wait(1);
            powerinfer_end_event();
        }

        iou.reap();
    }
    return;
    }
#endif
    while (true) {
        powerinfer_begin_event("io_queue.pop");
        Matrix *matrix = nullptr;
        auto v = io_queue.pop(iou.n_inflight <= 0);
        if (v.has_value()) {
            matrix = v.value();
        }
        powerinfer_end_event();

        if (matrix == io_worker_exit_signal) {
            break;
        }

        bool any_submit = false;
        if (matrix) {
            POWERINFER_ASSERT(matrix->data);
            iou.enqueue_read(matrix->data, matrix->file_offset, matrix_stride, matrix, [](void *user_data) {
                Matrix *matrix = static_cast<Matrix *>(user_data);

                std::unique_lock lock(*matrix->mutex);
                matrix->status = DATA_PRESENT;
                for (auto &task : matrix->pending_tasks) {
                    task->on_prev_task_finished();
                }
                matrix->pending_tasks.clear();
            });

            any_submit = true;
        }

        // If no IO request submitted, we wait for previous requests to complete
        if (!any_submit) {
            powerinfer_begin_event("iou.submit_and_wait");
            size_t wait_nr = iou.n_inflight > 0 ? 1 : 0;
            iou.submit_and_wait(wait_nr);
            powerinfer_end_event();
        }

        iou.reap();
    }
}

void ExpertCache::allocate_buffer(Matrix &matrix) {
    POWERINFER_ASSERT(!matrix.data);

    if (lru.size < max_n_cached_matrices) {
        // matrix_stride, not matrix_bytes: the read below transfers a full padded stride so the
        // length stays sector-aligned for FILE_FLAG_NO_BUFFERING. Only the first matrix_bytes are
        // weights; the tail is the writer's padding and is never read by the kernels.
        matrix.data = static_cast<char *>(az::aligned_alloc(io_alignment, matrix_stride));  // lane-110: portable wrapper
    } else {
        Matrix *victim = lru.evict().get_owner_ptr(&Matrix::lru_node);

        std::unique_lock lock(*victim->mutex);
        matrix.data = victim->data;
        victim->status = DATA_NOT_PRESENT;
        victim->data = nullptr;
    }

    lru.add(matrix.lru_node);
}

static std::shared_ptr<ExpertCache> cache_ptr{nullptr};

void set_global_expert_cache(const std::shared_ptr<ExpertCache> &cache_ptr) {
    ::moe_sparse_pipeline::cache_ptr = cache_ptr;
}

bool has_global_expert_cache() {
    return ::moe_sparse_pipeline::cache_ptr != nullptr;
}

auto get_global_expert_cache() -> ExpertCache & {
    return *::moe_sparse_pipeline::cache_ptr;
}

}

extern "C" {

void powerinfer_init_global_expert_cache(
    const char *expert_bundle_path,
    size_t n_layers,
    size_t n_experts,
    size_t n_matrices,
    size_t matrix_bytes
) {
    moe_sparse_pipeline::set_global_expert_cache(std::make_shared<moe_sparse_pipeline::ExpertCache>(
        expert_bundle_path,
        n_layers,
        n_experts,
        n_matrices,
        matrix_bytes
    ));
}

bool powerinfer_has_global_expert_cache(void) {
    return moe_sparse_pipeline::has_global_expert_cache();
}

}
