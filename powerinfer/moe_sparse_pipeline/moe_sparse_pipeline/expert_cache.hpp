#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>
#include <string>
#include <memory>
#include <functional>

#include "powerinfer-disk-queue.hpp"
#include "moe_sparse_pipeline/iou.hpp"
#include "az/core/lru.hpp"
#include "az/pipeline/task.hpp"

namespace moe_sparse_pipeline {

enum MatrixType {
    MATRIX_UP = 0,
    MATRIX_GATE = 1,
    MATRIX_DOWN = 2,
    N_MATRICES,
};

using IOCallbackFn = std::function<void()>;

enum DataStatus {
    DATA_NOT_PRESENT,
    DATA_LOADING,
    DATA_PRESENT,
};

struct Matrix {
    size_t layer_id = 0;
    size_t expert_id = 0;
    size_t matrix_id = 0;
    size_t file_offset = 0;
    az::ListNode lru_node;

    char *data = nullptr;
    std::unique_ptr<std::mutex> mutex;
    DataStatus status = DATA_NOT_PRESENT;
    std::vector<az::Arc<az::Task>> pending_tasks;

    Matrix() : mutex(std::make_unique<std::mutex>()) {}
};

struct ExpertCache {
    struct {
        std::atomic<size_t> n_examined{0};
        std::atomic<size_t> n_cached{0};
        std::atomic<size_t> n_prefetched{0};
    } stat;

    ExpertCache(
        const std::string &expert_bundle_path,
        size_t n_layers,
        size_t n_experts,
        size_t n_matrices,
        size_t matrix_bytes
    );
    ~ExpertCache();

    auto get(size_t layer_id, size_t expert_id, size_t matrix_id) -> Matrix & {
        // POWERINFER_EXPERT_HEATMAP=<path>: count accesses per (layer, expert) and dump at exit.
        //
        // This exists to answer a question BEFORE building anything: heat-driven pinning only pays
        // if expert access is SKEWED. If every expert is touched about equally, pinning a "hot" set
        // is pointless and no amount of implementation changes that -- which is a finding, not a
        // failure. The eviction policy today is a plain LRU with no notion of heat.
        //
        // Gated on a bool resolved once in the constructor, so the hot path costs one predictable
        // branch when off and one relaxed atomic increment when on.
        if (heatmap_enabled) {
            access_counts[layer_id * n_experts + expert_id].fetch_add(1, std::memory_order_relaxed);
            // Dump PERIODICALLY, not at destruction. The harness stops the server with
            // proc.terminate(), which on Windows is TerminateProcess -- no destructors, no atexit,
            // no flush. A destructor-only dump produced nothing at all on the first attempt, the
            // same way bundle generation's exit(0) swallows its own completion log line. Rewriting
            // the whole file every 65536 accesses costs one pass over ~10k rows at an interval the
            // hot path never notices, and whatever the process was killed mid-run still survives.
            if ((heatmap_total.fetch_add(1, std::memory_order_relaxed) & 0xFFFFu) == 0) {
                dump_heatmap();
            }
        }
        return layers[layer_id].experts[expert_id].matrices[matrix_id];
    }

    void lru_promote(Matrix &matrix);

    // NOTE: The caller should hold matrix's mutex
    void async_fetch(Matrix &matrix);

    void io_worker_main();

private:
    struct Expert {
        std::vector<Matrix> matrices;
    };

    struct Layer {
        std::vector<Expert> experts;
    };

    size_t n_layers = 0;
    size_t n_experts = 0;
    size_t n_matrices = 0;
    size_t matrix_bytes = 0;
    // On-disk stride: matrix_bytes rounded UP to io_alignment. ExpertBundleBuilder pads every
    // matrix it writes to io_alignment, so the file's stride is the padded size, not matrix_bytes.
    // Keeping them separate is what lets a model whose matrix is not already 4096-aligned stream at
    // all -- see the constructor.
    size_t matrix_stride = 0;
    // Per-(layer, expert) access histogram, allocated only when POWERINFER_EXPERT_HEATMAP is set.
    bool heatmap_enabled = false;
    std::string heatmap_path;
    std::vector<std::atomic<uint64_t>> access_counts;
    std::atomic<uint64_t> heatmap_total{0};
    void dump_heatmap();
    std::vector<Layer> layers;
    bool debug_print = false;

    IOUring iou;
    threadsafe_queue<Matrix *> io_queue;
    std::thread io_worker;

    az::LRU lru;

    void allocate_buffer(Matrix &matrix);
};

void set_global_expert_cache(const std::shared_ptr<ExpertCache> &cache_ptr);
bool has_global_expert_cache();
auto get_global_expert_cache() -> ExpertCache &;

}
