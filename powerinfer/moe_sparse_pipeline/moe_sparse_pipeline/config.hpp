#pragma once

#include <cstddef>
#include <string>
#include <cstdio>

namespace moe_sparse_pipeline {

static constexpr size_t max_batch_size = 8;
static constexpr size_t max_n_experts = 128;
static constexpr size_t io_queue_depth = 512;
static constexpr size_t io_alignment = 4096;


template <typename T>
T getenv_(const std::string &name, const T &default_value) {
    auto env = getenv(name.c_str());
    if (env) {
        if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(atoll(env));
        } else if constexpr (std::is_floating_point_v<T>) {
            return atof(env);
        } else {
            return std::string(env);
        }
    } else {
        return default_value;
    }
}



/*
    max_n_cached_matrices determines the maximum number of matrices that can be stored in system memory at the same time. 
    You can adjust the cache size according to memory limitations.

    For SmallThinker 21B:  52 moe layer * 64 experts * 3 matrices (up, gate, down) per expert (PS: One Q4_0 matrices sized 768 * 2560, is about 1.1MB)
    For 8GB mem: 3 * 64 * 32 is recommended

    For SmallThinker 4B:  32 moe layer * 32 experts * 3 matrices (up, gate, down) per expert (PS: One Q4_0 matrices sized 768 * 3584, is about 1.5MB)
    For 1GB mem: 3 * 32 * 8 is recommended
*/
/*
    The cache must be STRICTLY larger than the IO ring, and that is a correctness bound rather than
    a tuning preference.  `ExpertCache::allocate_buffer` evicts the LRU tail and takes its buffer
    WITHOUT checking whether the victim is still `DATA_LOADING`.  What makes that safe is the
    ordering, not a check: a matrix is added at the HEAD the moment its read is queued, and at most
    `io_queue_depth` reads are ever in flight, so the tail cannot be one of them — PROVIDED the
    cache holds more than `io_queue_depth` entries.  At exactly `io_queue_depth` the argument
    collapses: every entry can be in flight, and the tail's buffer is then a live `ReadFile`
    destination that gets handed to a different matrix.  Silent wrong weights, not a crash.

    The bound used to be enforced inside `getenv_` as `atoll(env) < io_queue_depth`, which had two
    defects, both reproduced before this was changed:
      MAX_N_CACHED=512 -> ACCEPTED, while the message said "must > 512"       (off by one)
      MAX_N_CACHED=-1  -> ACCEPTED, stored as 18446744073709551615            (signed vs unsigned)
    The second is the dangerous one: comparing `long long` against an unsigned `size_t` converts the
    signed side to unsigned, so every negative value passes the guard and yields an effectively
    unbounded cache that never evicts and grows until the box is out of memory.

    It also lived in a GENERIC template while being a bound on ONE option, so the next integral
    caller of `getenv_` would have silently inherited both the wrong limit and a message naming a
    variable it has nothing to do with.  The check now sits with the thing it bounds.
*/
static size_t checked_max_n_cached() {
    const long long v = getenv_<long long>("MAX_N_CACHED", 3 * 64 * 32);
    if (v <= static_cast<long long>(io_queue_depth)) {
        fprintf(stderr, "Error: MAX_N_CACHED must be > %zu (got %lld) — the expert cache must be "
                        "strictly larger than the IO queue depth or an in-flight read's buffer can "
                        "be evicted from under it\n", io_queue_depth, v);
        exit(-1);
    }
    return static_cast<size_t>(v);
}

static size_t max_n_cached_matrices = checked_max_n_cached();


static bool iou_enable_sq_poll = false;
static int iou_sq_poll_cpu_affinity = -1;
static int io_worker_cpu_affinity = -1;

}
