#include "llama-mmap.h"
#include "../ggml/src/ggml-backend-moe-cache.h"

#include "llama-impl.h"

#include "ggml.h"

#include <cstring>
#include <climits>
#include <cstdlib>
#include <stdexcept>
#include <cerrno>
#include <algorithm>

#ifdef __has_include
    #if __has_include(<unistd.h>)
        #include <unistd.h>
        #include <fcntl.h>
        #include <sys/stat.h>
        #if defined(_POSIX_MAPPED_FILES)
            #include <sys/mman.h>
        #endif
        #if defined(_POSIX_MEMLOCK_RANGE)
            #include <sys/resource.h>
        #endif
    #endif
#endif

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
    #include <io.h>
    #include <fcntl.h>   // _O_RDONLY for the unbuffered handle wrap (R31/M17)
    #include <malloc.h>  // _aligned_malloc
    #include <memory>
    #include <vector>
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#ifdef _WIN32
#    define llama_mmap_ftell _ftelli64
#    define llama_mmap_fseek _fseeki64
#else
#    define llama_mmap_ftell ftello
#    define llama_mmap_fseek fseeko
#endif

// TODO: consider moving to llama-impl.h if needed in more places
#if defined(_WIN32)
static std::string llama_format_win_err(DWORD err) {
    LPSTR buf;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);
    if (!size) {
        return "FormatMessageA failed";
    }
    std::string ret(buf, size);
    LocalFree(buf);
    return ret;
}
#endif

// llama_file

struct llama_file::impl {
#if defined(_WIN32)
    HANDLE fp_win32;
    struct aligned_buffer_deleter {
        void operator()(void * p) const { _aligned_free(p); }
    };
    std::unique_ptr<void, aligned_buffer_deleter> read_scratch;
    size_t read_scratch_capacity = 0;
    size_t read_scratch_peak = 0;
    std::string GetErrorMessageWin32(DWORD error_code) const {
        std::string ret;
        LPSTR lpMsgBuf = NULL;
        DWORD bufLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                    NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&lpMsgBuf, 0, NULL);
        if (!bufLen) {
            ret = format("Win32 error code: %lx", error_code);
        } else {
            ret = lpMsgBuf;
            LocalFree(lpMsgBuf);
        }

        return ret;
    }

    impl(const char * fname, const char * mode, const bool use_direct_io = false) {
        // R31/M17 (lane-223): the Windows O_DIRECT. FILE_FLAG_NO_BUFFERING bypasses the page cache, which
        // is what a model far bigger than RAM needs (a buffered read of a 93 GB file thrashes the cache the
        // resident tensors live in). Unbuffered handles require sector-aligned offset/length/buffer, so
        // reads go through read_aligned_chunk() with `alignment` = the volume's physical sector size.
        // Runtime switch: --load-mode direct_io (use_direct_io) selects it; any failure falls back to the
        // buffered fopen path below, exactly like the Linux O_DIRECT branch.
        // Origin: FreeToken host_banks.py:380-471 (O_DIRECT readers) + upstream PR #25294 (llama_moe_stream_pread);
        // this is the llama_file half, shared by --load-mode direct_io and any future expert streamer.
        if (use_direct_io && std::strcmp(mode, "rb") == 0) {
            if (init_unbuffered(fname)) {
                return;
            }
            LLAMA_LOG_WARN("Failed to open file '%s' unbuffered (FILE_FLAG_NO_BUFFERING): %s. Falling back to buffered I/O\n",
                           fname, GetErrorMessageWin32(GetLastError()).c_str());
        }

        fp = ggml_fopen(fname, mode);
        if (fp == NULL) {
            throw std::runtime_error(format("failed to open %s: %s", fname, strerror(errno)));
        }
        fp_win32 = (HANDLE) _get_osfhandle(_fileno(fp));
        try {
            seek(0, SEEK_END);
            size = tell();
            seek(0, SEEK_SET);
        } catch (...) {
            std::fclose(fp);
            fp = NULL;
            fp_win32 = NULL;
            throw;
        }
    }

    // open with FILE_FLAG_NO_BUFFERING; on success fp/fp_win32/size/alignment are set and true is returned
    bool init_unbuffered(const char * fname) {
        // UTF-8 -> UTF-16, as ggml_fopen does
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, fname, -1, NULL, 0);
        if (wlen <= 0) {
            return false;
        }
        std::vector<wchar_t> wfname(wlen);
        MultiByteToWideChar(CP_UTF8, 0, fname, -1, wfname.data(), wlen);

        HANDLE h = CreateFileW(wfname.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            return false;
        }

        // sector size for the alignment rule; FILE_STORAGE_INFO is Win8+, 4096 is a multiple of any
        // logical/physical sector size in use, so it is the safe default
        size_t sector = 4096;
#if _WIN32_WINNT >= 0x602
        FILE_STORAGE_INFO fsi;
        if (GetFileInformationByHandleEx(h, FileStorageInfo, &fsi, sizeof(fsi)) && fsi.PhysicalBytesPerSectorForAtomicity > 0) {
            sector = std::max<size_t>(sector, fsi.PhysicalBytesPerSectorForAtomicity);
        }
