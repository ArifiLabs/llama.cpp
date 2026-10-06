#include "llama-moe-disk.h"
#include "llama.h"
#include "ggml-backend.h"
#include <cstring>
#include <cstdio>

// --bench <file> <calls>: pool batch vs sequential read of 8 random ~2.75 MB ranges per call (the GLM decode
// shape). First 32 calls compare bytes; then times both. Loads the Vulkan ICD (device count) so thread
// attach/detach costs match the engine. Disk + CPU only; no GPU memory.
static int bench(const char * path, int calls) {
    llama_backend_init();
    printf("devices=%zu\n", ggml_backend_dev_count());
    auto source = std::make_shared<moe_disk_source>(path);
    moe_disk_reader reader(source, 0);
    const size_t range = 2756608, n = 8, limit = source->file.size() - range;
    std::vector<unsigned char> a(range * n), b(range * n);
    uint64_t state = 0x9E3779B97F4A7C15ull;
    auto next = [&] { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };
    auto fill = [&](std::vector<unsigned char> & buf, ggml_moe_disk_range * r) {
        for (size_t i = 0; i < n; ++i) { r[i] = {(next() % limit) & ~uint64_t(31), buf.data() + i * range, range}; }
    };
    double t_pool = 0, t_sync = 0;
    for (int c = 0; c < calls; ++c) {
        ggml_moe_disk_range r[8], s[8];
        fill(a, r);
        for (size_t i = 0; i < n; ++i) { s[i] = {r[i].offset, b.data() + i * range, range}; }
        uint64_t bytes = 0;
        auto t0 = std::chrono::steady_clock::now();
        if (!moe_disk_batch(&reader, r, n, &bytes)) { throw std::runtime_error("pool batch failed"); }
        auto t1 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < n; ++i) {
            if (!moe_disk_read(&reader, s[i].offset, s[i].dst, s[i].bytes, &bytes)) { throw std::runtime_error("sync read failed"); }
        }
        auto t2 = std::chrono::steady_clock::now();
        t_pool += std::chrono::duration<double>(t1 - t0).count();
        t_sync += std::chrono::duration<double>(t2 - t1).count();
        if (c < 32 && a != b) { throw std::runtime_error("pool vs sync byte mismatch"); }
    }
    const double mib = double(range) * n * calls / 1048576.0;
    printf("NVME_POOL_BENCH PASS calls=%d ranges=%zu bytes_equal_calls=%d pool_ms_per_call=%.3f sync_ms_per_call=%.3f pool_MiBps=%.1f sync_MiBps=%.1f\n",
            calls, n, std::min(calls, 32), 1000 * t_pool / calls, 1000 * t_sync / calls, mib / t_pool, mib / t_sync);
    return 0;
}

int main(int argc, char ** argv) try {
    if (argc == 4 && std::string(argv[1]) == "--bench") { return bench(argv[2], atoi(argv[3])); }
    // Runner owns and hashes these two identical synthetic files and manifest.
    if (argc != 3) { return 2; }
    auto source = std::make_shared<moe_disk_source>(argv[1]);
    if (source->paths.size() != 2) { throw std::runtime_error("fixture needs two replicas"); }
    moe_disk_reader reader(source, 37);
    int cases = 0;
    for (const char * depth : {"2", "8", "16"}) {
#ifdef _WIN32
        _putenv_s("GGML_ARIFI_MOE_NVME_IO_LANES", depth);
#else
        setenv("GGML_ARIFI_MOE_NVME_IO_LANES", depth, 1);
#endif
        for (int round = 0; round < 4; ++round) {
            const size_t length = round % 2 ? 8201 : (17u << 20) + 371;
            std::vector<unsigned char> a(length), b(731), c(29), expected_a(length), expected_b(731), expected_c(29);
            const size_t tail = source->file.size() - reader.offset - c.size();
            ggml_moe_disk_range ranges[] = {{17, a.data(), a.size()}, {4093, b.data(), b.size()}, {tail, c.data(), c.size()}};
            uint64_t bytes = 0;
            if (!moe_disk_batch(&reader, ranges, 3, &bytes) || bytes < length + b.size() + c.size()) {
                throw std::runtime_error("production batch refused");
            }
            llama_file reference(argv[1], "rb");
            reference.seek(reader.offset + 17, SEEK_SET); reference.read_raw(expected_a.data(), expected_a.size());
            reference.seek(reader.offset + 4093, SEEK_SET); reference.read_raw(expected_b.data(), expected_b.size());
            reference.seek(reader.offset + tail, SEEK_SET); reference.read_raw(expected_c.data(), expected_c.size());
            if (a != expected_a || b != expected_b || c != expected_c) { throw std::runtime_error("replica byte mismatch"); }
            ++cases;
        }
    }
    unsigned char out = 0;
    uint64_t bytes = 123;
    ggml_moe_disk_range bad = {source->file.size() - reader.offset, &out, 1};
    if (moe_disk_batch(&reader, &bad, 1, &bytes) || bytes) { throw std::runtime_error("EOF range accepted"); }
    bad = {UINT64_MAX, &out, 1};
    if (moe_disk_batch(&reader, &bad, 1, &bytes)) { throw std::runtime_error("overflow accepted"); }
#ifdef _WIN32
    _putenv_s("GGML_ARIFI_MOE_NVME_IO_LANES", "1");
#else
    setenv("GGML_ARIFI_MOE_NVME_IO_LANES", "1", 1);
#endif
    bad = {0, &out, 1};
    if (moe_disk_batch(&reader, &bad, 1, &bytes)) { throw std::runtime_error("insufficient lanes accepted"); }
    // Wrong-size, duplicate primary and malformed manifests must throw before I/O.
    for (int mode = 0; mode < 3; ++mode) {
        const std::string manifest = std::string(argv[2]) + ".red";
        {
            std::ofstream f(manifest);
            f << "# nvme-replicas-v1 quoted-primary quoted-replica bytes sha256\n";
            if (mode == 2) { f << "malformed\n"; }
            else { f << std::quoted(std::string(argv[1])) << ' ' << std::quoted(mode == 0 ? source->paths[1] : std::string(argv[1]))
                     << ' ' << (source->file.size() + (mode == 0 ? 1 : 0)) << ' ' << std::string(64, 'a') << '\n'; }
        }
#ifdef _WIN32
        _putenv_s("GGML_ARIFI_MOE_NVME_REPLICAS", manifest.c_str());
#else
        setenv("GGML_ARIFI_MOE_NVME_REPLICAS", manifest.c_str(), 1);
#endif
        bool refused = false;
        try { moe_disk_source red(argv[1]); } catch (const std::exception &) { refused = true; }
        if (!refused) { throw std::runtime_error("manifest RED accepted"); }
    }
    printf("NVME_REPLICA_PRODUCTION PASS %d/12 depths=2,8,16 bytes-equal; EOF/overflow/lanes/size/duplicate/format RED PASS\n", cases);
    return cases == 12 ? 0 : 1;
} catch (const std::exception & error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
