#include "az/core/aligned_alloc.hpp"

#include <cstdlib>

namespace az {

void *aligned_alloc(size_t alignment, size_t size) {
    // Align up size to surpass some warnings from sanitizers
    size_t remainder = size % alignment;
    if (remainder != 0) {
        size += alignment - remainder;
    }

#if defined(_WIN32)
    // lane-110 Windows port: was _MSC_VER-only, but MinGW/UCRT also lacks C11 ::aligned_alloc —
    // the CRT allocator is _aligned_malloc on ALL Windows toolchains (args reversed vs C11).
    return ::_aligned_malloc(size, alignment);
#else
    return ::aligned_alloc(alignment, size);
#endif
}

void aligned_free(void *ptr) {
#if defined(_WIN32)
    ::_aligned_free(ptr);
#else
    ::free(ptr);
#endif
}

}  // namespace az