#endif

        LARGE_INTEGER li;
        if (!GetFileSizeEx(h, &li)) {
            CloseHandle(h);
            return false;
        }

        // wrap the handle in a FILE* so file_id()/_fileno() and the mmap path keep working unchanged
        const int fd = _open_osfhandle((intptr_t) h, _O_RDONLY);
        if (fd == -1) {
            CloseHandle(h);
            return false;
        }
        FILE * f = _fdopen(fd, "rb");
        if (f == NULL) {
            _close(fd); // closes h too
            return false;
        }

        fp        = f;
        fp_win32  = h;
        size      = (size_t) li.QuadPart;
        alignment = sector;

        // probe: one aligned read at offset 0 proves the volume honours the flag (network/overlay
        // filesystems can accept the flag and then refuse aligned reads)
        try {
            std::vector<uint8_t> probe_dst(std::min<size_t>(size, 64));
            if (!probe_dst.empty()) {
                seek(0, SEEK_SET);
                read_aligned_chunk(probe_dst.data(), probe_dst.size());
                seek(0, SEEK_SET);
            }
        } catch (const std::exception & e) {
            LLAMA_LOG_WARN("unbuffered probe read failed on '%s': %s\n", fname, e.what());
            std::fclose(fp);
            fp = NULL;
            fp_win32 = NULL;
            alignment = 1;
            return false;
        }

        LLAMA_LOG_INFO("%s: opened '%s' unbuffered (FILE_FLAG_NO_BUFFERING, sector %zu)\n", __func__, fname, alignment);
        return true;
    }

    impl(FILE * file) : owns_fp(false) {
        fp = file;
        fp_win32 = (HANDLE) _get_osfhandle(_fileno(fp));
        seek(0, SEEK_END);
        size = tell();
        seek(0, SEEK_SET);
    }

    // A FILE_FLAG_NO_BUFFERING handle refuses an unaligned file-pointer move (ERROR_INVALID_PARAMETER,
    // the -lm dio probe failure), so unbuffered handles keep the logical position in logical_position
    // and park the OS pointer on the sector floor (lane-298 + moe-cache-x1, one mechanism).
    size_t tell() const {
        if (alignment > 1) { return logical_position; }
        LARGE_INTEGER li;
        li.QuadPart = 0;
        BOOL ret = SetFilePointerEx(fp_win32, li, &li, FILE_CURRENT);
        if (!ret) {
            throw std::runtime_error(format("read error: %s", GetErrorMessageWin32(GetLastError()).c_str()));
        }

        return li.QuadPart;
    }

    void seek(size_t offset, int whence) const {
        static_assert(SEEK_SET == FILE_BEGIN, "SEEK_SET != FILE_BEGIN");
        static_assert(SEEK_CUR == FILE_CURRENT, "SEEK_CUR != FILE_CURRENT");
        static_assert(SEEK_END == FILE_END, "SEEK_END != FILE_END");

        LARGE_INTEGER li;
        size_t proposed_position = logical_position;
        if (alignment > 1) {
            if (whence == SEEK_SET) {
                proposed_position = offset;
            } else if (whence == SEEK_CUR || whence == SEEK_END) {
                const size_t base = whence == SEEK_CUR ? logical_position : size;
                // The size_t API represents negative relative offsets in two's
                // complement, like the buffered SetFilePointerEx path below.
                if (offset <= size_t(INT64_MAX)) {
                    if (offset > SIZE_MAX - base) { throw std::runtime_error("NVME_SEEK_RANGE_REFUSED"); }
                    proposed_position = base + offset;
                } else {
                    const size_t distance = SIZE_MAX - offset + 1;
                    if (distance > base) { throw std::runtime_error("NVME_SEEK_RANGE_REFUSED"); }
                    proposed_position = base - distance;
                }
            } else {
                throw std::runtime_error("NVME_SEEK_WHENCE_REFUSED");
            }
            if (proposed_position > size_t(INT64_MAX)) { throw std::runtime_error("NVME_SEEK_RANGE_REFUSED"); }
            li.QuadPart = proposed_position & ~(alignment - 1);
            whence = FILE_BEGIN;
        } else {
            li.QuadPart = offset;
        }
        BOOL ret = SetFilePointerEx(fp_win32, li, NULL, whence);
        if (!ret) {
            throw std::runtime_error(format("read error: %s", GetErrorMessageWin32(GetLastError()).c_str()));
        }
        if (alignment > 1) { logical_position = proposed_position; }
    }

    // plain ReadFile loop; on an unbuffered handle the caller guarantees the alignment rule.
    // allow_eof_pad: an unbuffered read of the last partial sector legitimately comes back short at EOF
    void read_raw_unsafe(void * ptr, size_t len, bool allow_eof_pad = false) {
        if (has_direct_io() && ((logical_position | len | (uintptr_t) ptr) & (alignment - 1))) {
            throw std::runtime_error(format("unaligned unbuffered read: pos %zu len %zu align %zu", logical_position, len, alignment));
        }
        size_t bytes_read = 0;
        while (bytes_read < len) {
            size_t chunk_size = std::min<size_t>(len - bytes_read, 64*1024*1024);
            DWORD chunk_read = 0;
            BOOL result = ReadFile(fp_win32, reinterpret_cast<char*>(ptr) + bytes_read, chunk_size, &chunk_read, NULL);
            physical_read_bytes += chunk_read;
            if (alignment > 1) { logical_position += chunk_read; }
            if (!result) {
                throw std::runtime_error(format("read error: %s", GetErrorMessageWin32(GetLastError()).c_str()));
            }
            if (chunk_read < chunk_size || chunk_read == 0) {
                // tell(): after a short unbuffered read the OS pointer sits on unaligned EOF and even a
                // zero FILE_CURRENT move is refused there; logical_position already counts the bytes read
                if (allow_eof_pad && tell() == size) {
                    // EOF inside the alignment padding: zero the rest, the caller only copies `len` real bytes
                    std::memset(reinterpret_cast<char *>(ptr) + bytes_read + chunk_read, 0, len - bytes_read - chunk_read);
                    return;
                }
                throw std::runtime_error("unexpectedly reached end of file");
            }

            bytes_read += chunk_read;
        }
    }

    // unbuffered handles: read the sector-aligned superset into an aligned staging buffer, copy `size` bytes out
    void read_aligned_chunk(void * dest, size_t sz) {
        const size_t offset = tell();
        if (offset > size || sz > size - offset) {
            throw std::runtime_error("NVME_READ_RANGE_REFUSED");
        }
        // Also bound direct callers (e.g. a host tensor load), not just the
        // MoE callback. Padding belongs to the aligned IO, never the request.
        constexpr size_t chunk_limit = 8u << 20;
        if (sz > chunk_limit) {
            for (size_t done = 0; done < sz;) {
                const size_t count = std::min(chunk_limit, sz - done);
                read_aligned_chunk(static_cast<char *>(dest) + done, count);
                done += count;
            }
            return;
        }
        const size_t aligned_offset      = offset & ~(alignment - 1);
        const size_t offset_from_aligned = offset - aligned_offset;
        const size_t bytes_to_read       = (offset_from_aligned + sz + alignment - 1) & ~(alignment - 1);

        // A shard reader reuses this bounded bounce buffer across expert misses.
        // Allocate before replacing it: failure preserves the old reader state.
        if (bytes_to_read > read_scratch_capacity) {
            std::unique_ptr<void, aligned_buffer_deleter> next(_aligned_malloc(bytes_to_read, alignment));
            if (!next) {
                throw std::runtime_error(format("_aligned_malloc(%zu, %zu) failed", bytes_to_read, alignment));
            }
            read_scratch_peak = std::max(read_scratch_peak, read_scratch_capacity + bytes_to_read);
            read_scratch = std::move(next);
            read_scratch_capacity = bytes_to_read;
        }

        seek(aligned_offset, SEEK_SET);
        read_raw_unsafe(read_scratch.get(), bytes_to_read, /*allow_eof_pad =*/ true);

        std::memcpy(dest, reinterpret_cast<const char *>(read_scratch.get()) + offset_from_aligned, sz);

        // leave the logical position where a buffered read would have left it
        seek(offset + sz, SEEK_SET);
    }

    void read_raw(void * ptr, size_t len) {
        if (has_direct_io()) {
            read_aligned_chunk(ptr, len);
        } else {
            read_raw_unsafe(ptr, len);
        }
    }

    uint32_t read_u32() {
        uint32_t val;
        read_raw(&val, sizeof(val));
        return val;
    }

    void write_raw(const void * ptr, size_t len) const {
        size_t bytes_written = 0;
        while (bytes_written < len) {
            size_t chunk_size = std::min<size_t>(len - bytes_written, 64*1024*1024);
            DWORD chunk_written = 0;
            BOOL result = WriteFile(fp_win32, reinterpret_cast<char const*>(ptr) + bytes_written, chunk_size, &chunk_written, NULL);
            if (!result) {
                throw std::runtime_error(format("write error: %s", GetErrorMessageWin32(GetLastError()).c_str()));
            }
            if (chunk_written < chunk_size || chunk_written == 0) {
                throw std::runtime_error("unexpectedly failed to write bytes");
            }

            bytes_written += chunk_written;
        }
    }

    void write_u32(uint32_t val) const {
        write_raw(&val, sizeof(val));
    }

    // true only for a FILE_FLAG_NO_BUFFERING handle (alignment = sector size); buffered opens read plainly
    bool has_direct_io() const {
        return alignment > 1;
    }

    ~impl() {
        if (fp && owns_fp) {
            std::fclose(fp);
        }
    }
