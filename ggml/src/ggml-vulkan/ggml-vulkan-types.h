#pragma once

// vulkan_core.h MUST come first: ggml-vulkan.h guards the MoE-cache handle accessors on
// `#if defined(VK_VERSION_1_0)` so that non-Vulkan builds of the header stay free of the
// Vulkan headers. With ggml-vulkan.h first the guard is false here, the declarations are
// skipped, and the definitions below silently acquire C++ linkage while every caller that
// includes <vulkan/vulkan.h> first expects C linkage — a link error, not a compile error.
#include <vulkan/vulkan_core.h>
#include "ggml-vulkan.h"

#if defined(GGML_VULKAN_RUN_TESTS) || defined(GGML_VULKAN_CHECK_RESULTS)
#include <chrono>
#include "ggml-cpu.h"
#endif

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#if VK_HEADER_VERSION >= 301
namespace vk::detail { class DispatchLoaderDynamic; }
using vk::detail::DispatchLoaderDynamic;
#else
namespace vk { class DispatchLoaderDynamic; }
using vk::DispatchLoaderDynamic;
#endif

DispatchLoaderDynamic & ggml_vk_default_dispatcher();

#define VULKAN_HPP_DEFAULT_DISPATCHER ggml_vk_default_dispatcher()

#include <vulkan/vulkan.hpp>

#ifndef VK_NV_cooperative_matrix_decode_vector
#define VK_NV_cooperative_matrix_decode_vector 1
#define VK_NV_COOPERATIVE_MATRIX_DECODE_VECTOR_EXTENSION_NAME "VK_NV_cooperative_matrix_decode_vector"
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_DECODE_VECTOR_FEATURES_NV ((VkStructureType)1000689000)
typedef struct VkPhysicalDeviceCooperativeMatrixDecodeVectorFeaturesNV {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           cooperativeMatrixDecodeVector;
} VkPhysicalDeviceCooperativeMatrixDecodeVectorFeaturesNV;
#endif

#if __has_include(<spirv/unified1/spirv.hpp>)
#    include <spirv/unified1/spirv.hpp>
#elif __has_include(<spirv-headers/spirv.hpp>)
#    include <spirv-headers/spirv.hpp>
#elif __has_include(<spirv.hpp>)
#    include <spirv.hpp>
#else
     // Fallback to let the compiler throw a standard "file not found" error
#    include <spirv/unified1/spirv.hpp>
#endif

#include <algorithm>

#include <cmath>

#include <iomanip>

#include <iostream>

#include <tuple>

#include <vector>

#include <deque>

#include <sstream>

#include <utility>

#include <memory>

#include <limits>

#include <map>

#include <set>

#include <unordered_map>

#include <shared_mutex>

#include <mutex>
#include <atomic>

#include <future>

#include <condition_variable>

#include <thread>

#if defined(_MSC_VER)
# define NOMINMAX 1
# include <windows.h>
# define YIELD() YieldProcessor()
#elif defined(__clang__) || defined(__GNUC__)
# if defined(__x86_64__) ||defined(__i386__)
#  include <immintrin.h>
#  define YIELD() _mm_pause()
# elif defined(__arm__) || defined(__aarch64__)
#  if defined(__clang__)
#   include <arm_acle.h>
#   define YIELD() __yield()
#  else
#   define YIELD() asm volatile("yield")
#  endif
# endif
#endif

#if !defined(YIELD)
#define YIELD()
#endif

#include "ggml-impl.h"

#include "ggml-backend-impl.h"

#include "ggml-vulkan-shaders.hpp"

#if !defined(VK_KHR_shader_bfloat16)

#define VK_KHR_shader_bfloat16 1
#define VK_KHR_SHADER_BFLOAT16_SPEC_VERSION                          1
#define VK_KHR_SHADER_BFLOAT16_EXTENSION_NAME                        "VK_KHR_shader_bfloat16"
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_BFLOAT16_FEATURES_KHR ((VkStructureType)1000141000)
#define VK_COMPONENT_TYPE_BFLOAT16_KHR                               ((VkComponentTypeKHR)1000141000)

typedef struct VkPhysicalDeviceShaderBfloat16FeaturesKHR {
    VkStructureType                       sType;
    void*                                 pNext;
    VkBool32                              shaderBFloat16Type;
    VkBool32                              shaderBFloat16DotProduct;
    VkBool32                              shaderBFloat16CooperativeMatrix;
} VkPhysicalDeviceShaderBfloat16FeaturesKHR;
#endif

#if !defined(VK_VALVE_shader_mixed_float_dot_product)
#define VK_VALVE_shader_mixed_float_dot_product 1
#define VK_VALVE_SHADER_MIXED_FLOAT_DOT_PRODUCT_SPEC_VERSION 1
#define VK_VALVE_SHADER_MIXED_FLOAT_DOT_PRODUCT_EXTENSION_NAME "VK_VALVE_shader_mixed_float_dot_product"
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MIXED_FLOAT_DOT_PRODUCT_FEATURES_VALVE ((VkStructureType)1000673000)
typedef struct VkPhysicalDeviceShaderMixedFloatDotProductFeaturesVALVE {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           shaderMixedFloatDotProductFloat16AccFloat32;
    VkBool32           shaderMixedFloatDotProductFloat16AccFloat16;
    VkBool32           shaderMixedFloatDotProductBFloat16Acc;
    VkBool32           shaderMixedFloatDotProductFloat8AccFloat32;
} VkPhysicalDeviceShaderMixedFloatDotProductFeaturesVALVE;
#endif

#if !defined(VK_EXT_shader_ocp_microscaling_types)
#define VK_EXT_shader_ocp_microscaling_types 1
#define VK_EXT_SHADER_OCP_MICROSCALING_TYPES_SPEC_VERSION 1
#define VK_EXT_SHADER_OCP_MICROSCALING_TYPES_EXTENSION_NAME "VK_EXT_shader_ocp_microscaling_types"
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OCP_MICROSCALING_TYPES_FEATURES_EXT ((VkStructureType)1000672000)
typedef struct VkPhysicalDeviceShaderOCPMicroscalingTypesFeaturesEXT {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           shaderFloat4;
    VkBool32           shaderFloat6;
    VkBool32           shaderFloat8UnsignedE8M0;
    VkBool32           shaderMXInt8;
} VkPhysicalDeviceShaderOCPMicroscalingTypesFeaturesEXT;
#endif

#if !defined(VK_EXT_shader_float8)
#define VK_EXT_shader_float8 1
#define VK_EXT_SHADER_FLOAT8_SPEC_VERSION 1
#define VK_EXT_SHADER_FLOAT8_EXTENSION_NAME "VK_EXT_shader_float8"
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT8_FEATURES_EXT ((VkStructureType)1000567000)
typedef struct VkPhysicalDeviceShaderFloat8FeaturesEXT {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           shaderFloat8;
    VkBool32           shaderFloat8CooperativeMatrix;
} VkPhysicalDeviceShaderFloat8FeaturesEXT;
#endif

#ifndef VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME
#define VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME "VK_KHR_internally_synchronized_queues"
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INTERNALLY_SYNCHRONIZED_QUEUES_FEATURES_KHR ((VkStructureType)1000504000)
#define VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR ((VkDeviceQueueCreateFlagBits)0x00000004)

// Compile-time constant guaranteed; no runtime initialization overhead
static constexpr vk::DeviceQueueCreateFlagBits eInternallySynchronizedKHR =
    static_cast<vk::DeviceQueueCreateFlagBits>(0x00000004);

typedef struct VkPhysicalDeviceInternallySynchronizedQueuesFeaturesKHR {
    VkStructureType    sType;
    void* pNext;
    VkBool32           internallySynchronizedQueues;
} VkPhysicalDeviceInternallySynchronizedQueuesFeaturesKHR;
#else
static constexpr vk::DeviceQueueCreateFlagBits eInternallySynchronizedKHR = vk::DeviceQueueCreateFlagBits::eInternallySynchronizedKHR;
#endif

#define ROUNDUP_POW2(M, N) (((M) + (N) - 1) & ~((N) - 1))

#define CEIL_DIV(M, N) (((M) / (N)) + (((M) % (N)) != 0))

static bool is_pow2(uint32_t x) { return x > 1 && (x & (x-1)) == 0; }

#define VK_VENDOR_ID_AMD 0x1002

#define VK_VENDOR_ID_APPLE 0x106b

#define VK_VENDOR_ID_INTEL 0x8086

#define VK_VENDOR_ID_NVIDIA 0x10de

#define VK_VENDOR_ID_QUALCOMM 0x5143

#define VK_DEVICE_DESCRIPTOR_POOL_SIZE 256

