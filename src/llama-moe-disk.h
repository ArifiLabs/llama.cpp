#pragma once
// Internal miss reader shared by the loader and the byte-level production fixture.
#include "llama-mmap.h"
#include "llama-impl.h"
#include "../ggml/src/ggml-backend-moe-cache.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

inline std::string moe_disk_path_key(const std::string & path) {
    std::string key = std::filesystem::weakly_canonical(path).generic_string();
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
#endif
    return key;
}

inline std::atomic<size_t> moe_disk_live_sources{0};
inline std::mutex moe_disk_lane_mutex;
static constexpr size_t moe_disk_max_lanes = 16;
inline std::array<std::unique_ptr<llama_file>, moe_disk_max_lanes> moe_disk_lanes;
inline std::vector<std::string> moe_disk_lane_paths;

struct moe_disk_source {
    llama_file file;
    std::mutex mutex;
    size_t reported_scratch = 0;
    std::vector<std::string> paths;
    explicit moe_disk_source(const char * path) : file(path, "rb", true) {
        if (!file.has_direct_io()) { throw std::runtime_error("NVME_DIRECT_IO_REQUIRED"); }
        if (file.read_alignment() > 65536) { throw std::runtime_error("NVME_READ_ALIGNMENT_REFUSED"); }
        paths.push_back(file.path_name);
        // Manifest equality is established by the detached coordinator's full
        // SHA256 pass under read-only leases. Never infer equality from length.
        const char * manifest = getenv("GGML_ARIFI_MOE_NVME_REPLICAS");
        if (manifest && *manifest) {
            std::ifstream input(manifest);
            std::string line;
            if (!input || !std::getline(input, line) ||
                    line != "# nvme-replicas-v1 quoted-primary quoted-replica bytes sha256") {
                throw std::runtime_error("NVME_REPLICA_MANIFEST_REFUSED");
            }
            const std::string primary_key = moe_disk_path_key(file.path_name);
            while (std::getline(input, line)) {
                if (line.empty() || line[0] == '#') { continue; }
                std::istringstream row(line);
                std::string primary, replica, digest, extra;
                uint64_t bytes = 0;
                if (!(row >> std::quoted(primary) >> std::quoted(replica) >> bytes >> digest) ||
                        (row >> extra) || primary.empty() || replica.empty() || !bytes ||
                        digest.size() != 64 || !std::all_of(digest.begin(), digest.end(),
                            [](unsigned char c) { return std::isxdigit(c); })) {
                    throw std::runtime_error("NVME_REPLICA_FORMAT_REFUSED");
                }
                if (moe_disk_path_key(primary) != primary_key) { continue; }
                if (bytes != file.size() || std::filesystem::file_size(replica) != bytes) {
                    throw std::runtime_error("NVME_REPLICA_SIZE_REFUSED");
                }
                const std::string key = moe_disk_path_key(replica);
                if (std::any_of(paths.begin(), paths.end(), [&](const std::string & p) {
                        return moe_disk_path_key(p) == key;
                    }) || paths.size() >= moe_disk_max_lanes) {
                    throw std::runtime_error("NVME_REPLICA_DUPLICATE_OR_COUNT_REFUSED");
                }
                paths.push_back(replica);
            }
            if (!input.eof()) { throw std::runtime_error("NVME_REPLICA_MANIFEST_READ_FAILED"); }
        }
        // Across all loaders/models in this process, retain at most 32 shard
        // buffers: <= 260 MiB retained, <= 520 MiB including grow replacements.
        if (moe_disk_live_sources.fetch_add(1) >= 32) {
            moe_disk_live_sources.fetch_sub(1);
            throw std::runtime_error("NVME_READER_STAGING_BUDGET_REFUSED");
        }
    }
    ~moe_disk_source() { moe_disk_live_sources.fetch_sub(1); }

};

struct moe_disk_reader {
    std::shared_ptr<moe_disk_source> source;
    size_t offset;
    moe_disk_reader(std::shared_ptr<moe_disk_source> src, size_t start) : source(std::move(src)), offset(start) {}
};