#else
    impl(const char * fname, const char * mode, [[maybe_unused]] const bool use_direct_io = false) : fname(fname) {
#ifdef __linux__
        // Try unbuffered I/O for read only
        if (use_direct_io && std::strcmp(mode, "rb") == 0) {
            if (init_fd()) {
                return;
            }
            LLAMA_LOG_WARN("Failed to open file '%s' with error: %s. Falling back to buffered I/O",
                           fname, strerror(errno));
        }
#endif
        init_fp(mode);
    }

#ifdef __linux__
    bool init_fd() {
        fd = open(fname.c_str(), O_RDONLY | O_DIRECT);

        if (fd != -1) {
            struct stat file_stats{};
            fstat(fd, &file_stats);

            size = file_stats.st_size;
            alignment = file_stats.st_blksize;

            off_t ret = lseek(fd, 0, SEEK_SET);
            if (ret == -1) {
                throw std::runtime_error(format("seek error: %s", strerror(errno)));
            }
            return true;
        }
        return false;
    }
#endif

    void init_fp(const char * mode) {
        fp = ggml_fopen(fname.c_str(), mode);
        if (fp == NULL) {
            throw std::runtime_error(format("failed to open %s: %s", fname.c_str(), strerror(errno)));
        }
        seek(0, SEEK_END);
        size = tell();
        seek(0, SEEK_SET);
    }

    impl(FILE * file) : fname("(file*)"), owns_fp(false) {
        fp = file;
        seek(0, SEEK_END);
        size = tell();
        seek(0, SEEK_SET);
    }

    size_t tell() const {
        if (fd == -1) {
            off_t ret = llama_mmap_ftell(fp);
            if (ret == -1) {
                throw std::runtime_error(format("ftell error: %s", strerror(errno)));
            }

            return (size_t) ret;
        }

        off_t pos = lseek(fd, 0, SEEK_CUR);
        if (pos == -1) {
            throw std::runtime_error(format("lseek error: %s", strerror(errno)));
        }
        return (size_t) pos;
    }

    void seek(size_t offset, int whence) const {
        off_t ret = 0;
        if (fd == -1) {
            ret = llama_mmap_fseek(fp, offset, whence);
        } else {
            ret = lseek(fd, offset, whence);
        }
        if (ret == -1) {
            throw std::runtime_error(format("seek error: %s", strerror(errno)));
        }
    }

    void read_raw_unsafe(void * ptr, size_t len) {
        if (len == 0) {
            return;
        }
        errno = 0;
        if (fd == -1) {
            const size_t curr_off = tell();
            const size_t to_read = std::min(len, size - curr_off);

            std::size_t ret = std::fread(ptr, to_read, 1, fp);
            if (ferror(fp)) {
                throw std::runtime_error(format("read error: %s", strerror(errno)));
            }
            if (to_read > 0 && ret != 1) {
                throw std::runtime_error("unexpectedly reached end of file");
            }
        } else {
            size_t bytes_read = 0;
            while (bytes_read < len) {
                const size_t to_read = len - bytes_read;
                ssize_t ret = ::read(fd, reinterpret_cast<char *>(ptr) + bytes_read, to_read);

                if (ret == -1) {
                    if (errno == EINTR) {
                        continue;  // Interrupted by signal, retry
                    }
                    // Fallback to std::fread in case the DMA controller cannot access the buffer
                    if (errno == EFAULT || errno == EINVAL) {
                        LLAMA_LOG_WARN("%s: Falling back to buffered IO due to %s\n", __func__, strerror(errno));
                        auto curr_off = tell();
                        close(fd);
                        fd = -1;
                        alignment = 1;
                        init_fp("rb");
                        seek(curr_off, SEEK_SET);
                        read_raw_unsafe(ptr, len);
                        return;
                    }
                    throw std::runtime_error(format("read error: %s", strerror(errno)));
                }
                if (ret == 0) {
                    // EOF: allow if this read was only pulling alignment padding past file end
                    off_t pos = lseek(fd, 0, SEEK_CUR);
                    if (pos != -1 && (size_t) pos == size) {
                        std::memset(reinterpret_cast<char *>(ptr) + bytes_read, 0, len - bytes_read);
                        return;
                    }
                    throw std::runtime_error("unexpectedly reached end of file");
                }

                bytes_read += (size_t) ret;
            }
        }
    }

    void read_aligned_chunk(void * dest, size_t size) {
        size_t offset = tell();
        off_t aligned_offset = offset & ~(alignment - 1);
        off_t offset_from_alignment = offset - aligned_offset;
        size_t bytes_to_read = (offset_from_alignment + size + alignment - 1) & ~(alignment - 1);

        void * raw_buffer = nullptr;
        int ret = posix_memalign(&raw_buffer, alignment, bytes_to_read);
        if (ret != 0) {
            throw std::runtime_error(format("posix_memalign failed with error %d", ret));
        }

        struct aligned_buffer_deleter {
            void operator()(void * p) const { free(p); }
        };
        std::unique_ptr<void, aligned_buffer_deleter> buffer(raw_buffer);

        seek(aligned_offset, SEEK_SET);
        read_raw_unsafe(buffer.get(), bytes_to_read);

        uintptr_t actual_data = reinterpret_cast<uintptr_t>(buffer.get()) + offset_from_alignment;
        memcpy(dest, reinterpret_cast<void *>(actual_data), size);
    }

    void read_raw(void * ptr, size_t len) {
        if (has_direct_io()) {
            read_aligned_chunk(ptr, len);
        } else {
            read_raw_unsafe(ptr, len);
        }
    }

    uint32_t read_u32() {
        uint32_t ret;
        read_raw(&ret, sizeof(ret));
        return ret;
    }

    void write_raw(const void * ptr, size_t len) const {
        if (len == 0) {
            return;
        }
        errno = 0;
        size_t ret = std::fwrite(ptr, len, 1, fp);
        if (ret != 1) {
            throw std::runtime_error(format("write error: %s", strerror(errno)));
        }
    }

    void write_u32(uint32_t val) const {
        write_raw(&val, sizeof(val));
    }

    bool has_direct_io() const {
        return fd != -1 && alignment > 1;
    }

    ~impl() {
        if (fd != -1) {
            close(fd);
        } else if (owns_fp) {
            std::fclose(fp);
        }
    }
    int fd = -1;
    std::string fname;
