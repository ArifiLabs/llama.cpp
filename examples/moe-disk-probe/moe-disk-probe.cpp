#include "llama-mmap.h"
#include "ggml-backend-moe-cache.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

struct reader {
    llama_file file;
    size_t offset;
    bool fail = false;
    reader(const char * path, size_t start) : file(path, "rb", true), offset(start) {
        if (!file.has_direct_io()) { throw std::runtime_error("direct IO required"); }
    }
};
static int read_bytes(void * opaque, uint64_t offset, void * out, size_t bytes, uint64_t * physical) {
    auto & r = *static_cast<reader *>(opaque);
    if (r.fail) { return 0; }
    const uint64_t before = r.file.physical_read_bytes();
    r.file.seek(r.offset + offset, SEEK_SET);
    r.file.read_raw(out, bytes);
    if (physical) { *physical = r.file.physical_read_bytes() - before; }
    return 1;
}
static void free_reader(void * opaque) { delete static_cast<reader *>(opaque); }
int main(int argc, char ** argv) try {
    if (argc != 2 && argc != 3) { return 2; }
    std::vector<unsigned char> expected(10u * 1024 * 1024 + 371);
    for (size_t i = 0; i < expected.size(); ++i) { expected[i] = (i * 37 + i / 127) & 255; }
    if (argc == 2) {
        llama_file file(argv[1], "wb");
        file.write_raw(expected.data(), expected.size());
    } else {
        llama_file file(argv[1], "rb");
        file.read_raw(expected.data(), expected.size());
    }
    llama_file source(argv[1], "rb", true);
    if (!source.has_direct_io()) { throw std::runtime_error("direct IO fallback"); }
    const size_t origin = 37;
    const size_t length = source.size() - origin;
    source.seek(source.size() - 17, SEEK_SET);
    unsigned char eof_probe[18];
    bool eof_refused = false;
    try { source.read_raw(eof_probe, sizeof(eof_probe)); }
    catch (const std::runtime_error &) { eof_refused = true; }
    if (!eof_refused) { throw std::runtime_error("EOF request padded as real data"); }
    source.seek(origin, SEEK_SET);
    {
        llama_mmap identity(&source, 0);
        void * base = static_cast<unsigned char *>(identity.addr()) + origin;
        ggml_moe_cache_tensor_desc desc = {"blk.0.ffn_up_exps.weight", base, 1, 1, 1, int64_t(length), GGML_TYPE_Q4_K};
        auto * r = new reader(argv[1], origin);
        if (!ggml_moe_disk_register(&desc, r, read_bytes, free_reader)) { delete r; throw std::runtime_error("register failed"); }
        if (!ggml_moe_disk_contains(base)) { throw std::runtime_error("missing registry"); }
        int cases = 0;
        for (size_t offset : {size_t(0), size_t(17), size_t(4093), length - 731}) {
            const size_t count = std::min<size_t>(731, length - offset);
            std::vector<unsigned char> out(count);
            std::vector<unsigned char> reference(count);
            if (argc == 3) {
                llama_file buffered(argv[1], "rb");
                buffered.seek(origin + offset, SEEK_SET);
                buffered.read_raw(reference.data(), count);
            } else {
                memcpy(reference.data(), expected.data() + origin + offset, count);
            }
            uint64_t physical = 0;
            if (!ggml_moe_disk_read(base, offset, out.data(), count, &physical) ||
                    memcmp(out.data(), reference.data(), count) || physical < count) {
                throw std::runtime_error("byte mismatch");
            }
            ++cases;
        }
        // Same unbuffered reader: grow scratch, shrink reads, alternate offsets,
        // and repeat a last-partial-sector read without stale-byte/cursor reuse.
        size_t peak_scratch = 0;
        for (int i = 0; i < 128; ++i) {
            const size_t count = i % 3 == 0 ? length : (i % 3 == 1 ? 8201 : 731);
            const size_t offset = i % 2 ? length - count : 0;
            std::vector<unsigned char> out(count), reference(count);
            llama_file buffered(argv[1], "rb");
            buffered.seek(origin + offset, SEEK_SET);
            buffered.read_raw(reference.data(), count);
            uint64_t physical = 0;
            if (!ggml_moe_disk_read(base, offset, out.data(), count, &physical) ||
                    out != reference || physical < count) { throw std::runtime_error("scratch reuse bytes failed"); }
#if defined(_WIN32)
            const size_t scratch = r->file.direct_io_scratch_size();
            if (i == 0) { peak_scratch = scratch; }
            if (!scratch || scratch != peak_scratch || scratch > (8u << 20) + 2*r->file.read_alignment()) {
                throw std::runtime_error("scratch reuse bound failed");
            }
#endif
        }
        printf("NVME_SCRATCH_REUSE PASS 128/128 retained_bytes=%zu\n", peak_scratch);
        unsigned char byte = 0;
        uint64_t physical = 0;
        if (ggml_moe_disk_read(base, length, &byte, 1, &physical)) { throw std::runtime_error("range overflow accepted"); }
        r->fail = true;
        if (ggml_moe_disk_read(base, 0, &byte, 1, &physical)) { throw std::runtime_error("read failure accepted"); }
        puts("NVME_DIRECT_BYTES PASS 4/4; RANGE_RED PASS; READ_FAILURE_RED PASS");
    }
    if (ggml_moe_disk_descriptors(nullptr, 0)) { throw std::runtime_error("reader leaked after unmap"); }
    puts("NVME_UNMAP_CLEANUP PASS; registry_count=0");
    return 0;
} catch (const std::exception & error) {
    fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
}