inline int moe_disk_read(void * opaque, uint64_t offset, void * dst, size_t bytes, uint64_t * physical) {
    auto & reader = *static_cast<moe_disk_reader *>(opaque);
    try {
        if (offset > SIZE_MAX - reader.offset) { return 0; }
        auto & source = *reader.source;
        // seek + all chunks + physical counter delta is one transaction; different
        // tensors must never move a shared file cursor underneath another miss.
        std::lock_guard<std::mutex> lock(source.mutex);
        const uint64_t before = source.file.physical_read_bytes();
        source.file.seek(reader.offset + size_t(offset), SEEK_SET);
        for (size_t done = 0; done < bytes;) {
            const size_t count = std::min<size_t>(bytes - done, 8u << 20);
            source.file.read_raw(static_cast<char *>(dst) + done, count);
            done += count;
        }
        const size_t scratch = source.file.direct_io_scratch_size();
        if (scratch != source.reported_scratch) {
            LLAMA_LOG_INFO("NVME_READ_SCRATCH source=%p bytes=%zu peak=%zu chunk_limit=8388608\n",
                    (void *) &source, scratch, source.file.direct_io_scratch_peak());
            source.reported_scratch = scratch;
        }
        if (physical) { *physical = source.file.physical_read_bytes() - before; }
        return 1;
    } catch (const std::exception & error) {
        LLAMA_LOG_ERROR("NVME_READ_FAILED: %s\n", error.what());
        return 0;
    }
}

// Persistent lane workers (x4): x3 measured ~70 ms per decode node lost to 8 thread create/join per call
// (std::async) plus per-lane log lines. Threads are created once, never joined (detached, leaked pool):
// they block on cv_work and die with the process. Callers serialize through moe_disk_lane_mutex.
struct moe_disk_pool {
    std::mutex mu;
    std::condition_variable cv_work, cv_done;
    size_t spawned = 0, active = 0, running = 0, drives = 1;
    uint64_t generation = 0;
    const std::vector<std::vector<ggml_moe_disk_range>> * jobs = nullptr;
    std::array<std::atomic<size_t>, moe_disk_max_lanes> cursor{};
    std::array<uint64_t, moe_disk_max_lanes> lane_bytes{};
    std::array<bool, moe_disk_max_lanes> lane_ok{};
    std::array<size_t, moe_disk_max_lanes> lane_scratch{};
};
inline moe_disk_pool & moe_disk_pool_get() { static auto * pool = new moe_disk_pool(); return *pool; }

inline void moe_disk_worker(size_t lane, uint64_t seen) {
    auto & p = moe_disk_pool_get();
    for (;;) {
        std::unique_lock<std::mutex> lk(p.mu);
        p.cv_work.wait(lk, [&] { return p.generation != seen; });
        seen = p.generation;
        if (lane >= p.active) { continue; }
        const size_t d = lane % p.drives;
        const auto & jobs = (*p.jobs)[d];
        lk.unlock();
        uint64_t bytes = 0;
        bool ok = true;
        try {
            auto & file = *moe_disk_lanes[lane];
            const uint64_t before = file.physical_read_bytes();
            for (size_t i; (i = p.cursor[d].fetch_add(1)) < jobs.size();) {
                file.seek(jobs[i].offset, SEEK_SET);
                file.read_raw(jobs[i].dst, jobs[i].bytes);
            }
            bytes = file.physical_read_bytes() - before;
            const size_t scratch = file.direct_io_scratch_size();
            if (scratch != p.lane_scratch[lane]) {
                p.lane_scratch[lane] = scratch;
                LLAMA_LOG_INFO("NVME_READ_SCRATCH source=%p bytes=%zu peak=%zu chunk_limit=8388608\n",
                        (void *)&file, scratch, file.direct_io_scratch_peak());
            }
        } catch (const std::exception & error) {
            ok = false;
            LLAMA_LOG_ERROR("NVME_READ_LANE_FAILED lane=%zu: %s\n", lane, error.what());
        } catch (...) { ok = false; }
        lk.lock();
        p.lane_bytes[lane] = bytes;
        p.lane_ok[lane] = ok;
        if (--p.running == 0) { p.cv_done.notify_one(); }
    }
}