#endif

    size_t read_alignment() const {
        return alignment;
    }

    uint64_t physical_read_bytes = 0;
    mutable size_t logical_position = 0;
    size_t alignment = 1;

    FILE * fp{};
    size_t size{};
    bool owns_fp = true;
};

llama_file::llama_file(const char * fname, const char * mode, const bool use_direct_io) :
    path_name(fname), pimpl(std::make_unique<impl>(fname, mode, use_direct_io)) {}

uint64_t llama_file::physical_read_bytes() const { return pimpl->physical_read_bytes; }

llama_file::llama_file(FILE * file) : pimpl(std::make_unique<impl>(file)) {}

llama_file::~llama_file() = default;

size_t llama_file::tell() const { return pimpl->tell(); }
size_t llama_file::size() const { return pimpl->size; }

size_t llama_file::read_alignment() const { return pimpl->read_alignment(); }
bool llama_file::has_direct_io() const { return pimpl->has_direct_io(); }

int llama_file::file_id() const {
#ifdef _WIN32
    return _fileno(pimpl->fp);
#else
    if (pimpl->fd != -1) {
        return pimpl->fd;
    }
#if defined(fileno)
    return fileno(pimpl->fp);
#else
    return ::fileno(pimpl->fp);
#endif
#endif
}

void llama_file::seek(size_t offset, int whence) const { pimpl->seek(offset, whence); }
void llama_file::read_raw(void * ptr, size_t len) { pimpl->read_raw(ptr, len); }
#ifdef _WIN32
// unbuffered: the loader's last aligned chunk may run past EOF into padding (the Linux branch allows it too)
void llama_file::read_raw_unsafe(void * ptr, size_t len) { pimpl->read_raw_unsafe(ptr, len, pimpl->has_direct_io()); }
#else
void llama_file::read_raw_unsafe(void * ptr, size_t len) { pimpl->read_raw_unsafe(ptr, len); }
#endif

