#pragma once

// X1 shared-memory admission. Shared is ordinary Windows physical RAM, even
// when a host-visible memory type also advertises DEVICE_LOCAL.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <memory>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

inline std::mutex & arifi_vk_allocation_mutex() {
    static std::mutex mutex;
    return mutex;
}

inline uint64_t & arifi_vk_shared_charged() { static uint64_t bytes = 0; return bytes; }
inline uint64_t & arifi_vk_shared_pending() { static uint64_t bytes = 0; return bytes; }

struct arifi_vk_shared_lease {
    uint64_t bytes;
    bool pending = true;
    explicit arifi_vk_shared_lease(uint64_t value) : bytes(value) {}
    ~arifi_vk_shared_lease() {
        std::lock_guard<std::mutex> lock(arifi_vk_allocation_mutex());
        arifi_vk_shared_charged() -= bytes;
        if (pending) { arifi_vk_shared_pending() -= bytes; }
    }
    void commit() {
        std::lock_guard<std::mutex> lock(arifi_vk_allocation_mutex());
        if (pending) { arifi_vk_shared_pending() -= bytes; pending = false; }
    }
};

inline uint64_t arifi_vk_env_mib(const char * name, uint64_t fallback) {
    const char * text = getenv(name);
    if (!text) { return fallback; }
    char * end = nullptr;
    errno = 0;
    const auto value = strtoull(text, &end, 10);
    if (*text == '-' || errno || end == text || *end || value > (UINT64_MAX >> 20)) {
        fprintf(stderr, "[vk-shared-budget] invalid %s=%s; using default\n", name, text);
        return fallback;
    }
    return (uint64_t)value << 20;
}

inline uint64_t arifi_vk_available_bytes() {
#ifdef _WIN32
    MEMORYSTATUSEX state = {};
    state.dwLength = sizeof(state);
    return GlobalMemoryStatusEx(&state) ? state.ullAvailPhys : 0;
#else
    // No platform physical-memory probe yet: shared growth fails closed.
    return 0;
#endif
}

inline uint64_t arifi_vk_shared_remaining(uint64_t already_shared = 0) {
    constexpr uint64_t GiB = uint64_t(1) << 30;
    // Reserve an in-flight model mapping window as well as desktop headroom.
    // The 2 GiB default is the X1 maximum buffer, not the entire mmap file.
    const uint64_t mapping = arifi_vk_env_mib("GGML_ARIFI_VK_FILE_CACHE_RESERVE_MIB", 2 * GiB);
    const uint64_t available = arifi_vk_available_bytes();
    const uint64_t reserve = mapping > UINT64_MAX - 4 * GiB ? UINT64_MAX : mapping + 4 * GiB;
    const uint64_t physical = available > reserve ? available - reserve : 0;
    const uint64_t cap = std::min<uint64_t>(arifi_vk_env_mib("GGML_ARIFI_VK_SHARED_BUDGET_MIB", UINT64_MAX),
                                          uint64_t(11800) << 20);
    return std::min(physical, already_shared < cap ? cap - already_shared : 0);
}

inline bool arifi_vk_shared_admit(uint64_t bytes, uint64_t already_shared = 0) {
    const uint64_t remaining = arifi_vk_shared_remaining(already_shared);
    if (bytes <= remaining) { return true; }
    fprintf(stderr, "[vk-shared-budget] REFUSED_SHARED_BUDGET bytes=%llu remaining=%llu available=%llu; keep existing residency\n",
            (unsigned long long)bytes, (unsigned long long)remaining,
            (unsigned long long)arifi_vk_available_bytes());
    return false;
}

// Allocator-wide shared admission is scoped to MoE NVMe streaming (where it was measured);
// GGML_ARIFI_VK_SHARED_ADMIT=1|0 overrides. Default-on elsewhere refused staging buffers on
// dense loads near 6 GiB available (lane-298 ismoke-10061217 ig2) and fails closed off Windows.
inline bool arifi_vk_shared_admission_on() {
    const char * admit = getenv("GGML_ARIFI_VK_SHARED_ADMIT");
    if (admit && (strcmp(admit, "0") == 0 || strcmp(admit, "1") == 0)) { return admit[0] == '1'; }
    const char * nvme = getenv("GGML_ARIFI_MOE_NVME");
    return nvme && strcmp(nvme, "1") == 0;
}

inline std::shared_ptr<arifi_vk_shared_lease> arifi_vk_shared_reserve(uint64_t bytes) {
    std::lock_guard<std::mutex> lock(arifi_vk_allocation_mutex());
    constexpr uint64_t GiB = uint64_t(1) << 30;
    const auto mapping = arifi_vk_env_mib("GGML_ARIFI_VK_FILE_CACHE_RESERVE_MIB", 2 * GiB);
    const auto available = arifi_vk_available_bytes();
    const auto reserve = mapping > UINT64_MAX - 4 * GiB ? UINT64_MAX : mapping + 4 * GiB;
    const auto physical = available > reserve ? available - reserve : 0;
    const auto cap = std::min<uint64_t>(arifi_vk_env_mib("GGML_ARIFI_VK_SHARED_BUDGET_MIB", UINT64_MAX), uint64_t(11800) << 20);
    const auto cap_remaining = arifi_vk_shared_charged() < cap ? cap - arifi_vk_shared_charged() : 0;
    const auto pending = arifi_vk_shared_pending();
    if (pending > physical || bytes > physical - pending || bytes > cap_remaining) {
        fprintf(stderr, "[vk-shared-budget] REFUSED_SHARED_BUDGET bytes=%llu remaining=%llu pending=%llu\n",
                (unsigned long long)bytes, (unsigned long long)physical, (unsigned long long)pending);
        return {};
    }
    auto lease = std::make_shared<arifi_vk_shared_lease>(bytes);
    arifi_vk_shared_charged() += bytes;
    arifi_vk_shared_pending() += bytes;
    return lease;
}

// One ledger for all pure-device-local allocations in this Vulkan module,
// including the expert provider and the main backend. Pending allocations count.
inline uint64_t & arifi_vk_local_charged() { static uint64_t bytes = 0; return bytes; }
struct arifi_vk_local_lease {
    uint64_t bytes;
    explicit arifi_vk_local_lease(uint64_t value) : bytes(value) {}
    ~arifi_vk_local_lease() {
        std::lock_guard<std::mutex> lock(arifi_vk_allocation_mutex());
        arifi_vk_local_charged() -= bytes;
    }
};
inline std::shared_ptr<arifi_vk_local_lease> arifi_vk_local_reserve(uint64_t bytes) {
    std::lock_guard<std::mutex> lock(arifi_vk_allocation_mutex());
    const uint64_t cap = uint64_t(70) << 30;
    if (arifi_vk_local_charged() > cap || bytes > cap - arifi_vk_local_charged()) {
        fprintf(stderr, "[vk-shared-budget] REFUSED_LOCAL_70GIB bytes=%llu charged=%llu\n",
                (unsigned long long)bytes, (unsigned long long)arifi_vk_local_charged());
        return {};
    }
    auto lease = std::make_shared<arifi_vk_local_lease>(bytes);
    arifi_vk_local_charged() += bytes;
    return lease;
}