inline int moe_disk_batch(void * opaque, const ggml_moe_disk_range * ranges, size_t n, uint64_t * physical) {
    auto & reader = *static_cast<moe_disk_reader *>(opaque);
    auto & source = *reader.source;
    if (physical) { *physical = 0; }
    const auto entered = std::chrono::steady_clock::now();
    static double prev_log_seconds = 0;
    try {
        std::lock_guard<std::mutex> lock(source.mutex);
        std::lock_guard<std::mutex> lane_lock(moe_disk_lane_mutex);
        const double lock_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - entered).count();
        const char * setting = getenv("GGML_ARIFI_MOE_NVME_IO_LANES");
        char * tail = nullptr;
        const long requested = setting ? strtol(setting, &tail, 10) : 4;
        if (requested < 1 || requested > long(moe_disk_max_lanes) ||
                (setting && (tail == setting || *tail))) { throw std::runtime_error("NVME_IO_LANES_REFUSED"); }
        const size_t lane_count = size_t(requested), drive_count = source.paths.size();
        if (lane_count < drive_count) { throw std::runtime_error("NVME_REPLICA_LANES_REFUSED"); }
        std::vector<std::string> lane_paths;
        for (size_t i = 0; i < lane_count; ++i) { lane_paths.push_back(source.paths[i % drive_count]); }
        if (moe_disk_lane_paths != lane_paths) {
            for (auto & lane : moe_disk_lanes) { lane.reset(); }
            moe_disk_lane_paths.clear();
            for (size_t i = 0; i < lane_count; ++i) {
                moe_disk_lanes[i].reset(new llama_file(lane_paths[i].c_str(), "rb", true));
                if (!moe_disk_lanes[i]->has_direct_io() || moe_disk_lanes[i]->read_alignment() > 65536 ||
                        moe_disk_lanes[i]->size() != source.file.size()) {
                    throw std::runtime_error("NVME_REPLICA_IO_REFUSED");
                }
            }
            moe_disk_lane_paths = lane_paths;
            for (size_t d = 0; d < drive_count; ++d) {
                LLAMA_LOG_INFO("NVME_READ_SOURCE drive=%zu path=%s configured_workers=%zu no_buffering=1\n",
                        d, source.paths[d].c_str(), (lane_count + drive_count - 1 - d) / drive_count);
            }
        }
        // Sorted/coalesced runs remain contiguous within each 16MiB job. Jobs
        // are striped by bytes across replicas, each with an independent queue.
        std::vector<std::vector<ggml_moe_disk_range>> jobs(drive_count);
        size_t total_bytes = 0, job_count = 0;
        for (size_t i = 0; i < n; ++i) {
            if (ranges[i].offset > SIZE_MAX - reader.offset) { return 0; }
            const size_t offset = reader.offset + size_t(ranges[i].offset);
            if (offset > source.file.size() || ranges[i].bytes > source.file.size() - offset ||
                    (ranges[i].bytes && !ranges[i].dst) ||
                    ranges[i].bytes > (size_t(1) << 30) - total_bytes) { return 0; }
            total_bytes += ranges[i].bytes;
            for (size_t done = 0; done < ranges[i].bytes;) {
                const size_t bytes = std::min<size_t>(ranges[i].bytes - done, 16u << 20);
                jobs[job_count++ % drive_count].push_back({offset + done,
                        static_cast<char *>(ranges[i].dst) + done, bytes});
                done += bytes;
            }
        }
        std::array<uint64_t, moe_disk_max_lanes> drive_bytes{};
        size_t busy_lanes = 0;
        for (size_t lane = 0; lane < lane_count; ++lane) {
            if (lane / drive_count < jobs[lane % drive_count].size()) { ++busy_lanes; }
        }
        auto & pool = moe_disk_pool_get();
        const auto started = std::chrono::steady_clock::now();
        {
            std::unique_lock<std::mutex> lk(pool.mu);
            while (pool.spawned < lane_count) {
                std::thread(moe_disk_worker, pool.spawned, pool.generation).detach();
                ++pool.spawned;
            }
            pool.jobs = &jobs;
            pool.drives = drive_count;
            pool.active = lane_count;
            pool.running = lane_count;
            for (auto & value : pool.cursor) { value.store(0); }
            ++pool.generation;
            pool.cv_work.notify_all();
            // jobs/cursor are read by workers: wait for EVERY active lane, not only for the jobs to drain.
            pool.cv_done.wait(lk, [&] { return pool.running == 0; });
            pool.jobs = nullptr;
        }
        uint64_t bytes = 0;
        bool ok = true;
        for (size_t lane = 0; lane < lane_count; ++lane) {
            ok = ok && pool.lane_ok[lane];
            drive_bytes[lane % drive_count] += pool.lane_bytes[lane];
            bytes += pool.lane_bytes[lane];
        }
        if (!ok) { return 0; }
        const auto finished = std::chrono::steady_clock::now();
        const double wall = std::chrono::duration<double>(finished - started).count();
        const double setup_seconds = std::chrono::duration<double>(started - entered).count();
        if (physical) { *physical = bytes; }
        // x9: quiet runs (GGML_ARIFI_VK_MOE_TRACE unset or '0') log one ADDITIVE line per drive every 64 calls
        // (bytes and wall summed since the last line) instead of three lines per call.
        static const bool trace = [] { const char * v = getenv("GGML_ARIFI_VK_MOE_TRACE"); return v && *v && strcmp(v, "0") != 0; }();
        static std::array<uint64_t, moe_disk_max_lanes> agg_bytes{};
        static std::array<size_t, moe_disk_max_lanes> agg_jobs{};
        static double agg_wall = 0;
        static size_t agg_calls = 0;
        if (!trace) {
            for (size_t d = 0; d < drive_count; ++d) { agg_bytes[d] += drive_bytes[d]; agg_jobs[d] += jobs[d].size(); }
            agg_wall += wall;
            if (++agg_calls < 64) { return 1; }
            for (size_t d = 0; d < drive_count; ++d) {
                LLAMA_LOG_INFO("NVME_READ_DRIVE drive=%zu bytes=%llu batch_wall_seconds=%.9f effective_MiBps=%.3f jobs=%zu aggregate_calls=%zu\n",
                        d, (unsigned long long)agg_bytes[d], agg_wall,
                        agg_wall > 0 ? agg_bytes[d] / 1048576.0 / agg_wall : 0, agg_jobs[d], agg_calls);
                agg_bytes[d] = 0; agg_jobs[d] = 0;
            }
            agg_wall = 0; agg_calls = 0;
            return 1;
        }
        for (size_t d = 0; d < drive_count; ++d) {
            LLAMA_LOG_INFO("NVME_READ_DRIVE drive=%zu bytes=%llu batch_wall_seconds=%.9f effective_MiBps=%.3f jobs=%zu\n",
                    d, (unsigned long long)drive_bytes[d], wall,
                    wall > 0 ? drive_bytes[d] / 1048576.0 / wall : 0, jobs[d].size());
        }
        LLAMA_LOG_INFO("NVME_READ_BATCH ranges=%zu jobs=%zu lanes=%zu configured_lanes=%zu drives=%zu no_buffering=1 backend=parallel-direct-cursors bytes=%llu lock_s=%.6f setup_s=%.6f wall_s=%.6f prev_log_s=%.6f\n",
                n, job_count, busy_lanes, lane_count, drive_count, (unsigned long long)bytes,
                lock_seconds, setup_seconds, wall, prev_log_seconds);
        prev_log_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - finished).count();
        return 1;
    } catch (const std::exception & error) { LLAMA_LOG_ERROR("NVME_READ_BATCH_FAILED: %s\n", error.what()); return 0; }
}

inline void moe_disk_free(void * reader) { delete static_cast<moe_disk_reader *>(reader); }