uint32_t llama_file::read_u32() { return pimpl->read_u32(); }

void llama_file::write_raw(const void * ptr, size_t len) const { pimpl->write_raw(ptr, len); }
void llama_file::write_u32(uint32_t val) const { pimpl->write_u32(val); }

// llama_mmap

#if defined(_POSIX_MAPPED_FILES) || defined(_WIN32)
// merge `ranges` and return their complement within [0, limit)
static llama_mmap::ranges ranges_complement(llama_mmap::ranges ranges, size_t limit) {
    llama_mmap::ranges res;
    std::sort(ranges.begin(), ranges.end());

    size_t pos = 0;
    for (const auto & range : ranges) {
        const size_t beg = std::min(range.first,  limit);
        const size_t end = std::min(range.second, limit);
        if (beg > pos) {
            res.emplace_back(pos, beg);
        }
        pos = std::max(pos, end);
    }
    if (pos < limit) {
        res.emplace_back(pos, limit);
    }

    return res;
}
#endif

struct llama_mmap::impl {
#ifdef _POSIX_MAPPED_FILES
    std::vector<std::pair<size_t, size_t>> mapped_fragments;

    impl(struct llama_file * file, size_t prefetch, bool numa, const llama_mmap::ranges & lazy_ranges) {
        size = file->size();
        int fd = file->file_id();
        int flags = MAP_SHARED;
        if (numa) { prefetch = 0; }
#ifdef __linux__
        if (posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL)) {
            LLAMA_LOG_WARN("warning: posix_fadvise(.., POSIX_FADV_SEQUENTIAL) failed: %s\n",
                    strerror(errno));
        }
        // MAP_POPULATE would fault in the lazy ranges too
        if (prefetch && lazy_ranges.empty()) { flags |= MAP_POPULATE; }
#endif
        addr = mmap(NULL, file->size(), PROT_READ, flags, fd, 0);
        if (addr == MAP_FAILED) {
            throw std::runtime_error(format("mmap failed: %s", strerror(errno)));
        }

        // page-aligned madvise over [beg, end), clamped to the file
        auto advise = [&](size_t beg, size_t end, int advice, const char * name) {
            const size_t page_size = sysconf(_SC_PAGESIZE);
            beg = beg & ~(page_size - 1);
            end = std::min((end + page_size - 1) & ~(page_size - 1), file->size());
            if (beg >= end) {
                return;
            }
            if (posix_madvise((char *) addr + beg, end - beg, advice)) {
                LLAMA_LOG_WARN("warning: posix_madvise(.., %s) failed: %s\n", name, strerror(errno));
            }
        };

        if (prefetch > 0) {
            for (const auto & range : ranges_complement(lazy_ranges, std::min(file->size(), prefetch))) {
                advise(range.first, range.second, POSIX_MADV_WILLNEED, "POSIX_MADV_WILLNEED");
            }
        }
        for (const auto & range : lazy_ranges) {
            advise(range.first, range.second, POSIX_MADV_RANDOM, "POSIX_MADV_RANDOM");
        }
        if (numa) {
            if (posix_madvise(addr, file->size(), POSIX_MADV_RANDOM)) {
                LLAMA_LOG_WARN("warning: posix_madvise(.., POSIX_MADV_RANDOM) failed: %s\n",
                        strerror(errno));
            }
        }