#define VK_CHECK(err, msg, dev)                                     \
    do {                                                            \
        vk::Result err_;                                            \
        try {                                                       \
            err_ = (err);                                           \
        } catch (vk::DeviceLostError &) {                           \
            ggml_vk_print_device_lost_info(dev);                    \
            GGML_LOG_ERROR("ggml_vulkan: %s at %s:%d\n",            \
                #err, __FILE__, __LINE__);                          \
            throw;                                                  \
        }                                                           \
        if (err_ != vk::Result::eSuccess) {                         \
            GGML_LOG_ERROR("ggml_vulkan: %s error %s at %s:%d\n",   \
                #err, to_string(err_).c_str(), __FILE__, __LINE__); \
            throw vk::SystemError(vk::make_error_code(err_),        \
                "ggml_vulkan: " msg);                               \
        }                                                           \
    } while (0)

#ifdef GGML_VULKAN_DEBUG
#define VK_LOG_DEBUG(msg) std::cerr << msg << std::endl
#else
#define VK_LOG_DEBUG(msg) ((void) 0)
#endif // GGML_VULKAN_DEBUG

#define MAX_PARAMETER_COUNT 12

#define MAX_FUSED_ADDS (MAX_PARAMETER_COUNT - 3)

struct vk_pipeline_struct;

typedef std::shared_ptr<struct vk_pipeline_struct> vk_pipeline;

struct vk_pipeline_struct {
    std::string name;
    vk::ShaderModule shader_module;
    vk::PipelineLayout layout;
    vk::Pipeline pipeline;
    uint32_t push_constant_size;
    uint32_t parameter_count;
    std::array<uint32_t, 3> wg_denoms;
    uint32_t align;
    // true if fields have been set by ggml_vk_create_pipeline
    bool initialized {};
    // true while a compile is in flight, used to dedupe concurrent claims.
    // Protected by device->compile_mutex.
    bool compile_pending {};
    // set to true when the shader has been compiled
    std::atomic<bool> compiled {};
    // number of registers used, extracted from pipeline executable properties
    uint32_t register_count {};

#if defined(VK_EXT_shader_64bit_indexing)
    bool is_64b_indexing {};
#endif
    // linked list of pipelines for multiple compilation variants.
    // currently only used to compile a 64-bit indexing variant.
    vk_pipeline next;
};

typedef std::weak_ptr<vk_pipeline_struct> vk_pipeline_ref;

struct vk_matmul_pipeline_key {
    ggml_type type_a;
    ggml_type type_b;
    bool mul_mat_id;
    bool f16acc;

    bool operator<(const vk_matmul_pipeline_key & o) const {
        return std::tie(type_a, type_b, mul_mat_id, f16acc)
             < std::tie(o.type_a, o.type_b, o.mul_mat_id, o.f16acc);
    }
};

struct vk_matmul_pipeline_pair {
    vk_pipeline unaligned;
    vk_pipeline aligned;
    uint32_t align;
};

struct vk_tile_config {
    std::vector<uint32_t> warptile;
    std::array<uint32_t, 3> wg_denoms;
    uint32_t align;
};

using matmul_tile_selector_t = std::function<uint32_t(
    uint32_t m, uint32_t n, uint32_t k, uint32_t shader_core_count,
    const std::vector<vk_matmul_pipeline_pair>& configs)>;

struct vk_device_struct;

typedef std::shared_ptr<vk_device_struct> vk_device;

typedef std::weak_ptr<vk_device_struct> vk_device_ref;

struct vk_buffer_struct;

typedef std::shared_ptr<vk_buffer_struct> vk_buffer;

typedef std::weak_ptr<vk_buffer_struct> vk_buffer_ref;

struct ggml_backend_vk_buffer_type_context {
    std::string name;
    vk_device device;
};

struct vk_command_buffer {
    vk::CommandBuffer buf;
    uint64_t use_counter = 0;
    bool in_use = false;
};

struct vk_queue;

struct vk_command_pool {
    void init(vk_device& device, vk_queue *q_);
    void destroy(vk::Device& device);

    vk::CommandPool pool;
    // Using deque so the pointers to command buffers
    // remain valid even if we add more
    std::deque<vk_command_buffer> cmd_buffers;

    vk_queue *q;

    size_t buffers_in_use() const {
        return std::count_if(cmd_buffers.begin(), cmd_buffers.end(),
            [](const auto& cb) { return cb.in_use; });
    }
};

struct vk_queue_handle {
    vk::Queue queue;
    vk_device_ref device;
    std::mutex * device_submit_mutex = nullptr;
    virtual void submit(vk::ArrayProxy<const vk::SubmitInfo> submits, vk::Fence fence) = 0;
    virtual void lock()   {}   // no-op by default (internally synchronized case)
    virtual void unlock() {}
    virtual ~vk_queue_handle() = default;
};

struct vk_queue_handle_synchronized : vk_queue_handle {
    std::mutex mutex;
    void submit(vk::ArrayProxy<const vk::SubmitInfo> submits, vk::Fence fence) override;

    void lock()   override { mutex.lock(); }
    void unlock() override { mutex.unlock(); }
};

struct vk_queue_handle_unsynchronized : vk_queue_handle {
    void submit(vk::ArrayProxy<const vk::SubmitInfo> submits, vk::Fence fence) override;

    // lock()/unlock() inherited no-ops
};

struct vk_queue {
    uint32_t queue_family_index;
    std::shared_ptr<vk_queue_handle> handle;

    vk_command_pool cmd_pool;

    vk::PipelineStageFlags stage_flags;

    bool transfer_only;
};

static constexpr uint32_t mul_mat_vec_max_cols = 8;

static constexpr uint32_t p021_max_gqa_ratio = 8;

// ArifiLabs lane-248 / R57: the mat-vec ADMIT width, made runtime-selectable (runtime-switch law,
// F-08: default = the inherited constant, no default moves).
//
// The decode multiplier on the drafted line is the cost of one extra VERIFY ROW, not the n=1 rate
// (R63 §3a: the marginal row is 43.0 ms = 0.177 plain-token-equivalents, and that number alone caps
// the line at 10.9 t/s with a perfect drafter at n-max 4). Deciding where that marginal cost is
// cheapest needs the SAME (shape, type, width) timed on BOTH paths, and n <= 8 could only ever take
// mat-vec: the GEMM path was unreachable below 9 columns, so the two regimes could never be
// compared at a width the served line actually uses. Lowering this value hands widths above it to
// ggml_vk_mul_mat_q_f16() out of one binary. GGML_ARIFI_MMV_MAX_COLS=0 puts EVERY width including
// n=1 on the GEMM path (the n=1 clause is gated by the same value, so the two regimes can be
// compared at the same anchor); 8 (the default) is byte-for-byte the inherited routing.
static uint32_t ggml_vk_mmv_max_cols() {
    static const uint32_t v = [] {
        const char * s = getenv("GGML_ARIFI_MMV_MAX_COLS");
        if (s == nullptr) {
            return mul_mat_vec_max_cols;
        }
        int x = atoi(s);
        x = std::max(0, std::min(x, (int) mul_mat_vec_max_cols));
        // Receipt line, emitted ONLY when the variable is set: its presence identifies a forced
        // arm in a paired sweep and its absence the inherited one, which is what lets a receipt
        // prove which route a cell took. Unconditional logging would be a behavioural change at
        // the default, which this switch must not have.
        GGML_LOG_INFO("ggml_vulkan: mat-vec admit width: %d (GGML_ARIFI_MMV_MAX_COLS)\n", x);
        return (uint32_t) x;
    }();
    return v;
}

// lane-230 / R46b commit D: the R47d S-X8 mat-vec sweep (lane-232) as a DEVICE-PROBED DEFAULT.
//
// R47d paired rows 1/2/4/8 against workgroups 64/128/256 over the four real 27B S-X8 decode
// shapes at n=1..8, two rounds each, and the optimum is NOT one global pair: the n=1 FFN
// orientations want rows=4/wg=256 while the 248320x5120 output wants rows=8/wg=64. So the rule is
// an exact (m, k, n) lookup with its own pipeline per cell, selected at dispatch. Every selected
// cell measured >= 0.97 of the probed default in BOTH rounds; rows=8/wg=256 was REFUSED by shared
// memory and never enters the table. Unmeasured shapes, n > 8 and every other device keep the
// probed default. Evidence: research/local-inference/lane-evidence/2026-09-11-lane-232-r47a-alloc-
// instrument/R47D-TRACES-SWEEP.md, section "Sweep selection".
//
// The sweep ran on lane-232's base (arifi/main 766cb9528), which does NOT carry R46's n <= 3
// whole-block decode. R46 replaces the decode math and the row-blocking loop below n <= 3 and
// leaves the reduction shape (tmpsh[NUM_COLS][NUM_ROWS][BLOCK_SIZE]) untouched, so the shared
// memory arithmetic still holds, but the n=1/2/3 OPTIMA are UNVERIFIED on this kernel until they
// are re-measured on the rebased stage.
enum vk_sx8_mmv_tune_variant {
    SX8_TUNE_ROWS1_WG256 = 0,
    SX8_TUNE_ROWS4_WG64,
    SX8_TUNE_ROWS4_WG128,
    SX8_TUNE_ROWS4_WG256,
    SX8_TUNE_ROWS8_WG64,
    SX8_TUNE_ROWS8_WG128,
    SX8_TUNE_VARIANT_COUNT,
};

static constexpr uint32_t vk_sx8_mmv_tune_rows[SX8_TUNE_VARIANT_COUNT] = { 1,   4,  4,   4,   8,  8   };
static constexpr uint32_t vk_sx8_mmv_tune_wg  [SX8_TUNE_VARIANT_COUNT] = { 256, 64, 128, 256, 64, 128 };

struct vk_sx8_mmv_tune_cell {
    uint32_t m;
    uint32_t k;
    uint32_t n;
    uint32_t variant;
};

// m is the output row count (ne20), k the row length (ne00) - the same orientation the R47d table
// prints as "m x k". 17408x5120 at n=7 is absent on purpose: the sweep's own winner there IS the
// probed default, so no variant pipeline is built for it.
static constexpr vk_sx8_mmv_tune_cell vk_sx8_mmv_tune_table[] = {
    {   4096, 14336, 1, SX8_TUNE_ROWS1_WG256 },
    {   4096, 14336, 2, SX8_TUNE_ROWS4_WG256 },
    {   4096, 14336, 3, SX8_TUNE_ROWS4_WG256 },
    {   4096, 14336, 4, SX8_TUNE_ROWS4_WG256 },
    {   4096, 14336, 5, SX8_TUNE_ROWS4_WG128 },
    {   4096, 14336, 6, SX8_TUNE_ROWS4_WG128 },
    {   4096, 14336, 7, SX8_TUNE_ROWS4_WG64  },
    {   4096, 14336, 8, SX8_TUNE_ROWS4_WG128 },
    {   5120, 17408, 1, SX8_TUNE_ROWS4_WG256 },
    {   5120, 17408, 2, SX8_TUNE_ROWS4_WG256 },
    {   5120, 17408, 3, SX8_TUNE_ROWS4_WG256 },
    {   5120, 17408, 4, SX8_TUNE_ROWS4_WG128 },
    {   5120, 17408, 5, SX8_TUNE_ROWS4_WG256 },
    {   5120, 17408, 6, SX8_TUNE_ROWS4_WG128 },
    {   5120, 17408, 7, SX8_TUNE_ROWS4_WG64  },
    {   5120, 17408, 8, SX8_TUNE_ROWS4_WG256 },
    {  17408,  5120, 1, SX8_TUNE_ROWS4_WG256 },
    {  17408,  5120, 2, SX8_TUNE_ROWS4_WG256 },
    {  17408,  5120, 3, SX8_TUNE_ROWS4_WG256 },
    {  17408,  5120, 4, SX8_TUNE_ROWS8_WG128 },
    {  17408,  5120, 5, SX8_TUNE_ROWS4_WG128 },
    {  17408,  5120, 6, SX8_TUNE_ROWS4_WG64  },
    {  17408,  5120, 8, SX8_TUNE_ROWS4_WG64  },
    { 248320,  5120, 1, SX8_TUNE_ROWS8_WG64  },
    { 248320,  5120, 2, SX8_TUNE_ROWS4_WG64  },
    { 248320,  5120, 3, SX8_TUNE_ROWS4_WG64  },
    { 248320,  5120, 4, SX8_TUNE_ROWS8_WG64  },
    { 248320,  5120, 5, SX8_TUNE_ROWS4_WG64  },
    { 248320,  5120, 6, SX8_TUNE_ROWS4_WG64  },
    { 248320,  5120, 7, SX8_TUNE_ROWS4_WG64  },
    { 248320,  5120, 8, SX8_TUNE_ROWS4_WG64  },
};

// -1 = no measured cell, keep the probed default.
static int ggml_vk_sx8_mmv_tune_lookup(uint32_t m, uint32_t k, uint32_t n) {
    for (const auto & cell : vk_sx8_mmv_tune_table) {
        if (cell.m == m && cell.k == k && cell.n == n) {
            return (int) cell.variant;
        }
    }
    return -1;
}

enum vk_device_architecture {
    OTHER,
    AMD_GCN,
    AMD_RDNA1,
    AMD_RDNA2,
    AMD_RDNA3,
    AMD_RDNA4,
    INTEL_XE1,
    INTEL_XE2,
    NVIDIA_PRE_TURING,
    NVIDIA_TURING,
    QUALCOMM_ADRENO,
};

enum vk_conv_shapes {
    CONV_SHAPE_128x128,
    CONV_SHAPE_64x32,
    CONV_SHAPE_32x256,
    CONV_SHAPE_64x128,
    CONV_SHAPE_COUNT,
};

struct vk_conv_block_size {
    uint32_t K;
    uint32_t NPQ;
    uint32_t CRS;
};

inline vk_conv_block_size vk_conv_block_sizes[CONV_SHAPE_COUNT] = {
    // K   NPQ  CRS
    { 128, 128, 16 }, // CONV_SHAPE_128x128
    {  64,  32, 32 }, // CONV_SHAPE_64x32
    {  32, 256, 16 }, // CONV_SHAPE_32x256
    {  64, 128, 16 }, // CONV_SHAPE_64x128
};

enum dmmv_wg_sizes {
    DMMV_WG_SIZE_SUBGROUP,
    DMMV_WG_SIZE_LARGE,
    DMMV_WG_SIZE_COUNT,
};

enum FaCodePath {
    FA_SCALAR,
    FA_COOPMAT1,
    FA_COOPMAT2,
};

struct vk_fa_pipeline_state {
    uint32_t HSK, HSV;
    uint32_t Br, Bc;
    uint32_t D_split, row_split;
    bool shmem_staging;
    FaCodePath path;
    uint32_t workgroup_size, subgroup_size;
    bool aligned;
    bool f32acc;
    uint32_t flags;
    uint32_t limit_occupancy_shmem;
    ggml_type k_type;
    ggml_type v_type;

    bool operator<(const vk_fa_pipeline_state &b) const {
        return std::tie(HSK, HSV, Br, Bc, D_split, row_split, shmem_staging, path, workgroup_size, subgroup_size, aligned, f32acc, flags, limit_occupancy_shmem, k_type, v_type) <
               std::tie(b.HSK, b.HSV, b.Br, b.Bc, b.D_split, b.row_split, b.shmem_staging, b.path, b.workgroup_size, b.subgroup_size, b.aligned, b.f32acc, b.flags, b.limit_occupancy_shmem, b.k_type, b.v_type);
    }
};

struct vk_conv2d_pipeline_state {
    vk_conv2d_pipeline_state(uint32_t s0, uint32_t s1, uint32_t p0, uint32_t p1, uint32_t d0, uint32_t d1, uint32_t KW, uint32_t KH, uint32_t aligned)
        : s0(s0), s1(s1), p0(p0), p1(p1), d0(d0), d1(d1), KW(KW), KH(KH), aligned(aligned) {}

    uint32_t s0, s1, p0, p1, d0, d1, KW, KH;
    // when set, shader can skip K/CRS/NPQ bounds checks and address clamps
    uint32_t aligned;

    bool operator<(const vk_conv2d_pipeline_state &b) const {
        return std::tie(s0, s1, p0, p1, d0, d1, KW, KH, aligned) <
               std::tie(b.s0, b.s1, b.p0, b.p1, b.d0, b.d1, b.KW, b.KH, b.aligned);
    }
};

struct vk_conv3d_pipeline_state {
    vk_conv3d_pipeline_state(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t p0, uint32_t p1, uint32_t p2,
                             uint32_t d0, uint32_t d1, uint32_t d2, uint32_t KW, uint32_t KH, uint32_t KD, uint32_t aligned)
        : s0(s0), s1(s1), s2(s2), p0(p0), p1(p1), p2(p2), d0(d0), d1(d1), d2(d2), KW(KW), KH(KH), KD(KD), aligned(aligned) {}

    uint32_t s0, s1, s2, p0, p1, p2, d0, d1, d2, KW, KH, KD;
    uint32_t aligned;

    bool operator<(const vk_conv3d_pipeline_state &b) const {
        return std::tie(s0, s1, s2, p0, p1, p2, d0, d1, d2, KW, KH, KD, aligned) <
               std::tie(b.s0, b.s1, b.s2, b.p0, b.p1, b.p2, b.d0, b.d1, b.d2, b.KW, b.KH, b.KD, b.aligned);
    }
};

struct vk_solve_tri_pipeline_state {
    vk_solve_tri_pipeline_state(uint32_t N, uint32_t K)
        : N(N), K(K) {}

    uint32_t N, K;

    bool operator<(const vk_solve_tri_pipeline_state &b) const {
        return std::tie(N, K) <
               std::tie(b.N, b.K);
    }
};

enum shader_reduction_mode {
    SHADER_REDUCTION_MODE_SHMEM,
    SHADER_REDUCTION_MODE_HYBRID,
    SHADER_REDUCTION_MODE_SUBGROUP,
    SHADER_REDUCTION_MODE_COUNT,
};

static constexpr uint32_t num_argsort_pipelines = 11;

static constexpr uint32_t num_topk_moe_pipelines = 10;

static constexpr uint32_t num_topk_pipelines = 11;

static constexpr std::initializer_list<ggml_op> topk_moe_early_softmax_norm{ GGML_OP_SOFT_MAX, GGML_OP_RESHAPE,  GGML_OP_ARGSORT,
                                                                             GGML_OP_VIEW,     GGML_OP_GET_ROWS, GGML_OP_RESHAPE,
                                                                             GGML_OP_SUM_ROWS, GGML_OP_CLAMP,    GGML_OP_DIV,
                                                                             GGML_OP_RESHAPE };

static constexpr std::initializer_list<ggml_op> topk_moe_sigmoid_norm_bias{ GGML_OP_UNARY,    GGML_OP_RESHAPE,  GGML_OP_ADD,
                                                                            GGML_OP_ARGSORT,  GGML_OP_VIEW,     GGML_OP_GET_ROWS,
                                                                            GGML_OP_RESHAPE,  GGML_OP_SUM_ROWS, GGML_OP_CLAMP,
                                                                            GGML_OP_DIV,      GGML_OP_RESHAPE };

static constexpr std::initializer_list<ggml_op> topk_moe_sqrt_softplus_norm_bias{ GGML_OP_UNARY,    GGML_OP_SQRT,
                                                                                  GGML_OP_RESHAPE,  GGML_OP_ADD,
                                                                                  GGML_OP_ARGSORT,  GGML_OP_VIEW,
                                                                                  GGML_OP_GET_ROWS, GGML_OP_RESHAPE,
                                                                                  GGML_OP_SUM_ROWS, GGML_OP_CLAMP,
                                                                                  GGML_OP_DIV,      GGML_OP_RESHAPE };

static constexpr std::initializer_list<ggml_op> topk_moe_early_softmax     { GGML_OP_SOFT_MAX, GGML_OP_RESHAPE,  GGML_OP_ARGSORT,
                                                                             GGML_OP_VIEW,     GGML_OP_GET_ROWS };

static constexpr std::initializer_list<ggml_op> topk_moe_late_softmax      { GGML_OP_ARGSORT,  GGML_OP_VIEW,
                                                                             GGML_OP_GET_ROWS, GGML_OP_RESHAPE,
                                                                             GGML_OP_SOFT_MAX, GGML_OP_RESHAPE };

static constexpr std::initializer_list<ggml_op> snake_pattern              { GGML_OP_MUL,      GGML_OP_SIN,
                                                                             GGML_OP_SQR,      GGML_OP_MUL,
                                                                             GGML_OP_ADD };

static constexpr std::initializer_list<ggml_op> topk_qsa_pattern { GGML_OP_GET_ROWS, GGML_OP_PERMUTE,
                                                                   GGML_OP_CONT,     GGML_OP_CPY,
                                                                   GGML_OP_RESHAPE,  GGML_OP_ADD,
                                                                   GGML_OP_TOP_K };

static constexpr std::initializer_list<std::array<int, 3>> topk_qsa_edges {
    { 1, 0, 0 }, // permute->src[0] == get_rows
    { 2, 0, 1 }, // cont->src[0]    == permute
    { 4, 0, 3 }, // reshape->src[0] == cpy (mask cast)
    { 5, 0, 2 }, // add->src[0]     == cont
    { 5, 1, 4 }, // add->src[1]     == reshape
    { 6, 0, 5 }, // top_k->src[0]   == add
};

static constexpr std::initializer_list<ggml_op> rms_norm_mul_add_mul_pattern { GGML_OP_RMS_NORM, GGML_OP_MUL, GGML_OP_ADD, GGML_OP_MUL };

static constexpr std::initializer_list<ggml_op> rms_norm_mul_add_pattern     { GGML_OP_RMS_NORM, GGML_OP_MUL, GGML_OP_ADD };

static constexpr std::initializer_list<ggml_op> rms_norm_mul_rope_view_set_rows_pattern { GGML_OP_RMS_NORM, GGML_OP_MUL, GGML_OP_ROPE, GGML_OP_VIEW, GGML_OP_SET_ROWS };

static constexpr std::initializer_list<ggml_op> rms_norm_view_set_rows_pattern { GGML_OP_RMS_NORM, GGML_OP_VIEW, GGML_OP_SET_ROWS };

static constexpr std::initializer_list<ggml_op> rope_view_set_rows_pattern { GGML_OP_ROPE, GGML_OP_VIEW, GGML_OP_SET_ROWS };

static constexpr std::initializer_list<std::array<int, 3>> topk_moe_early_softmax_norm_edges {
    { 1, 0, 0 }, // reshape->src[0]  == softmax
    { 2, 0, 0 }, // argsort->src[0]  == softmax
    { 3, 0, 2 }, // view->src[0]     == argsort
    { 4, 0, 1 }, // get_rows->src[0] == reshape
    { 4, 1, 3 }, // get_rows->src[1] == view
    { 5, 0, 4 }, // reshape->src[0]  == get_rows
    { 6, 0, 5 }, // sum_rows->src[0] == reshape
    { 7, 0, 6 }, // clamp->src[0]    == sum_rows
    { 8, 0, 5 }, // div->src[0]      == reshape
    { 8, 1, 7 }, // div->src[1]      == clamp
    { 9, 0, 8 }, // reshape->src[0]  == div
};

static constexpr std::initializer_list<std::array<int, 3>> topk_moe_sigmoid_norm_bias_edges {
    { 1, 0, 0 }, // reshape->src[0]  == sigmoid
    { 2, 0, 0 }, // add->src[0]      == sigmoid
    { 3, 0, 2 }, // argsort->src[0]  == add
    { 4, 0, 3 }, // view->src[0]     == argsort
    { 5, 0, 1 }, // get_rows->src[0] == reshape
    { 5, 1, 4 }, // get_rows->src[1] == view
    { 6, 0, 5 }, // reshape->src[0]  == get_rows
    { 7, 0, 6 }, // sum_rows->src[0] == reshape
    { 8, 0, 7 }, // clamp->src[0]    == sum_rows
    { 9, 0, 6 }, // div->src[0]      == reshape
    { 9, 1, 8 }, // div->src[1]      == clamp
    {10, 0, 9 }, // reshape->src[0]  == div
};

static constexpr std::initializer_list<std::array<int, 3>> topk_moe_sqrt_softplus_norm_bias_edges {
    { 1, 0, 0 }, // sqrt->src[0]     == softplus
    { 2, 0, 1 }, // reshape->src[0]  == sqrt
    { 3, 0, 1 }, // add->src[0]      == sqrt
    { 4, 0, 3 }, // argsort->src[0]  == add
    { 5, 0, 4 }, // view->src[0]     == argsort
    { 6, 0, 2 }, // get_rows->src[0] == reshape
    { 6, 1, 5 }, // get_rows->src[1] == view
    { 7, 0, 6 }, // reshape->src[0]  == get_rows
    { 8, 0, 7 }, // sum_rows->src[0] == reshape
    { 9, 0, 8 }, // clamp->src[0]    == sum_rows
    {10, 0, 7 }, // div->src[0]      == reshape
    {10, 1, 9 }, // div->src[1]      == clamp
    {11, 0,10 }, // reshape->src[0]  == div
};

static constexpr std::initializer_list<std::array<int, 3>> topk_moe_early_softmax_edges {
    { 1, 0, 0 }, // reshape->src[0]  == softmax
    { 2, 0, 0 }, // argsort->src[0]  == softmax
    { 3, 0, 2 }, // view->src[0]     == argsort
    { 4, 0, 1 }, // get_rows->src[0] == reshape
    { 4, 1, 3 }, // get_rows->src[1] == view
};

static constexpr std::initializer_list<std::array<int, 3>> topk_moe_late_softmax_edges {
    { 1, 0, 0 }, // view->src[0]     == argsort
    { 2, 1, 1 }, // get_rows->src[1] == view
    { 3, 0, 2 }, // reshape->src[0]  == get_rows
    { 4, 0, 3 }, // soft_max->src[0] == reshape
    { 5, 0, 4 }, // reshape->src[0]  == soft_max
};

enum topk_moe_mode {
    TOPK_MOE_EARLY_SOFTMAX,
    TOPK_MOE_EARLY_SOFTMAX_NORM,
    TOPK_MOE_LATE_SOFTMAX,
    TOPK_MOE_SIGMOID_NORM_BIAS,
    TOPK_MOE_SQRT_SOFTPLUS_NORM_BIAS,
    TOPK_MOE_COUNT,
};

enum rms_norm_mode {
    RMS_NORM_MUL,
    RMS_NORM_MUL_ADD,
    RMS_NORM_MUL_ADD_MUL,
    RMS_NORM_MUL_ROPE,
    RMS_NORM_MUL_ROPE_VIEW_SET_ROWS,
    RMS_NORM_VIEW_SET_ROWS,
    RMS_NORM_COUNT,
};

static constexpr std::initializer_list<std::array<int, 3>> rope_view_set_rows_edges {
    { 1, 0, 0 }, // view->src[0]     == rope
    { 2, 0, 1 }, // set_rows->src[0] == view
};

static constexpr std::initializer_list<std::array<int, 3>> rms_norm_mul_rope_view_set_rows_edges {
    { 1, 0, 0 }, // mul->src[0]      == rms
    { 2, 0, 1 }, // rope->src[0]     == mul
    { 3, 0, 2 }, // view->src[0]     == rope
    { 4, 0, 3 }, // set_rows->src[0] == view
};

static constexpr std::initializer_list<std::array<int, 3>> rms_norm_view_set_rows_edges {
    { 1, 0, 0 }, // view->src[0]     == rms_norm
    { 2, 0, 1 }, // set_rows->src[0] == view
};

static constexpr std::array<ggml_type, 9> lightning_indexer_k_types = {
    GGML_TYPE_F32,
    GGML_TYPE_F16,
    GGML_TYPE_BF16,
    GGML_TYPE_Q8_0,
    GGML_TYPE_Q5_1,
    GGML_TYPE_Q5_0,
    GGML_TYPE_Q4_1,
    GGML_TYPE_Q4_0,
    GGML_TYPE_IQ4_NL,
};

class vk_memory_logger;

// lane-230 / R46b B7a: cumulative per-memory-heap reservation ledger.
//
// Before this, ggml_vk_find_memory_properties() accepted a memory type whenever the WHOLE heap was
// at least as large as the ONE allocation being made, so N concurrent allocations could each be
// admitted against the same bytes and the (N+1)-th allocation or the first pinned upload died in the
// driver. The ledger keeps the requirement bytes this process has already reserved per heap and
// admits a candidate only if reserved + requirement still fits the heap budget.
//
// Keying is by Vulkan heapIndex. That is the Vulkan abstraction and NOT a claim about a physical
// memory channel, bank or link; two memory types that report the same heapIndex share one counter
// because Vulkan says they draw on the same pool, and nothing here infers hardware topology.
//
// Budget in B7a == the heap's reported size. No env knob, no headroom factor, and no planning at
// all: B7b adds per-buffer-type batch planning on top of this ledger, and loader-wide planning is
// still open. Neither is implemented here.
//
// ponytail: this counts the same bytes the R47a trace's heap_live already counts, deliberately.
// They are not the same number (heap_live drops at ggml_vk_destroy_buffer and never sees a buffer
// freed by plain shared_ptr death; this ledger drops at ~vk_buffer_struct and never counts an
// imported host pointer), and the difference is what proves the ledger is wired to real buffer
// lifetime. Collapse the two only when the trace counter is fixed to follow buffer destruction.
#ifndef VK_MAX_MEMORY_HEAPS
#define VK_MAX_MEMORY_HEAPS 16
#endif

// R46b B7b: one placement option for one planned buffer. `type` is a Vulkan memory TYPE index and
// `heap` is the heap that type lives on - they are deliberately separate fields, because several
// types share one heap and the budget belongs to the heap, never to the type.
struct vk_plan_candidate {
    uint32_t type  = 0;
    uint32_t heap  = 0;
    uint64_t bytes = 0;   // real VkMemoryRequirements::size for this buffer, not the requested size
    // R46b B7c: this placement spills the buffer onto memory that is NOT DEVICE_LOCAL. The property
    // belongs to the memory TYPE, not to the heap: a heap can carry both a DEVICE_LOCAL and a
    // host-only type (R47d's small combined-property heap 2 is exactly that), so classifying by heap
    // would mislabel one of them.
    bool     host_split = false;
};

// Bound on the backtracking search. Hitting it means "did not finish deciding", which is reported
// as INDETERMINATE and never as a refusal: "I did not finish" and "no assignment exists" are
// different facts and the planner must not conflate them. What the CALLER then does is caller
// policy, not this backend's to assert (ggml-alloc.c refuses a multi-buffer batch on INDETERMINATE
// and lets a single-buffer one fall through). Same reasoning as the bulk-large-heap policy
// fallback: a planner that reports a refusal it did not actually prove is worse than one that
// admits it stopped searching.
#define VK_PLAN_MAX_SEARCH_NODES 1000000

enum vk_plan_result {
    VK_PLAN_FEASIBLE      = 0,
    VK_PLAN_INFEASIBLE    = 1,
    VK_PLAN_INDETERMINATE = 2,
};

class vk_heap_ledger {
public:
    // Admit `bytes` against `heap` if reserved + bytes <= budget. The subtraction form is
    // overflow-proof by construction: no sum is ever formed, so a huge `bytes` refuses instead of
    // wrapping. Fails closed on an out-of-range heap.
    bool reserve(uint32_t heap, uint64_t bytes, uint64_t budget) {
        if (heap >= VK_MAX_MEMORY_HEAPS) {
            return false;
        }
        std::lock_guard<std::mutex> guard(mutex);
        if (reserved_bytes[heap] > budget || bytes > budget - reserved_bytes[heap]) {
            return false;
        }
        reserved_bytes[heap] += bytes;
        return true;
    }

    // Returns false and leaves the counter UNTOUCHED on underflow (a double release or a release of
    // bytes never reserved). Silently clamping would hide the bug and corrupt every later decision.
    bool release(uint32_t heap, uint64_t bytes) {
        if (heap >= VK_MAX_MEMORY_HEAPS) {
            return false;
        }
        std::lock_guard<std::mutex> guard(mutex);
        if (bytes > reserved_bytes[heap]) {
            return false;
        }
        reserved_bytes[heap] -= bytes;
        return true;
    }

    // Advisory: used by candidate SELECTION. reserve() is the authority, so a racing reservation
    // between would_fit() and reserve() just means the candidate is refused at reserve() time.
    bool would_fit(uint32_t heap, uint64_t bytes, uint64_t budget) const {
        if (heap >= VK_MAX_MEMORY_HEAPS) {
            return false;
        }
        std::lock_guard<std::mutex> guard(mutex);
        return reserved_bytes[heap] <= budget && bytes <= budget - reserved_bytes[heap];
    }

    uint64_t reserved(uint32_t heap) const {
        if (heap >= VK_MAX_MEMORY_HEAPS) {
            return 0;
        }
        std::lock_guard<std::mutex> guard(mutex);
        return reserved_bytes[heap];
    }

    // R46b B7b — BATCH reservation: the complete set of buffers handed to THIS call, all or nothing.
    //
    // `cands[i]` is the ordered candidate list for planned buffer i, in the placement policy's own
    // preference order. Searches for a COMPLETE assignment (every buffer in the batch placed) and
    // commits all of it, or commits nothing. Search and commit both happen under the one ledger
    // mutex, so two concurrent plans on the same device can never both be admitted against the same
    // bytes, and a single-buffer reserve() cannot slip in between deciding and charging.
    //
    // The atomicity is over the batch, NOT over a whole model load: the caller hands one
    // (buffer type, context) group per call (scope: see ggml-backend-impl.h).
    //
    // Greedy first-choice is not enough: a buffer whose only option is heap H must be able to push
    // an earlier buffer off H onto its second choice. The walk therefore backtracks.
    //
    // Returns VK_PLAN_FEASIBLE (out_choice[i] = index into cands[i]), VK_PLAN_INFEASIBLE (no
    // complete assignment exists) or VK_PLAN_INDETERMINATE (search bound hit; decide nothing).
    //
    // R46b B7c: `host_split_max` caps the TOTAL bytes this one batch may place on candidates marked
    // host_split (not DEVICE_LOCAL). UINT64_MAX means "no cap of my own"; the per-heap budget check
    // below still applies and is unchanged. 0 forbids any host placement. `out_host_bytes` reports
    // how many bytes the committed assignment actually put on host memory (0 when nothing spilled).
    vk_plan_result reserve_plan(const std::vector<std::vector<vk_plan_candidate>> & cands,
                                const uint64_t * budgets, size_t n_budgets,
                                std::vector<size_t> & out_choice,
                                uint64_t host_split_max = UINT64_MAX,
                                uint64_t * out_host_bytes = nullptr) {
        const size_t n = cands.size();
        out_choice.assign(n, 0);
        if (out_host_bytes != nullptr) {
            *out_host_bytes = 0;
        }
        if (n == 0) {
            return VK_PLAN_FEASIBLE;
        }

        // Symmetry breaking. A real load hands us dozens of buffers with identical requirements and
        // identical candidate lists; those are interchangeable, so any feasible assignment can be
        // permuted into one where their choices never decrease. Restricting the walk to that
        // canonical order collapses what is otherwise a combinatorial explosion (35 identical
        // buffers over 2 heaps is 2^35 arrangements of the same few real outcomes) without
        // discarding any distinct outcome.
        std::vector<bool> same_as_prev(n, false);
        for (size_t k = 1; k < n; ++k) {
            if (cands[k].size() != cands[k - 1].size()) {
                continue;
            }
            bool same = true;
            for (size_t c = 0; c < cands[k].size() && same; ++c) {
                // R46b B7c: host_split is part of what makes two candidates interchangeable. Two
                // types on ONE heap can differ in DEVICE_LOCAL, so comparing heap+bytes alone would
                // call them identical and the non-decreasing-choice restriction could then prune the
                // only assignment that fits under host_split_max - a refusal the planner did not
                // prove.
                same = cands[k][c].heap       == cands[k - 1][c].heap  &&
                       cands[k][c].bytes      == cands[k - 1][c].bytes &&
                       cands[k][c].host_split == cands[k - 1][c].host_split;
            }
            same_as_prev[k] = same;
        }

        std::lock_guard<std::mutex> guard(mutex);

        // Necessary condition, decided in O(n) with no search at all: every item must land on SOME
        // heap, so if the total requirement exceeds the total FREE capacity across all heaps, no
        // complete assignment can exist. This is genuine infeasibility, not a guess.
        //
        // It matters because it covers the overwhelmingly common case - the model is simply bigger
        // than memory - which is exactly the shape the exhaustive walk is worst at: dozens of
        // differently sized buffers are not interchangeable, so the symmetry break above cannot
        // collapse them and the walk would hit its bound and answer INDETERMINATE - an undecided
        // answer on exactly the case that is in fact decidable in O(n). Answering it here keeps the
        // common "model is bigger than memory" shape a proven refusal instead of an admission that
        // the planner gave up.
        {
            uint64_t free_total = 0;
            for (uint32_t h = 0; h < VK_MAX_MEMORY_HEAPS && h < n_budgets; ++h) {
                if (reserved_bytes[h] >= budgets[h]) {
                    continue;
                }
                const uint64_t heap_free = budgets[h] - reserved_bytes[h];
                free_total = heap_free > UINT64_MAX - free_total ? UINT64_MAX : free_total + heap_free;
            }

            uint64_t need = 0;
            for (const auto & item : cands) {
                // the smallest this item can possibly consume anywhere
                uint64_t least = 0;
                for (size_t c = 0; c < item.size(); ++c) {
                    if (c == 0 || item[c].bytes < least) {
                        least = item[c].bytes;
                    }
                }
                if (least > UINT64_MAX - need) {
                    return VK_PLAN_INFEASIBLE;   // the total wraps: far past any real budget
                }
                need += least;
            }

            if (need > free_total) {
                return VK_PLAN_INFEASIBLE;
            }
        }

        uint64_t delta[VK_MAX_MEMORY_HEAPS] = {};
        std::vector<size_t> next(n, 0);
        uint64_t nodes      = 0;
        uint64_t host_used  = 0;   // R46b B7c: bytes this walk has placed on host_split candidates
        size_t   i          = 0;

        while (i < n) {
            if (++nodes > VK_PLAN_MAX_SEARCH_NODES) {
                return VK_PLAN_INDETERMINATE;
            }

            bool placed = false;
            for (size_t c = next[i]; c < cands[i].size(); ++c) {
                const vk_plan_candidate & cand = cands[i][c];
                if (cand.heap >= VK_MAX_MEMORY_HEAPS || cand.heap >= n_budgets) {
                    continue;   // fail closed on an out-of-range heap, exactly like reserve()
                }
                const uint64_t budget = budgets[cand.heap];
                // Subtraction form only: no sum is ever formed, so nothing can wrap. Already-live
                // reservations from other buffers are charged first, then this plan's own delta.
                if (reserved_bytes[cand.heap] > budget) {
                    continue;
                }
                const uint64_t after_live = budget - reserved_bytes[cand.heap];
                if (delta[cand.heap] > after_live) {
                    continue;
                }
                if (cand.bytes > after_live - delta[cand.heap]) {
                    continue;
                }
                // R46b B7c: the declared bound on host placement, in the same subtraction form so
                // nothing can wrap. A candidate that would take the batch past the bound is skipped
                // exactly like one that does not fit its heap, so the walk keeps looking (and
                // backtracks) instead of refusing.
                if (cand.host_split && (host_used > host_split_max || cand.bytes > host_split_max - host_used)) {
                    continue;
                }
                delta[cand.heap] += cand.bytes;
                if (cand.host_split) {
                    host_used += cand.bytes;
                }
                out_choice[i] = c;
                next[i]       = c + 1;   // resume after this choice if we ever backtrack into it
                placed        = true;
                break;
            }

            if (placed) {
                i++;
                if (i < n) {
                    // Interchangeable with its predecessor: start where the predecessor landed.
                    next[i] = same_as_prev[i] ? out_choice[i - 1] : 0;
                }
                continue;
            }
            if (i == 0) {
                return VK_PLAN_INFEASIBLE;
            }
            i--;
            const vk_plan_candidate & undo = cands[i][out_choice[i]];
            delta[undo.heap] -= undo.bytes;
            if (undo.host_split) {
                host_used -= undo.bytes;
            }
        }

        // Every delta was validated against its budget above, so this cannot overflow.
        for (uint32_t h = 0; h < VK_MAX_MEMORY_HEAPS; ++h) {
            reserved_bytes[h] += delta[h];
        }
        if (out_host_bytes != nullptr) {
            *out_host_bytes = host_used;
        }
        return VK_PLAN_FEASIBLE;
    }

#ifdef GGML_VK_INTERNAL_TESTS
    // R46b B7a-R2 (finding 2): reset() and total_reserved() exist ONLY in an internal-test build.
    // A production-linked build has no way to zero a live ledger, and the internal-test build only
    // ever resets the test's OWN ledger instance (vk_heapres_test_ledger()), never a device's.
    void reset() {
        std::lock_guard<std::mutex> guard(mutex);
        for (uint32_t h = 0; h < VK_MAX_MEMORY_HEAPS; ++h) {
            reserved_bytes[h] = 0;
        }
    }

    uint64_t total_reserved() const {
        std::lock_guard<std::mutex> guard(mutex);
        uint64_t total = 0;
        for (uint32_t h = 0; h < VK_MAX_MEMORY_HEAPS; ++h) {
            total += reserved_bytes[h];
        }
        return total;
    }
#endif // GGML_VK_INTERNAL_TESTS

private:
    mutable std::mutex mutex;
    uint64_t reserved_bytes[VK_MAX_MEMORY_HEAPS] = {};
};

// R46b B7c: the per-device STAGING RESERVE. Bytes carved out of the host-visible heap in the B7a
// ledger BEFORE any bulk weight buffer can be admitted, so a bounded host split cannot starve upload
// staging (sync_staging), prealloc scratch or the compute buffers that come after the load.
//
// It is a plain charge against the ledger, not a Vulkan allocation: nothing is allocated until the
// staging buffer is really needed, and when it is, its own reservation is taken on top. The reserve
// is therefore a floor the weights cannot eat, and it is deliberately NOT consumed by the real
// staging allocation - releasing it early would give the floor back to whatever allocates next.
//
// release() is exact-once by construction: it zeroes `bytes` before returning, so a second call
// releases nothing. That matters because the ledger refuses (rather than clamps) an over-release,
// and a double release that happened to be covered by someone else's live bytes would silently steal
// them.
struct vk_staging_reserve {
    vk_heap_ledger * ledger = nullptr;
    uint32_t         heap   = UINT32_MAX;
    uint64_t         bytes  = 0;

    bool try_reserve(vk_heap_ledger * l, uint32_t h, uint64_t b, uint64_t budget) {
        GGML_ASSERT(bytes == 0);
        if (l == nullptr || b == 0 || !l->reserve(h, b, budget)) {
            return false;
        }
        ledger = l;
        heap   = h;
        bytes  = b;
        return true;
    }

    bool release() {
        if (bytes == 0) {
            return false;
        }
        const uint64_t b = bytes;
        bytes = 0;
        return ledger->release(heap, b);
    }
};

// R46b B7a-R2 (finding 4): test-only fault injection into ggml_vk_create_buffer(), so the
// post-allocation cleanup paths (map / bind / buffer-device-address) are exercised through the
// PRODUCTION allocator instead of a helper. In a normal build the macro expands to nothing and
// neither the stage variable nor the enum exists.
#ifdef GGML_VK_INTERNAL_TESTS
enum vk_test_fault_stage {
    VK_TEST_FAULT_NONE        = 0,
    VK_TEST_FAULT_AFTER_ALLOC = 1,
    VK_TEST_FAULT_AFTER_MAP   = 2,
    VK_TEST_FAULT_AFTER_BIND  = 3,
    VK_TEST_FAULT_AFTER_BDA   = 4,
};

static std::atomic<int> vk_test_fault_stage_active { VK_TEST_FAULT_NONE };
static std::atomic<int> vk_test_fault_mapped       { 0 };

// R46b B7a-R3 (finding 2): witness that device->device.getBufferAddress() REALLY ran. The stage-4
// fault fires after the `if (device->buffer_device_address)` block whether or not that block was
// entered, so without this witness a device with no BDA support reports "after getBufferAddress"
// green while never touching BDA.
static std::atomic<int> vk_test_fault_bda { 0 };

// R46b B7a-R3 (finding 1): ordering witness for ~vk_buffer_struct(). Sampled at the instant the
// DRIVER memory is freed; the ledger must still be counting the buffer's bytes at that point.
// Armed only around the one destruction the probe is measuring, because device/instance teardown
// destroys buffers of its own that would otherwise clobber the sample.
static std::atomic<int>      vk_test_destroy_witness_armed { 0 };
static std::atomic<uint64_t> vk_test_reserved_at_driver_free { 0 };

// R46b B7b: counts every vkAllocateMemory THIS process asked for, incremented immediately before
// the call at all three call sites. A NULL return from a refused load proves nothing about "failed
// before the first driver allocation"; this counter does.
static std::atomic<uint64_t> vk_test_driver_alloc_calls { 0 };

// R46b B7b: let the first `vk_test_fault_skip` buffers through before the fault fires. A batch
// transaction can only be tested by failing PART WAY through it.
static std::atomic<int> vk_test_fault_skip { 0 };

#define VK_TEST_FAULT(stage)                                                              \
    do {                                                                                  \
        if (vk_test_fault_stage_active.load(std::memory_order_relaxed) == (int) (stage)) { \
            if (vk_test_fault_skip.fetch_sub(1, std::memory_order_relaxed) <= 0) {         \
                throw vk::DeviceLostError("R46b B7a-R2 injected fault");                   \
            }                                                                              \
        }                                                                                 \
    } while (0)
#define VK_TEST_NOTE_DRIVER_ALLOC() vk_test_driver_alloc_calls.fetch_add(1, std::memory_order_relaxed)
#define VK_TEST_NOTE_MAPPED() vk_test_fault_mapped.store(1, std::memory_order_relaxed)
#define VK_TEST_NOTE_BDA()    vk_test_fault_bda.store(1, std::memory_order_relaxed)
#define VK_TEST_NOTE_DRIVER_FREE(dev)                                                     \
    do {                                                                                  \
        if (vk_test_destroy_witness_armed.load(std::memory_order_acquire)) {              \
            vk_test_reserved_at_driver_free.store((dev)->heap_ledger.total_reserved(),    \
                                                  std::memory_order_release);             \
        }                                                                                 \
    } while (0)
#else
#define VK_TEST_FAULT(stage)          do { } while (0)
#define VK_TEST_NOTE_DRIVER_ALLOC()   do { } while (0)
#define VK_TEST_NOTE_MAPPED()         do { } while (0)
#define VK_TEST_NOTE_BDA()            do { } while (0)
#define VK_TEST_NOTE_DRIVER_FREE(dev) do { } while (0)
#endif // GGML_VK_INTERNAL_TESTS

struct vk_device_struct {
    std::recursive_mutex mutex;
    std::mutex queue_submit_mutex;
    mutable std::shared_mutex pinned_memory_mutex;

    // R46b B7a-R2 (finding 3): the reservation ledger is a MEMBER of the device, not a process
    // singleton keyed by heap index. Vulkan heap indices are scoped to a physical device, so two
    // GPUs in one process both have heap 0/1; a singleton would collide their independent budgets
    // and release across devices. Making it a member also ties the ledger's lifetime exactly to
    // the device's, so no stale identity (a recycled VkDevice handle or pointer) can alias it.
    vk_heap_ledger heap_ledger;

    // R46b B7c: charged once during device creation, released once in ~vk_device_struct.
    vk_staging_reserve staging_reserve;

    // Guards compile_pending, all_pipelines, and the dynamic pipeline maps
    // (flash_attn, fa_mask_opt, solve_tri, conv2d, etc). The actual compile
    // runs with no lock held, so different pipelines can compile in parallel.
    // Lock order is device->mutex -> compile_mutex, never the reverse.
    std::mutex compile_mutex;
    std::condition_variable compile_cv;

    uint32_t debug_cmdbuf_idx {};

    vk::PhysicalDevice physical_device;
    vk::PhysicalDeviceProperties properties;
    std::string name;
    uint64_t max_memory_allocation_size;
    uint64_t max_buffer_size;
    uint64_t suballocation_block_size;
    uint64_t min_imported_host_pointer_alignment;
    bool external_memory_host {};
    bool fp16;
    bool bf16;
    bool pipeline_robustness;
    bool memory_priority;
    vk::Device device;
    uint32_t vendor_id;
    vk::DriverId driver_id;
    vk_device_architecture architecture;
    std::unique_ptr<vk_queue> compute_queue;
    std::unique_ptr<vk_queue> transfer_queue;
    bool single_queue;
    bool support_async;
    bool async_use_transfer_queue;
    bool has_internally_synchronized_queues = false;
    uint32_t subgroup_size;
    uint32_t subgroup_size_log2;
    uint32_t shader_core_count;
    bool uma;
    bool prefer_host_memory;
    bool float_controls_rte_fp16;
    bool float_controls_denorm_preserve_fp16;
    bool subgroup_basic;
    bool subgroup_arithmetic;
    bool subgroup_shuffle;
    bool subgroup_ballot;
    bool subgroup_clustered;
    bool subgroup_vote;
    bool multi_add;
    bool shader_int64;
    bool buffer_device_address;

    // R46b B7b: VK_KHR_maintenance4 is enabled for this device. It provides
    // getBufferMemoryRequirementsKHR(VkDeviceBufferMemoryRequirements), which answers the batch
    // planner's "what would this buffer actually require" question with no VkBuffer object at all.
    bool maintenance4 = false;
    bool vulkan_memory_model;

    bool add_rms_fusion;
    uint32_t partials_binding_alignment;
    uint32_t max_nodes_per_submit;

    bool shader_64b_indexing;

    bool integer_dot_product;
    // 0: default, 1: force mmvq, -1: disable mmvq
    int32_t mmvq_mode;

    // arifi lane-235 / R48b: MMVQ A-side hoist state, probed ONCE at device creation because both
    // the pipelines (specialization constant 3) and the routing (ggml_vk_should_use_mmvq) read it,
    // and ggml_vk_load_shaders() is also called lazily for a single requested pipeline.
    // mmvq_a_hoist is the device-level gate; the per-(type, NUM_COLS) narrowing is applied where
    // the pipelines are created. mmvq_route_legacy=1 keeps the hoisted nest but restores the
    // pre-R48b routing rules.
    uint32_t mmvq_a_hoist;
    uint32_t mmvq_a_hoist_iq1;
    bool     mmvq_route_legacy;

    // arifi lane-252 / R64: Q5_K n>1 route arm. 0 = legacy (the shipped f32 dequant route, the
    // default), 1 = route (admit Q5_K to the A-hoisted q8_1 MMVQ path at n=5..8, k <= 8192).
    // Probed once at device creation next to mmvq_a_hoist, for the same reason.
    uint32_t q5k_mmvq_route;
    // arifi lane-256 / R71: the Q6_K q8_1 MMVQ admit. 0 = legacy (the shipped f32 dequant route
    // at every width), 1 = the measured admit. DEFAULT 0 ON EVERY DEVICE -- the win is real but
    // UNCLEARED, see ggml_vk_should_use_mmvq_impl.
    uint32_t q6k_mmvq_route;
    bool q6k_mmvq_n6_rows1 = false;  // lane-270 / R74b: set in ggml_vk_load_shaders
    // arifi lane-262 / R75: the IQ4_XS q8_1 MMVQ arm. 0 = legacy (mul_mat_vec_iq4_xs f32 dequant
    // at every width), 1 = route (the measured admit in ggml_vk_should_use_mmvq_impl),
    // 2 = all (decider diagnostic: MMVQ at every n and k, MUL_MAT and MUL_MAT_ID).
    uint32_t iq4xs_mmvq_route;
    // arifi lane-253 / R65: the NUM_COLS index (i = NUM_COLS-1) at and above which rm_int_n() hands
    // the q8_1 MMVQ pipelines the static 4-row shape. 4 = the inherited RDNA3 rule, untouched.
    // Probed once here for the same reason as mmvq_a_hoist: pipeline creation is entered lazily.
    uint32_t mmvq_wide_rows_from;
    // arifi lane-271 / R85: Q4_K q8_1 MMVQ at NUM_COLS >= 5 only. q4k_w5_overlap = spec constant 4
    // of mul_mat_vecq.comp (0 = shipped nest, 1 = next-slice prefetch, 3 = 4+1 column split);
    // q4k_w5_rows = rows per workgroup (0 = the inherited rm_int_n value). Both 0 by default.
    uint32_t q4k_w5_overlap;
    uint32_t q4k_w5_rows;
    // arifi lane-271 / R85: the shipped cell switch. 1 = route the measured-winning (m, n) cells to
    // pipeline_mul_mat_vec_q4k_q8_1_split. Device probe: ON on AMD RDNA3; GGML_ARIFI_Q4K_W5_SPLIT=0|1.
    uint32_t q4k_w5_split;

    bool subgroup_size_control;
    uint32_t subgroup_min_size;
    uint32_t subgroup_max_size;
    bool subgroup_require_full_support;

    // floor(log2(maxComputeWorkGroupInvocations))
    uint32_t max_workgroup_size_log2 {};

    bool coopmat_support;
    bool coopmat_acc_f32_support {};
    bool coopmat_acc_f16_support {};
    bool coopmat_bf16_support {};
    bool coopmat_support_16x16x16_f16acc {};
    bool coopmat_support_16x16x16_f32acc {};
    bool coopmat1_fa_support {};
    uint32_t coopmat_m;
    uint32_t coopmat_n;
    uint32_t coopmat_k;

    bool coopmat_int_support;
    uint32_t coopmat_int_m;
    uint32_t coopmat_int_n;
    uint32_t coopmat_int_k;

    bool coopmat2;
    bool coopmat2_bf16_support {};
    bool coopmat2_decode_vector;

    bool dot2_f16 {};
    bool ocp_fp4 {};

    bool pipeline_executable_properties_support {};

    bool device_fault {};
    PFN_vkGetDeviceFaultInfoEXT pfn_vkGetDeviceFaultInfoEXT {};

    bool serialize_submissions {};

    const ggml_cgraph * diag_cgraph {};
    int diag_prev_start = -1;
    int diag_prev_end = -1;

    size_t idx;

    bool mul_mat_l[GGML_TYPE_COUNT];
    bool mul_mat_m[GGML_TYPE_COUNT];
    bool mul_mat_s[GGML_TYPE_COUNT];
    bool mul_mat_id_l[GGML_TYPE_COUNT];
    bool mul_mat_id_m[GGML_TYPE_COUNT];
    bool mul_mat_id_s[GGML_TYPE_COUNT];

    // Separate flags for the q8_1 (integer dot) mmq path, whose shader uses
    // a different shared-memory layout than the float matmul shaders.
    bool mul_mat_l_int[GGML_TYPE_COUNT];
    bool mul_mat_m_int[GGML_TYPE_COUNT];
    bool mul_mat_s_int[GGML_TYPE_COUNT];
    bool mul_mat_id_l_int[GGML_TYPE_COUNT];
    bool mul_mat_id_m_int[GGML_TYPE_COUNT];
    bool mul_mat_id_s_int[GGML_TYPE_COUNT];

    vk::DescriptorSetLayout dsl;

    std::map<vk_matmul_pipeline_key, std::vector<vk_matmul_pipeline_pair>> pipeline_matmul;
    matmul_tile_selector_t matmul_tile_selector;
    matmul_tile_selector_t matmul_id_tile_selector;

    vk_pipeline pipeline_matmul_split_k_reduce;
    vk_pipeline pipeline_quantize_q8_1_x4;

    vk_pipeline pipeline_dequant[GGML_TYPE_COUNT];
    vk_pipeline pipeline_dequant_transpose[GGML_TYPE_COUNT]; // fused dequant+transpose for FA quant-KV
    // lane-230 / R46b commit D: one S-X8 f32 mat-vec pipeline per measured (rows, workgroup)
    // variant per NUM_COLS, built only on a device the R47d sweep speaks for and only for the
    // (variant, n) pairs the lookup table names. Everything else stays null and is never selected.
    bool sx8_mmv_tuned = false;
    vk_pipeline pipeline_dequant_mul_mat_vec_sx8_tuned[SX8_TUNE_VARIANT_COUNT][mul_mat_vec_max_cols];

    vk_pipeline pipeline_dequant_mul_mat_vec_f32_f32[DMMV_WG_SIZE_COUNT][GGML_TYPE_COUNT][mul_mat_vec_max_cols];
    vk_pipeline pipeline_dequant_mul_mat_vec_f16_f32[DMMV_WG_SIZE_COUNT][GGML_TYPE_COUNT][mul_mat_vec_max_cols];
    vk_pipeline pipeline_dequant_mul_mat_vec_id_f32[DMMV_WG_SIZE_COUNT][GGML_TYPE_COUNT];

    vk_pipeline pipeline_dequant_mul_mat_vec_q8_1_f32[DMMV_WG_SIZE_COUNT][GGML_TYPE_COUNT][mul_mat_vec_max_cols];
    // arifi lane-271 / R85: Q4_K q8_1 MMVQ with the 4+1 column split (spec constant 4 = 3), created
    // at NUM_COLS 5 and 6 only and chosen per m in ggml_vk_get_dequantize_mul_mat_vec.
    vk_pipeline pipeline_mul_mat_vec_q4k_q8_1_split[DMMV_WG_SIZE_COUNT][mul_mat_vec_max_cols];
    vk_pipeline pipeline_dequant_mul_mat_vec_id_q8_1_f32[DMMV_WG_SIZE_COUNT][GGML_TYPE_COUNT];

    // Activation pre-rotation for the TurboQuant rotated matmul path. One
    // pipeline serves TQ3_1S and TQ4_1S: they share the 32-element sign pattern
    // and butterfly, and the shader only touches the activation.
    vk_pipeline pipeline_tq_rotate_act;

    vk_pipeline pipeline_mul_mat_vec_p021_f16_f32[p021_max_gqa_ratio];
    vk_pipeline pipeline_mul_mat_vec_nc_f16_f32;
    vk_pipeline pipeline_get_rows[GGML_TYPE_COUNT];
    vk_pipeline pipeline_get_rows_f32[GGML_TYPE_COUNT];
    vk_pipeline pipeline_get_rows_back_f32;
    vk_pipeline pipeline_acc_f32;
    vk_pipeline pipeline_set_f32;

    // [src0 0=fp32,1=fp16][src1 0=fp32,1=fp16][dst 0=fp32,1=fp16]
    vk_pipeline pipeline_add[2][2][2];
    vk_pipeline pipeline_add_norepeat[2][2][2];
    vk_pipeline pipeline_sub[2][2][2];
    vk_pipeline pipeline_sub_norepeat[2][2][2];
    vk_pipeline pipeline_mul[2][2][2];
    vk_pipeline pipeline_mul_norepeat[2][2][2];
    vk_pipeline pipeline_div[2][2][2];
    vk_pipeline pipeline_div_norepeat[2][2][2];
    vk_pipeline pipeline_add_rms[2][2][2];
    vk_pipeline pipeline_add_rms_norepeat[2][2][2];

    // indexed by num_additional_fused_ops == num_adds - 1
    vk_pipeline pipeline_multi_add[MAX_FUSED_ADDS];
    vk_pipeline pipeline_multi_add_rms[MAX_FUSED_ADDS];

    vk_pipeline pipeline_add_id_f32;

    vk_pipeline pipeline_concat_i8, pipeline_concat_i16, pipeline_concat_i32, pipeline_concat_i64;
    vk_pipeline pipeline_concat_transpose_i32;
    vk_pipeline pipeline_upscale_nearest_f32, pipeline_upscale_bilinear_f32, pipeline_upscale_bicubic_f32, pipeline_upscale_bilinear_antialias_f32;
    vk_pipeline pipeline_scale_f32;
    vk_pipeline pipeline_log[2];
    vk_pipeline pipeline_tri[2];
    vk_pipeline pipeline_diag[2];
    vk_pipeline pipeline_clamp[2];
    vk_pipeline pipeline_pad_f32;
    vk_pipeline pipeline_pad_reflect_1d_f32;
    vk_pipeline pipeline_roll_f32;
    vk_pipeline pipeline_repeat_i32, pipeline_repeat_back_f32;
    vk_pipeline pipeline_repeat_i16;
    vk_pipeline pipeline_cpy_f32_f32, pipeline_cpy_f32_f16, pipeline_cpy_f16_f16, pipeline_cpy_f16_f32, pipeline_cpy_f32_bf16, pipeline_cpy_bf16_f32, pipeline_cpy_f32_i32, pipeline_cpy_i32_f32;
    vk_pipeline pipeline_contig_cpy_f32_f32, pipeline_contig_cpy_f32_f16, pipeline_contig_cpy_f16_f16, pipeline_contig_cpy_f16_f32, pipeline_contig_cpy_f32_bf16, pipeline_contig_cpy_bf16_f32, pipeline_contig_cpy_f32_i32, pipeline_contig_cpy_i32_f32;
    vk_pipeline pipeline_cpy_f32_quant[GGML_TYPE_COUNT];
    vk_pipeline pipeline_cpy_quant_f32[GGML_TYPE_COUNT];
    // lane-194: quant -> F16 read-back. Only GGML_TYPE_Q4_0 is populated; every other slot stays
    // nullptr and supports_op is keyed on exactly that, so an unpopulated type declines rather
    // than reaching "Missing CPY op for types" at dispatch.
    vk_pipeline pipeline_cpy_quant_f16[GGML_TYPE_COUNT];
    vk_pipeline pipeline_cpy_transpose_16, pipeline_cpy_transpose_32;
    vk_pipeline pipeline_cpy_transpose_02_16, pipeline_cpy_transpose_02_32;
    // [src0 0=fp32,1=fp16][dst]
    vk_pipeline pipeline_set_rows_i32[2][GGML_TYPE_COUNT];
    vk_pipeline pipeline_set_rows_i64[2][GGML_TYPE_COUNT];
    vk_pipeline pipeline_norm_f32;
    vk_pipeline pipeline_group_norm_f32;
    vk_pipeline pipeline_rms_norm_f32;
    vk_pipeline pipeline_rms_norm_mul_f32;
    vk_pipeline pipeline_rms_norm_mul_add_f32;
    vk_pipeline pipeline_rms_norm_mul_add_mul_f32;
    vk_pipeline pipeline_rms_norm_mul_add_partials_f32;
    vk_pipeline pipeline_rms_norm_mul_add_mul_partials_f32;
    vk_pipeline pipeline_rms_norm_set_rows_f32_f32;
    vk_pipeline pipeline_rms_norm_set_rows_f32_f16;
    vk_pipeline pipeline_rms_norm_partials_f32;
    vk_pipeline pipeline_rms_norm_mul_partials_f32;
    vk_pipeline pipeline_rms_norm_mul_rope_f32_f32;
    vk_pipeline pipeline_rms_norm_mul_rope_f32_f16;
    vk_pipeline pipeline_rms_norm_back_f32;
    vk_pipeline pipeline_l2_norm_f32;

    // [src/dst 0=fp32,1=fp16]
    vk_pipeline pipeline_exp[2];
    vk_pipeline pipeline_expm1[2];
    vk_pipeline pipeline_elu[2];
    vk_pipeline pipeline_gelu[2];
    vk_pipeline pipeline_gelu_erf[2];
    vk_pipeline pipeline_gelu_quick[2];
    vk_pipeline pipeline_silu[2];
    vk_pipeline pipeline_relu[2];
    vk_pipeline pipeline_sqr[2];
    vk_pipeline pipeline_sqrt[2];
    vk_pipeline pipeline_sin[2];
    vk_pipeline pipeline_cos[2];
    vk_pipeline pipeline_xielu[2];
    vk_pipeline pipeline_neg[2];
    vk_pipeline pipeline_tanh[2];
    vk_pipeline pipeline_sigmoid[2];
    vk_pipeline pipeline_hardsigmoid[2];
    vk_pipeline pipeline_hardswish[2];
    vk_pipeline pipeline_abs[2];
    vk_pipeline pipeline_softplus[2];
    vk_pipeline pipeline_step[2];
    vk_pipeline pipeline_round[2];
    vk_pipeline pipeline_ceil[2];
    vk_pipeline pipeline_floor[2];
    vk_pipeline pipeline_trunc[2];
    vk_pipeline pipeline_sgn[2];

    // fused UNARY+MUL pipelines: [op][f16][norepeat][op_on_b]
    vk_pipeline pipeline_unary_mul[4][2][2][2];

    vk_pipeline pipeline_add1_f16_f16;
    vk_pipeline pipeline_add1_f16_f32;
    vk_pipeline pipeline_add1_f32_f32;

    vk_pipeline pipeline_arange_f32;

    vk_pipeline pipeline_fill_f32;
    vk_pipeline pipeline_fill_f16;

    vk_pipeline pipeline_geglu[2];
    vk_pipeline pipeline_reglu[2];
    vk_pipeline pipeline_swiglu[2];
    vk_pipeline pipeline_swiglu_oai[2];
    vk_pipeline pipeline_swiglu_clamp[2];
    vk_pipeline pipeline_geglu_erf[2];
    vk_pipeline pipeline_geglu_quick[2];

    vk_pipeline pipeline_leaky_relu[2];
    vk_pipeline pipeline_silu_back_f32;
    vk_pipeline pipeline_diag_mask_inf_f32;
    vk_pipeline pipeline_soft_max_f32, pipeline_soft_max_f32_f16;
    vk_pipeline pipeline_soft_max_f32_wg512, pipeline_soft_max_f32_f16_wg512;
    vk_pipeline pipeline_soft_max_back_f32;

    vk_pipeline pipeline_soft_max_large1_f32, pipeline_soft_max_large1_f32_f16;
    vk_pipeline pipeline_soft_max_large2_f32, pipeline_soft_max_large2_f32_f16;
    vk_pipeline pipeline_soft_max_large3_f32, pipeline_soft_max_large3_f32_f16;

    vk_pipeline pipeline_rope_norm_f32, pipeline_rope_norm_f16, pipeline_rope_norm_f32_f16;
    vk_pipeline pipeline_rope_neox_f32, pipeline_rope_neox_f16, pipeline_rope_neox_f32_f16;
    vk_pipeline pipeline_rope_multi_f32, pipeline_rope_multi_f16, pipeline_rope_multi_f32_f16;
    vk_pipeline pipeline_rope_vision_f32, pipeline_rope_vision_f16;
    vk_pipeline pipeline_argsort_f32[num_argsort_pipelines];
    vk_pipeline pipeline_argsort_large_f32[num_argsort_pipelines];
    vk_pipeline pipeline_topk_f32[num_topk_pipelines];
    vk_pipeline pipeline_topk_radix_f32;
    vk_pipeline pipeline_topk_radix_qsa; // qwen4 QSA indexer fusion (f16 mask)
    vk_pipeline pipeline_sum_rows_f32;
    vk_pipeline pipeline_cross_entropy_loss_f32, pipeline_cross_entropy_loss_f32_wg512;
    vk_pipeline pipeline_cross_entropy_loss_back_f32, pipeline_cross_entropy_loss_back_f32_wg512;
    vk_pipeline pipeline_fwht_f32[4];
    vk_pipeline pipeline_escha_mm[2];    // [0]=ESCHA2 (K=2), [1]=ESCHA3 (K=3) — lane-164
    vk_pipeline pipeline_escha_mm_mc[2]; // same, multi-column (ESCHA_MM_COLS per workgroup)
    // Column ladder (seat-40), indexed [rung][K]. Rung r serves ESCHA_COL_RUNGS[r] columns.
    vk_pipeline pipeline_escha_mm_col[6][2];
    vk_pipeline pipeline_escha_mm_col_f16s[6][2];
    // KHR cooperative-matrix C=16 arm, f16 operands with an f32 accumulator.
    vk_pipeline pipeline_escha_mm_cm[2];
    vk_pipeline pipeline_escha_mm_lut[2];   // table-generator arm (seat-40)
    vk_buffer   escha_lut;                  // 65,536 f16 generator values, 128 KiB
    vk_pipeline pipeline_escha_mm_b2[2]; // same, 256 threads over two output blocks (seat-40)
    vk_pipeline pipeline_cumsum_f32;
    vk_pipeline pipeline_cumsum_small_f32;
    vk_pipeline pipeline_cumsum_multipass1_f32;
    vk_pipeline pipeline_cumsum_multipass2_f32;
    vk_pipeline pipeline_argmax_f32;
    vk_pipeline pipeline_count_equal_i32;
    vk_pipeline pipeline_dsv4_hc_comb_f32;
    vk_pipeline pipeline_dsv4_hc_pre_f32;
    vk_pipeline pipeline_dsv4_hc_pre_gated_f32;
    vk_pipeline pipeline_dsv4_hc_post_f32;
    vk_pipeline pipeline_dsv4_hc_post_nocomb_f32;
    std::map<vk_solve_tri_pipeline_state, vk_pipeline> pipeline_solve_tri_f32;
    vk_pipeline pipeline_im2col_f32, pipeline_im2col_f32_f16;
    vk_pipeline pipeline_im2col_3d_f32, pipeline_im2col_3d_f32_f16;
    vk_pipeline pipeline_timestep_embedding_f32;
    vk_pipeline pipeline_conv_transpose_1d_f32;
    vk_pipeline pipeline_col2im_1d_f32;
    vk_pipeline pipeline_col2im_1d_f16;
    vk_pipeline pipeline_col2im_1d_bf16;
    vk_pipeline pipeline_out_prod_f32;
    vk_pipeline pipeline_snake_f32;
    vk_pipeline pipeline_snake_f16;
    vk_pipeline pipeline_snake_bf16;
    vk_pipeline pipeline_pool1d_f32;
    vk_pipeline pipeline_pool2d_f32;
    vk_pipeline pipeline_turbo_wht;
    vk_pipeline pipeline_rwkv_wkv6_f32;
    vk_pipeline pipeline_rwkv_wkv7_f32;
    vk_pipeline pipeline_gated_linear_attn_f32;
    vk_pipeline pipeline_lightning_indexer_f32[GGML_TYPE_COUNT];
    // [size_idx][kda] where size_idx: 0=d16, 1=d32, 2=d64, 3=d128
    vk_pipeline pipeline_gated_delta_net[4][2];
    vk_pipeline pipeline_ssm_scan_f32_d128;
    vk_pipeline pipeline_ssm_scan_f32_d256;
    vk_pipeline pipeline_ssm_conv_f32;
    vk_pipeline pipeline_ssm_conv_silu_f32;
    vk_pipeline pipeline_ssm_conv_bias_silu_f32;
    vk_pipeline pipeline_opt_step_adamw_f32;
    vk_pipeline pipeline_opt_step_sgd_f32;
    std::map<vk_conv2d_pipeline_state, vk_pipeline> pipeline_conv2d_f32[CONV_SHAPE_COUNT];
    std::map<vk_conv2d_pipeline_state, vk_pipeline> pipeline_conv2d_f16_f32[CONV_SHAPE_COUNT];
    std::map<vk_conv2d_pipeline_state, vk_pipeline> pipeline_conv_transpose_2d_f32[CONV_SHAPE_COUNT];
    std::map<vk_conv2d_pipeline_state, vk_pipeline> pipeline_conv_transpose_2d_f16_f32[CONV_SHAPE_COUNT];
    std::map<vk_conv3d_pipeline_state, vk_pipeline> pipeline_conv3d_f32[CONV_SHAPE_COUNT];
    std::map<vk_conv3d_pipeline_state, vk_pipeline> pipeline_conv3d_f16_f32[CONV_SHAPE_COUNT];
    vk_pipeline pipeline_conv2d_dw_whcn_f32, pipeline_conv2d_dw_whcn_f16_f32;
    vk_pipeline pipeline_conv2d_dw_cwhn_f32, pipeline_conv2d_dw_cwhn_f16_f32;

    std::map<vk_fa_pipeline_state, vk_pipeline> pipeline_flash_attn_f32_f16;

    std::map<std::pair<uint32_t, uint32_t>, vk_pipeline> pipeline_fa_mask_opt;

    vk_pipeline pipeline_fa_sparse_compact;
    vk_pipeline pipeline_fa_sparse_compact_subgroup;
    bool fa_sparse_compact_use_subgroups;

    vk_pipeline pipeline_flash_attn_split_k_reduce;
    std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, std::pair<vk_pipeline, vk_pipeline>> pipeline_xe_fa_decode_dual_phases;
    vk_pipeline pipeline_count_experts;

    // [2] is for whether to take n_experts from spec constant (0) or push constant (1)
    vk_pipeline pipeline_topk_moe[num_topk_moe_pipelines][2];

    std::vector<vk_pipeline_ref> all_pipelines;

    std::vector<std::tuple<void*, size_t, vk_buffer>> pinned_memory;

    vk::Fence fence;
    vk_buffer sync_staging;

    ggml_backend_buffer_type buffer_type;

    bool disable_fusion;
    bool disable_host_visible_vidmem;
    bool allow_sysmem_fallback;
    bool disable_graph_optimize;

    std::unique_ptr<vk_memory_logger> memory_logger;

    ~vk_device_struct();

};

inline void vk_command_pool::init(vk_device& device, vk_queue *q_) {
    cmd_buffers.clear();
    q = q_;

    vk::CommandPoolCreateInfo command_pool_create_info(
        vk::CommandPoolCreateFlags(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
        q->queue_family_index);
    pool = device->device.createCommandPool(command_pool_create_info);
}

inline void vk_command_pool::destroy(vk::Device& device) {
    device.destroyCommandPool(pool);
    pool = nullptr;
    cmd_buffers.clear();
}


// RAII ownership of ONE reservation. A buffer that owns memory owns exactly one of these; anything
// that fails before ownership is accepted (a refused candidate, a throwing allocateMemory, a
// throwing bind/map/address setup) rolls back through the destructor. release() is idempotent, so
// destruction releases exactly once and a second destruction is a no-op instead of an underflow.
//
// R46b B7a-R2 (finding 3): the guard names the LEDGER it reserved from, so a reservation taken on
// device A can only ever be released back to device A's ledger.
struct vk_heap_reservation {
    vk_heap_ledger * ledger = nullptr;
    uint32_t         heap   = UINT32_MAX;
    uint64_t         bytes  = 0;
    bool             held   = false;

    vk_heap_reservation() = default;
    vk_heap_reservation(const vk_heap_reservation &)             = delete;
    vk_heap_reservation & operator=(const vk_heap_reservation &) = delete;

    vk_heap_reservation(vk_heap_reservation && other) noexcept { *this = std::move(other); }

    vk_heap_reservation & operator=(vk_heap_reservation && other) noexcept {
        if (this != &other) {
            release();
            ledger = other.ledger;
            heap   = other.heap;
            bytes  = other.bytes;
            held   = other.held;
            other.held = false;
        }
        return *this;
    }

    bool try_reserve(vk_heap_ledger * l, uint32_t h, uint64_t b, uint64_t budget) {
        GGML_ASSERT(!held);
        if (l == nullptr || !l->reserve(h, b, budget)) {
            return false;
        }
        ledger = l;
        heap   = h;
        bytes  = b;
        held   = true;
        return true;
    }

    // R46b B7b: take ownership of bytes a batch plan ALREADY charged to this ledger. It does
    // not touch the ledger - the plan made the charge - so there is no double charge; it only moves
    // who is responsible for the single matching release. noexcept, and the first thing the planned
    // allocation path does, so no window exists where the plan has stopped owning the bytes and no
    // reservation owns them yet.
    void adopt(vk_heap_ledger * l, uint32_t h, uint64_t b) noexcept {
        release();
        ledger = l;
        heap   = h;
        bytes  = b;
        held   = true;
    }

    bool release() {
        if (!held) {
            return false;
        }
        held = false;
        return ledger->release(heap, bytes);
    }

    ~vk_heap_reservation() { release(); }
};

struct vk_buffer_struct {
    vk::Buffer buffer = VK_NULL_HANDLE;
    vk::DeviceMemory device_memory = VK_NULL_HANDLE;
    vk::MemoryPropertyFlags memory_property_flags;
    void * ptr;
    size_t size = 0;
    vk::DeviceAddress bda_addr {};

    vk_device device;

    // R46b B7a: the heap reservation this buffer owns, released exactly once by its destructor.
    // It lives here and not in ggml_vk_destroy_buffer() on purpose: that function does buf.reset()
    // on a reference, so other shared_ptr copies can keep the allocation alive. Releasing there
    // would drop the reservation while the driver still holds the bytes, which under-counts the
    // ledger and re-admits the over-allocation this is meant to stop.
    //
    // R46b B7a-R2 (finding 3): declared AFTER `device` on purpose. The ledger it points into is a
    // member of vk_device_struct, so `device` must still be alive when the reservation releases;
    // members are destroyed in reverse declaration order, so this one goes first, while `device` is
    // still alive. The real guarantee is the destructor body below: it frees the DRIVER memory first
    // and only then calls release() as its LAST statement (R3 finding 1 moved that release down from
    // the top, where it was the R2 concurrency defect). This declaration order is the backstop that
    // keeps the ledger reachable if the reservation member is destroyed without that explicit call.
    vk_heap_reservation reservation;

    ~vk_buffer_struct() {
        // R46b B7a-R3 (finding 1): the DRIVER memory goes back first, the ledger is told only
        // afterwards. The reverse order (R2) advertised the bytes as free while the driver still
        // owned them, so a concurrent allocator could be admitted against bytes that were still in
        // use - the same ledger/driver divergence the setup guard avoids on the failure path.
        //
        // No lock is needed for this: releasing last makes the ledger transiently OVER-count, which
        // is the conservative direction. It can refuse an allocation a hair early; it can never
        // admit an overcommit. The release itself is mutex-protected inside vk_heap_ledger.
        if (size != 0) {
            VK_LOG_DEBUG("~vk_buffer_struct(" << buffer << ", " << size << ")");

            VK_TEST_NOTE_DRIVER_FREE(device);
            device->device.freeMemory(device_memory);
            device->device.destroyBuffer(buffer);
        }

        // Unconditional and last: it does not depend on `size` being initialized, it runs on every
        // path including the size == 0 one, and release() is idempotent.
        reservation.release();
    }
};

// R46b B7a-R2 (finding 1): exception-safe ownership of the raw driver objects during buffer setup.
//
// ggml_vk_create_buffer() creates the VkBuffer and then allocates VkDeviceMemory, but map, bind,
// buffer-device-address and the memory logger can all still throw. Until `buf->size` is set,
// ~vk_buffer_struct() returns early and frees nothing, so before this guard a throw in that window
// leaked the VkDeviceMemory while the reservation ledger rolled back and reported the bytes free -
// the ledger and the driver diverged and a later allocation was admitted against bytes the driver
// still held.
//
// This guard is the SOLE owner of those handles until commit(). It clears the handles (and `size`)
// as it destroys them, so ~vk_buffer_struct() can never free them a second time; exactly-once is
// therefore by construction and not by an invariant on `size`.
static void vk_alloc_trace_record_free(VkBuffer buffer);  // defined with the R47a trace facility

struct vk_buffer_setup_guard {
    vk_device & device;
    vk_buffer & buf;
    bool        armed = true;

    vk_buffer_setup_guard(vk_device & d, vk_buffer & b) : device(d), buf(b) {}
    vk_buffer_setup_guard(const vk_buffer_setup_guard &)             = delete;
    vk_buffer_setup_guard & operator=(const vk_buffer_setup_guard &) = delete;

    // Ownership transfer: from here the buffer owns the handles and its destructor frees them.
    void commit() { armed = false; }

    ~vk_buffer_setup_guard() {
        if (!armed || !buf) {
            return;
        }
        // Retire the R47a trace record first. vk_alloc_trace_record_alloc() runs as soon as
        // allocateMemory() succeeds, i.e. before every fallible step below it, so without this the
        // new cleanup path would free the driver objects while heap_live still counted the bytes
        // and by_buffer still held a key on a destroyed VkBuffer handle. No-op when trace is off.
        vk_alloc_trace_record_free(buf->buffer);

        // vkFreeMemory implicitly unmaps a mapped allocation, and both calls accept VK_NULL_HANDLE,
        // so this is correct whether the failure happened before or after map/bind.
        buf->size = 0;
        buf->ptr  = nullptr;
        if (buf->device_memory) {
            device->device.freeMemory(buf->device_memory);
            buf->device_memory = VK_NULL_HANDLE;
        }
        if (buf->buffer) {
            device->device.destroyBuffer(buf->buffer);
            buf->buffer = VK_NULL_HANDLE;
        }
    }
};

struct vk_subbuffer {
    vk_buffer buffer;
    uint64_t offset;
    uint64_t size;

    operator vk::DescriptorBufferInfo() const {
        return { buffer->buffer, offset, size };
    }
};

struct vk_semaphore {
    vk::Semaphore s;
    uint64_t value;
};

struct vk_event {
    std::vector<vk::Event> events_free; // Events available for reuse
    std::vector<vk::Event> events_submitted; // Events that are fully submitted and can be reused on next synchronize
    vk::Event event;
    bool has_event;

    vk_semaphore tl_semaphore;
    vk_command_buffer* cmd_buffer = nullptr;
    uint64_t cmd_buffer_use_counter = 0;
};

struct vk_submission {
    vk_command_buffer* buffer = nullptr;
    std::vector<vk_semaphore> wait_semaphores;
    std::vector<vk_semaphore> signal_semaphores;
};

typedef std::vector<vk_submission> vk_sequence;

#define MAT_VEC_FUSION_FLAGS_BIAS0 0x1

#define MAT_VEC_FUSION_FLAGS_BIAS1 0x2

#define MAT_VEC_FUSION_FLAGS_SCALE0 0x4

#define MAT_VEC_FUSION_FLAGS_SCALE1 0x8

struct vk_staging_memcpy {
    vk_staging_memcpy(void * _dst, const void * _src, size_t _n) : dst(_dst), src(_src), n(_n) {}

    void * dst;
    const void * src;
    size_t n;
};

struct vk_staging_memset {
    vk_staging_memset(void * _dst, uint32_t _val, size_t _n) : dst(_dst), val(_val), n(_n) {}

    void * dst;
    uint32_t val;
    size_t n;
};

struct vk_context_struct {
    vk_submission * s;
    std::vector<vk_sequence> seqs;

    int exit_tensor_idx;

    std::vector<vk_staging_memcpy> in_memcpys;
    std::vector<vk_staging_memcpy> out_memcpys;
    std::vector<vk_staging_memset> memsets;

    std::vector<std::string> debug_labels;

    // lane-232 / R47a: one entry per recorded transfer, populated only under GGML_VK_ALLOC_TRACE
    // and flushed (then cleared) by ggml_vk_submit right before the queue call.
    std::vector<std::string> trace_ops;

    vk_command_pool * p {};
};

typedef std::shared_ptr<vk_context_struct> vk_context;

typedef std::weak_ptr<vk_context_struct> vk_context_ref;

struct ggml_vk_garbage_collector {
    std::vector<vk_semaphore> tl_semaphores;
    std::vector<vk_semaphore> semaphores;
    std::vector<vk::Event> events;
    std::vector<vk_context> contexts;
};

#define VK_LOG_MEMORY(msg) if (vk_memory_logger_enabled) { std::cerr << "ggml_vulkan memory: " << msg << std::endl; }

static std::string format_size(size_t size) {
    const size_t kib = 1024;
    const size_t mib = kib * 1024;
    const size_t gib = mib * 1024;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (size >= gib) {
        oss << static_cast<double>(size) / gib << " GiB";
    } else if (size >= mib) {
        oss << static_cast<double>(size) / mib << " MiB";
    } else if (size >= kib) {
        oss << static_cast<double>(size) / kib << " KiB";
    } else {
        oss << size << " B";
    }

    return oss.str();
}

class vk_memory_logger {
public:
    vk_memory_logger(): total_device(0), total_host(0) {}
    void log_allocation(vk_buffer_ref buf_ref, size_t size);
    void log_deallocation(vk_buffer_ref buf_ref);

private:
    std::map<vk::Buffer, size_t> allocations; // Track allocations
    size_t total_device;
    size_t total_host;
    static std::mutex log_mutex;
};

inline std::mutex vk_memory_logger::log_mutex;

class vk_perf_logger {
  public:
    void print_timings(bool force = false);


    std::string get_node_fusion_name(const ggml_tensor * node, const char *fusion_name, uint64_t *n_flops);


    void log_timing(const ggml_tensor * node, const char *fusion_name, uint64_t time) {
        uint64_t n_flops;
        std::string name = get_node_fusion_name(node, fusion_name, &n_flops);
        if (n_flops) {
            flops[name].push_back(n_flops);
        }
        timings[name].push_back(time);
    }

    void log_timing(const std::vector<ggml_tensor *> &nodes, const std::vector<const char *> &names, uint64_t time) {
        uint64_t total_flops = 0;
        std::string name;
        for (size_t n = 0; n < nodes.size(); ++n) {
            uint64_t n_flops = 0;
            name += get_node_fusion_name(nodes[n], names[n], &n_flops);
            total_flops += n_flops;

            if (n != nodes.size() - 1) {
                name += ", ";
            }
        }
        if (total_flops) {
            flops[name].push_back(total_flops);
        }
        timings[name].push_back(time);
    }

  private:
    std::map<std::string, std::vector<uint64_t>> timings;
    std::map<std::string, std::vector<uint64_t>> flops;
    uint32_t print_count {};
};

struct ggml_backend_vk_context {
    std::string name;

    vk_device device;

    size_t semaphore_idx, event_idx;
    ggml_vk_garbage_collector gc;
    size_t prealloc_size_x, prealloc_size_y, prealloc_size_split_k, prealloc_size_add_rms_partials, prealloc_size_add_rms_partials_offset;
    vk_buffer prealloc_x, prealloc_y, prealloc_split_k, prealloc_add_rms_partials, sync_staging;
    vk::Fence fence, almost_ready_fence;
    bool submit_pending {};
    bool almost_ready_fence_pending {};
    // Set before op_add and unset after op_rms_norm to indicate that the add should
    // write partial sums to accumulate the square of the vector components
    bool do_add_rms_partials_offset_calculation;
    bool do_add_rms_partials;

    uint64_t last_total_flops {UINT64_MAX};

    // Cache most recent tensor that was converted into prealloc_y, and what pipeline it used to convert.
    vk_pipeline_struct * prealloc_y_last_pipeline_used {};
    const ggml_tensor * prealloc_y_last_tensor_used {};
    // True when the K dimension in prealloc_y is padded.
    bool prealloc_y_last_k_padded {};

    // Track which nodes have been used since the last sync, and whether they were written to
    std::vector<const ggml_tensor *> unsynced_nodes_written;
    std::vector<const ggml_tensor *> unsynced_nodes_read;
    // Track which prealloc buffers have pending reads that need to be synchronized.
    // These are checked before writing to the buffer (and call ggml_vk_sync_buffers if set),
    // and set to true after the buffer contents are consumed.
    bool prealloc_x_need_sync, prealloc_y_need_sync, prealloc_split_k_need_sync;

    vk_context_ref compute_ctx;

    vk_context_ref transfer_ctx;
    vk_semaphore transfer_semaphore;
    uint64_t transfer_semaphore_last_submitted {};

    std::vector<vk_context_ref> tensor_ctxs;

    std::vector<vk::DescriptorPool> descriptor_pools;
    std::vector<vk::DescriptorSet> descriptor_sets;
    uint32_t descriptor_set_idx {};
    uint32_t pipeline_descriptor_set_requirements {};

    vk_command_pool compute_cmd_pool;
    vk_command_pool transfer_cmd_pool;

    // number of additional consecutive nodes that are being fused with the
    // node currently being processed
    int num_additional_fused_ops {};
    // Bitmask of which fused ops need to write an intermediate value to memory.
    // Bit 'i' means nodes[start_of_fusion + i] writes to memory.
    // If there's no fusion, bit 0 is still set.
    int fused_ops_write_mask {};
    topk_moe_mode fused_topk_moe_mode {};
    bool fused_topk_moe_scale {};
    // QSA indexer gather+add+top_k fused into one radix-select
    bool fused_topk_qsa {};
    rms_norm_mode fused_rms_norm_mode {RMS_NORM_COUNT};

    // for GGML_VK_PERF_LOGGER
    std::unique_ptr<vk_perf_logger> perf_logger;
    vk::QueryPool query_pool;
    std::vector<const char *> query_fusion_names;
    std::vector<int> query_fusion_node_count;
    std::vector<ggml_tensor *> query_nodes;
    std::vector<int> query_node_idx;
    int32_t num_queries {};
    int32_t query_idx {};
};

struct ggml_backend_vk_buffer_context {
    vk_device_ref device;
    vk_buffer dev_buffer;
    std::string name;

    ggml_backend_vk_buffer_context(vk_device_ref device, vk_buffer&& dev_buffer, std::string& name) :
        device(device),
        dev_buffer(dev_buffer),
        name(name) {
    }

    ~ggml_backend_vk_buffer_context();

};

struct vk_instance_t {
    vk::Instance instance;

    bool debug_utils_support = false;  // VK_EXT_debug_utils enabled
    PFN_vkSetDebugUtilsObjectNameEXT pfn_vkSetDebugUtilsObjectNameEXT = {};
    PFN_vkQueueBeginDebugUtilsLabelEXT pfn_vkQueueBeginDebugUtilsLabelEXT = {};
    PFN_vkQueueEndDebugUtilsLabelEXT   pfn_vkQueueEndDebugUtilsLabelEXT   = {};
    PFN_vkCmdBeginDebugUtilsLabelEXT   pfn_vkCmdBeginDebugUtilsLabelEXT   = {};
    PFN_vkCmdEndDebugUtilsLabelEXT pfn_vkCmdEndDebugUtilsLabelEXT = {};
    PFN_vkCmdInsertDebugUtilsLabelEXT  pfn_vkCmdInsertDebugUtilsLabelEXT  = {};

    std::vector<size_t> device_indices;
    std::vector<bool>   device_supports_membudget;
    vk_device devices[GGML_VK_MAX_DEVICES];
};

typedef void (*ggml_vk_func_t)(ggml_backend_vk_context * ctx, vk_context& subctx, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst);

static constexpr uint32_t kSpvOpCooperativeMatrixLoadTensorNV = 5367;

static constexpr uint32_t kSpvCapabilityCooperativeMatrixDecodeVectorNV = 5447;

static constexpr uint32_t kSpvTensorAddressingDecodeVectorFuncBit = 0x4;

struct vk_fa_tuning_params {
    FaCodePath path;
    uint32_t workgroup_size;
    uint32_t subgroup_size;
    uint32_t block_rows;
    uint32_t block_cols;
    uint32_t d_split;
    uint32_t row_split;
    bool shmem_staging;
    bool disable_subgroups;
    uint32_t limit_occupancy_shmem;

    void print() const {
        std::cerr << "path=" << path << " workgroup_size=" << workgroup_size << " subgroup_size=" << subgroup_size <<
                     " block_rows=" << block_rows << " block_cols=" << block_cols << " d_split=" << d_split <<
                     " row_split=" << row_split << " shmem_staging=" << shmem_staging << " disable_subgroups=" << disable_subgroups <<
                     " limit_occupancy_shmem=" << limit_occupancy_shmem << std::endl;
    }
};

struct GpuPipelineConfig {
    // GPU architecture identifier.
    // Example: vk_device_architecture::AMD_GCN
    vk_device_architecture arch;

    // Mapping of pipeline names to their specific subgroup sizes.
    // Example: {"soft_max_f32", 64}
    std::unordered_map<std::string, uint32_t> pipelines;

    // Default subgroup size for this GPU.
    // Defaults to 0 if not explicitly provided.
    uint32_t default_subgroup_size = 0;
};

static constexpr uint32_t RDNA_DEFAULT_SUBGROUP_SIZE = 32;

struct CompileTask {
    vk_pipeline pipeline;
    size_t spv_size;
    const void * spv_data;
    std::string entrypoint;
    uint32_t parameter_count;
    std::array<uint32_t, 3> wg_denoms;
    std::vector<uint32_t> specialization_constants;
    bool disable_robustness;
    bool require_full_subgroups;
    uint32_t required_subgroup_size;
};

struct ggml_vk_debug_label {
    // at most one of these is set, depending on the scope the label was opened in
    vk_context_struct * subctx {};
    vk_queue_handle *   qhandle {};

    // one region per dispatch, e.g. "matmul_q4_k_f32_f16acc_aligned_m (192,8,1)".
    // RGP cannot recover the pipeline name on its own, it only has the hash
    ggml_vk_debug_label(vk_context & ctx, const std::string & pipeline_name, uint32_t wg0, uint32_t wg1, uint32_t wg2);


    // one region per graph node
    // fused nodes are joined with '+', e.g. "RMS_NORM+MUL+ROPE Qcur-19"
    ggml_vk_debug_label(vk_context & ctx, const ggml_cgraph * cgraph, int node_idx, int n_fused);


    // one region per graph evaluation, opened on the queue instead of a command buffer
    // so it spans every submit the evaluation makes
    ggml_vk_debug_label(vk_queue_handle * handle, const char * name);


    // call before the command buffer can end, the destructor covers the rest
    void close();


    ~ggml_vk_debug_label() {
        close();
    }

    ggml_vk_debug_label(const ggml_vk_debug_label &) = delete;
    ggml_vk_debug_label & operator=(const ggml_vk_debug_label &) = delete;

private:
    // the constructors check this too, so the name is not built when markers are off
    void begin(vk_context & ctx, const std::string & name);

};

#define UNUSED GGML_UNUSED

struct ggml_backend_vk_device_context {
    size_t device;
    std::string name;
    std::string description;
    bool is_integrated_gpu;
    std::string pci_bus_id;
    int op_offload_min_batch_size;
};