        mapped_fragments.emplace_back(0, file->size());
    }

    static void align_range(size_t * first, size_t * last, size_t page_size) {
        size_t offset_in_page = *first & (page_size - 1);
        size_t offset_to_page = offset_in_page == 0 ? 0 : page_size - offset_in_page;
        *first += offset_to_page;

        *last = *last & ~(page_size - 1);

        if (*last <= *first) {
            *last = *first;
        }
    }

    void unmap_fragment(size_t first, size_t last) {
        int page_size = sysconf(_SC_PAGESIZE);
        align_range(&first, &last, page_size);
        size_t len = last - first;

        if (len == 0) {
            return;
        }

        GGML_ASSERT(first % page_size == 0);
        GGML_ASSERT(last % page_size == 0);
        GGML_ASSERT(last > first);

        void * next_page_start = (uint8_t *) addr + first;

        if (munmap(next_page_start, len)) {
            LLAMA_LOG_WARN("warning: munmap failed: %s\n", strerror(errno));
        }

        std::vector<std::pair<size_t, size_t>> new_mapped_fragments;
        for (const auto & frag : mapped_fragments) {
            if (frag.first < first && frag.second > last) {
                new_mapped_fragments.emplace_back(frag.first, first);
                new_mapped_fragments.emplace_back(last, frag.second);
            } else if (frag.first < first && frag.second > first) {
                new_mapped_fragments.emplace_back(frag.first, first);
            } else if (frag.first < last && frag.second > last) {
                new_mapped_fragments.emplace_back(last, frag.second);
            } else if (frag.first >= first && frag.second <= last) {
            } else {
                new_mapped_fragments.push_back(frag);
            }
        }
        mapped_fragments = std::move(new_mapped_fragments);
    }

    ~impl() {
        for (const auto & frag : mapped_fragments) {
            if (munmap((char *) addr + frag.first, frag.second - frag.first)) {
                LLAMA_LOG_WARN("warning: munmap failed: %s\n", strerror(errno));
            }
        }
    }
#elif defined(_WIN32)
    HANDLE hMapping = nullptr;

    impl(struct llama_file * file, size_t prefetch, bool numa, const llama_mmap::ranges & lazy_ranges) {
        GGML_UNUSED(numa);

        size = file->size();

        HANDLE hFile = (HANDLE) _get_osfhandle(file->file_id());

        hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

        if (hMapping == NULL) {
            DWORD error = GetLastError();
            throw std::runtime_error(format("CreateFileMappingA failed: %s", llama_format_win_err(error).c_str()));
        }

        addr = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        DWORD error = GetLastError();

        if (addr == NULL) {
            CloseHandle(hMapping);
            throw std::runtime_error(format("MapViewOfFile failed: %s", llama_format_win_err(error).c_str()));
        }

        if (prefetch > 0) {
#if _WIN32_WINNT >= 0x602
            BOOL (WINAPI *pPrefetchVirtualMemory) (HANDLE, ULONG_PTR, PWIN32_MEMORY_RANGE_ENTRY, ULONG);
            HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");

            pPrefetchVirtualMemory = (decltype(pPrefetchVirtualMemory))(void *) GetProcAddress(hKernel32, "PrefetchVirtualMemory");

            if (pPrefetchVirtualMemory) {
                std::vector<WIN32_MEMORY_RANGE_ENTRY> entries;
                for (const auto & range : ranges_complement(lazy_ranges, std::min(size, prefetch))) {
                    WIN32_MEMORY_RANGE_ENTRY entry;
                    entry.VirtualAddress = (char *) addr + range.first;
                    entry.NumberOfBytes  = (SIZE_T) (range.second - range.first);
                    entries.push_back(entry);
                }
                if (!entries.empty() &&
                        !pPrefetchVirtualMemory(GetCurrentProcess(), (ULONG_PTR) entries.size(), entries.data(), 0)) {
                    LLAMA_LOG_WARN("warning: PrefetchVirtualMemory failed: %s\n",
                            llama_format_win_err(GetLastError()).c_str());
                }
            }
#else
            LLAMA_LOG_DEBUG("skipping PrefetchVirtualMemory because _WIN32_WINNT < 0x602\n");
#endif
        }
    }

    void unmap_fragment(size_t first, size_t last) {
        // lane-298 port item 2: the view stays mapped (so a later read cannot fault), but VirtualUnlock on
        // unlocked pages drops them from the working set to the standby list, which counts as available RAM
        static const bool trim = getenv("LLAMA_WIN_UNMAP_FRAGMENT") != nullptr;
        if (!trim) {
            return;
        }
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        const size_t page = si.dwPageSize;
        first = (first + page - 1) & ~(page - 1);
        last  = last & ~(page - 1);
        if (last <= first) {
            return;
        }
        VirtualUnlock((char *) addr + first, last - first); // FALSE + ERROR_NOT_LOCKED is the expected result
    }

    ~impl() {
        if (hMapping) {
            if (addr) {
                if (!UnmapViewOfFile(addr)) {
                    LLAMA_LOG_WARN("warning: UnmapViewOfFile failed: %s\n",
                            llama_format_win_err(GetLastError()).c_str());
                }
            }
            if (!CloseHandle(hMapping)) {
                LLAMA_LOG_WARN("warning: CloseHandle failed: %s\n",
                        llama_format_win_err(GetLastError()).c_str());
            }
        }
    }
#else
    impl(struct llama_file * file, size_t prefetch, bool numa, const llama_mmap::ranges & lazy_ranges) {
        GGML_UNUSED(file);
        GGML_UNUSED(prefetch);
        GGML_UNUSED(numa);
        GGML_UNUSED(lazy_ranges);

        throw std::runtime_error("mmap not supported");
    }

    void unmap_fragment(size_t first, size_t last) {
        GGML_UNUSED(first);
        GGML_UNUSED(last);

        throw std::runtime_error("mmap not supported");
    }
#endif

    void * addr;
    size_t size;
};

llama_mmap::llama_mmap(struct llama_file * file, size_t prefetch, bool numa,
        const ranges & lazy_ranges) : pimpl(std::make_unique<impl>(file, prefetch, numa, lazy_ranges)) {}

llama_mmap::~llama_mmap() {
    ggml_moe_disk_unregister_range(pimpl->addr, pimpl->size);
    // unpin before the pages are unmapped by the impl destructor
    if (host_reg_addr && host_unreg_fn) {
        host_unreg_fn(host_reg_addr);
    }
}

size_t llama_mmap::size() const { return pimpl->size; }
void * llama_mmap::addr() const { return pimpl->addr; }

void llama_mmap::unmap_fragment(size_t first, size_t last) { pimpl->unmap_fragment(first, last); }

bool llama_mmap::release_mapped_pages(const void * p, size_t n) {
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION mbi;
    if (n == 0 || VirtualQuery(p, &mbi, sizeof(mbi)) == 0 || mbi.Type != MEM_MAPPED) {
        return false;
    }
    // outward to whole pages: a page shared with other rows only costs them a soft refault
    const uintptr_t page  = 4096;
    const uintptr_t first = (uintptr_t) p & ~(page - 1);
    const uintptr_t last  = ((uintptr_t) p + n + page - 1) & ~(page - 1);
    VirtualUnlock((void *) first, last - first); // FALSE + ERROR_NOT_LOCKED is the expected result
    return true;
#else
    // ponytail: POSIX keeps clean file pages in the page cache, reclaimable; add madvise if RSS growth shows up
    GGML_UNUSED(p);
    GGML_UNUSED(n);
    return false;
#endif
}

size_t llama_mmap::prefetch_rows(const void * base, size_t row_bytes, const int32_t * rows, size_t n) {
#if defined(_WIN32) && _WIN32_WINNT >= 0x602
    static const auto pf = (BOOL (WINAPI *)(HANDLE, ULONG_PTR, PWIN32_MEMORY_RANGE_ENTRY, ULONG))
        (void *) GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "PrefetchVirtualMemory");
    if (!pf || n == 0) {
        return 0;
    }
    const uintptr_t page = 4096;
    std::vector<uintptr_t> pg;
    pg.reserve(2*n);
    for (size_t i = 0; i < n; ++i) {
        const uintptr_t a = (uintptr_t) base + (uintptr_t) rows[i] * row_bytes;
        for (uintptr_t p = a & ~(page - 1); p < a + row_bytes; p += page) { // a row may straddle two pages
            pg.push_back(p);
        }
    }
    std::sort(pg.begin(), pg.end());
    pg.erase(std::unique(pg.begin(), pg.end()), pg.end());
    std::vector<WIN32_MEMORY_RANGE_ENTRY> e;
    for (uintptr_t p : pg) {
        if (!e.empty() && (uintptr_t) e.back().VirtualAddress + e.back().NumberOfBytes == p) {
            e.back().NumberOfBytes += page;
        } else {
            e.push_back({ (PVOID) p, (SIZE_T) page });
        }
    }
    return pf(GetCurrentProcess(), (ULONG_PTR) e.size(), e.data(), 0) ? pg.size() : 0;
#else
    // ponytail: POSIX would be madvise(MADV_WILLNEED) per page; add if a Linux cell shows the cold-fault cost
    GGML_UNUSED(base);
    GGML_UNUSED(row_bytes);
    GGML_UNUSED(rows);
    GGML_UNUSED(n);
    return 0;
#endif
}

size_t llama_mmap::register_host(size_t first, size_t last, bool (*reg_fn)(void *, size_t), void (*unreg_fn)(void *)) {
#ifdef _POSIX_MAPPED_FILES
    if (host_reg_addr || !reg_fn || !unreg_fn || last <= first) {
        return 0;
    }

    // expand outward to the page boundaries retained by unmap_fragment
    const size_t page_size = sysconf(_SC_PAGESIZE);
    first = first & ~(page_size - 1);
    last  = (last + page_size - 1) & ~(page_size - 1);

    void * reg_addr = (uint8_t *) pimpl->addr + first;
    if (!reg_fn(reg_addr, last - first)) {
        return 0;
    }

    host_reg_addr  = reg_addr;
    host_unreg_fn  = unreg_fn;
    return last - first;
#else
    GGML_UNUSED(first);
    GGML_UNUSED(last);
    GGML_UNUSED(reg_fn);
    GGML_UNUSED(unreg_fn);
    return 0;
#endif
}

#if defined(_POSIX_MEMLOCK_RANGE) || defined(_WIN32)
const bool llama_mmap::SUPPORTED  = true;
#else
const bool llama_mmap::SUPPORTED  = false;
#endif

// llama_mlock

struct llama_mlock::impl {
#ifdef _POSIX_MEMLOCK_RANGE
    static size_t lock_granularity() {
        return (size_t) sysconf(_SC_PAGESIZE);
    }

    bool raw_lock(const void * addr, size_t size) const {
        if (!mlock(addr, size)) {
            return true;
        }

#ifdef __APPLE__
#define MLOCK_SUGGESTION \
        "Try increasing the sysctl values 'vm.user_wire_limit' and 'vm.global_user_wire_limit' and/or " \
        "decreasing 'vm.global_no_user_wire_amount'.  Also try increasing RLIMIT_MEMLOCK (ulimit -l).\n"
#else
#define MLOCK_SUGGESTION \
        "Try increasing RLIMIT_MEMLOCK ('ulimit -l' as root).\n"
#endif

        char* errmsg = std::strerror(errno);
        bool suggest = (errno == ENOMEM);
#if defined(TARGET_OS_VISION) || defined(TARGET_OS_TV) || defined(_AIX) || defined(__HAIKU__)
        // visionOS/tvOS/Haiku don't support RLIMIT_MEMLOCK
        // Skip resource limit checks on these platforms
        suggest = false;
#else
        struct rlimit lock_limit;
        if (suggest && getrlimit(RLIMIT_MEMLOCK, &lock_limit)) {
            suggest = false;
        }
        if (suggest && ((uint64_t)lock_limit.rlim_max > (uint64_t)lock_limit.rlim_cur + size)) {
            suggest = false;
        }
#endif

        LLAMA_LOG_WARN("warning: failed to mlock %zu-byte buffer (after previously locking %zu bytes): %s\n%s",
                size, this->size, errmsg, suggest ? MLOCK_SUGGESTION : "");
        return false;
    }

    static void raw_unlock(void * addr, size_t size) {
        if (munlock(addr, size)) {
            LLAMA_LOG_WARN("warning: failed to munlock buffer: %s\n", std::strerror(errno));
        }
    }
#elif defined(_WIN32)
    static size_t lock_granularity() {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return (size_t) si.dwPageSize;
    }

    bool raw_lock(void * ptr, size_t len) const {
        for (int tries = 1; ; tries++) {
            if (VirtualLock(ptr, len)) {
                return true;
            }
            if (tries == 2) {
                LLAMA_LOG_WARN("warning: failed to VirtualLock %zu-byte buffer (after previously locking %zu bytes): %s\n",
                    len, size, llama_format_win_err(GetLastError()).c_str());
                return false;
            }

            SIZE_T min_ws_size, max_ws_size;
            if (!GetProcessWorkingSetSize(GetCurrentProcess(), &min_ws_size, &max_ws_size)) {
                LLAMA_LOG_WARN("warning: GetProcessWorkingSetSize failed: %s\n",
                        llama_format_win_err(GetLastError()).c_str());
                return false;
            }
            size_t increment = len + 1048576;
            min_ws_size += increment;
            max_ws_size += increment;
            if (!SetProcessWorkingSetSize(GetCurrentProcess(), min_ws_size, max_ws_size)) {
                LLAMA_LOG_WARN("warning: SetProcessWorkingSetSize failed: %s\n",
                        llama_format_win_err(GetLastError()).c_str());
                return false;
            }
        }
    }

    static void raw_unlock(void * ptr, size_t len) {
        if (!VirtualUnlock(ptr, len)) {
            LLAMA_LOG_WARN("warning: failed to VirtualUnlock buffer: %s\n",
                    llama_format_win_err(GetLastError()).c_str());
        }
    }
#else
    static size_t lock_granularity() {
        return (size_t) 65536;
    }

    bool raw_lock(const void * addr, size_t len) const {
        LLAMA_LOG_WARN("warning: mlock not supported on this system\n");
        return false;
    }

    static void raw_unlock(const void * addr, size_t len) {}
#endif

    impl() : addr(NULL), size(0), failed_already(false) {}

    void init(void * ptr) {
        GGML_ASSERT(addr == NULL && size == 0);
        addr = ptr;
    }

    void grow_to(size_t target_size) {
        GGML_ASSERT(addr);
        if (failed_already) {
            return;
        }
        size_t granularity = lock_granularity();
        target_size = (target_size + granularity - 1) & ~(granularity - 1);
        if (target_size > size) {
            if (raw_lock((uint8_t *) addr + size, target_size - size)) {
                size = target_size;
            } else {
                failed_already = true;
            }
        }
    }

    void * addr;
    size_t size;

    bool failed_already;
};

llama_mlock::llama_mlock() : pimpl(std::make_unique<impl>()) {}
llama_mlock::~llama_mlock() = default;

void llama_mlock::init(void * ptr) { pimpl->init(ptr); }
void llama_mlock::grow_to(size_t target_size) { pimpl->grow_to(target_size); }

#if defined(_POSIX_MEMLOCK_RANGE) || defined(_WIN32)
const bool llama_mlock::SUPPORTED = true;
#else
const bool llama_mlock::SUPPORTED = false;
#endif

size_t llama_path_max() {
    return PATH_MAX;
}

size_t llama_file::direct_io_scratch_size() const {
#if defined(_WIN32)
    return pimpl->read_scratch_capacity;
#else
    return 0; // POSIX aligned reads own a temporary buffer, not retained scratch.
#endif
}

size_t llama_file::direct_io_scratch_peak() const {
#if defined(_WIN32)
    return pimpl->read_scratch_peak;
#else
    return 0;
#endif
}
