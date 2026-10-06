#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

#include "types.glsl"

#if defined(DATA_A_Q2_0)
FLOAT_TYPE get_dm(uint ib) {
    return FLOAT_TYPE(data_a[ib / 2].d);
}
#elif defined(DATA_A_Q2_0_G128)
FLOAT_TYPE get_dm(uint ib) {
    return FLOAT_TYPE(data_a[ib / 4].d);
}
#elif defined(DATA_A_Q4_0) || defined(DATA_A_Q5_0) || defined(DATA_A_Q8_0) || defined(DATA_A_IQ1_S) || defined(DATA_A_IQ2_XXS) || defined(DATA_A_IQ2_XS) || defined(DATA_A_IQ2_S) || defined(DATA_A_IQ3_XXS) || defined(DATA_A_IQ3_S) || defined(DATA_A_IQ4_XS) || defined(DATA_A_IQ4_NL)
FLOAT_TYPE get_dm(uint ib) {
    return FLOAT_TYPE(data_a[ib].d);
}
#endif

#if defined(DATA_A_Q4_1) || defined(DATA_A_Q5_1)
FLOAT_TYPEV2 get_dm(uint ib) {
    return FLOAT_TYPEV2(data_a_packed32[ib].dm);
}
#endif

#if defined(DATA_A_MXFP4)
FLOAT_TYPE get_dm(uint ib) {
    return FLOAT_TYPE(e8m0_to_fp32(data_a[ib].e));
}
#endif

#if defined(DATA_A_Q2_K)
FLOAT_TYPEV2 get_dm(uint ib) {
    const uint ib_k = ib / 8;
    return FLOAT_TYPEV2(data_a_packed32[ib_k].dm);
}
#endif

// Each iqs value maps to a 32-bit integer
#if defined(DATA_A_Q2_0)
uint unpack_q2_0(uint bits) {
    // Move bit pairs [1:0], [3:2], [5:4], [7:6] to [1:0], [9:8], [17:16], [25:24].
    bits &= 0xffu;
    bits = (bits | (bits << 12u)) & 0x000f000fu;
    return (bits | (bits << 6u)) & 0x03030303u;
}

i32vec4 repack4(uint ib, uint iqs) {
    const uint qs_idx = (ib & 1u) * 4u + iqs * 2u;
    const uint bits = pack32(u16vec2(data_a_packed16[ib / 2].qs[qs_idx],
                                     data_a_packed16[ib / 2].qs[qs_idx + 1]));
    return i32vec4(unpack_q2_0(bits), unpack_q2_0(bits >> 8u),
                   unpack_q2_0(bits >> 16u), unpack_q2_0(bits >> 24u));
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const float da, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(da * (float(q_sum) * dsb.x - dsb.y / float(sum_divisor)));
}
#elif defined(DATA_A_Q2_0_G128)
uint unpack_q2_0_g128(uint bits) {
    bits &= 0xffu;
    bits = (bits | (bits << 12u)) & 0x000f000fu;
    return (bits | (bits << 6u)) & 0x03030303u;
}

i32vec4 repack4(uint ib, uint iqs) {
    const uint block_idx = ib / 4;
    const uint byte_idx = (ib & 3u) * 8u + iqs * 4u;
    const uint bits = pack32(u8vec4(
        data_a[block_idx].qs[byte_idx],
        data_a[block_idx].qs[byte_idx + 1],
        data_a[block_idx].qs[byte_idx + 2],
        data_a[block_idx].qs[byte_idx + 3]));
    return i32vec4(unpack_q2_0_g128(bits), unpack_q2_0_g128(bits >> 8u),
                   unpack_q2_0_g128(bits >> 16u), unpack_q2_0_g128(bits >> 24u));
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const float da, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(da * (float(q_sum) * dsb.x - dsb.y / float(sum_divisor)));
}
#elif defined(DATA_A_Q2_0)
    const uint qs_idx = (ib & 1u) * 4u + iqs * 2u;
    const uint bits = pack32(u16vec2(data_a_packed16[ib / 2].qs[qs_idx],
                                     data_a_packed16[ib / 2].qs[qs_idx + 1]));
    return i32vec4(unpack_q2_0(bits), unpack_q2_0(bits >> 8u),
                   unpack_q2_0(bits >> 16u), unpack_q2_0(bits >> 24u));
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const float da, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(da * (float(q_sum) * dsb.x - dsb.y / float(sum_divisor)));
}
#endif

#if defined(DATA_A_Q4_0)
// 2-byte loads for Q4_0 blocks (18 bytes)
i32vec2 repack(uint ib, uint iqs) {
    const u16vec2 quants = u16vec2(data_a_packed16[ib].qs[iqs * 2    ],
                                   data_a_packed16[ib].qs[iqs * 2 + 1]);
    const uint32_t vui = pack32(quants);
    return i32vec2( vui       & 0x0F0F0F0F,
                   (vui >> 4) & 0x0F0F0F0F);
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const float da, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(da * (float(q_sum) * dsb.x - (8 / sum_divisor) * dsb.y));
}
#endif

#if defined(DATA_A_Q4_1)
// 4-byte loads for Q4_1 blocks (20 bytes)
i32vec2 repack(uint ib, uint iqs) {
    const uint32_t vui = data_a_packed32[ib].qs[iqs];
    return i32vec2( vui       & 0x0F0F0F0F,
                   (vui >> 4) & 0x0F0F0F0F);
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const vec2 dma, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(float(q_sum) * dma.x * dsb.x + dma.y * dsb.y / sum_divisor);
}
#endif

#if defined(DATA_A_Q5_0)
// 2-byte loads for Q5_0 blocks (22 bytes)
i32vec2 repack(uint ib, uint iqs) {
    const u16vec2 quants = u16vec2(data_a_packed16[ib].qs[iqs * 2    ],
                                   data_a_packed16[ib].qs[iqs * 2 + 1]);
    const uint32_t vui = pack32(quants);
    const int32_t qh = int32_t((uint32_t(data_a_packed16[ib].qh[1]) << 16 | data_a_packed16[ib].qh[0]) >> (4 * iqs));
    const int32_t v0 = int32_t(vui & 0x0F0F0F0F)
                     | ((qh & 0xF) * 0x02040810) & 0x10101010; // (0,1,2,3) -> (4,12,20,28)

    const int32_t v1 = int32_t((vui >> 4) & 0x0F0F0F0F)
                     | (((qh >> 16) & 0xF) * 0x02040810) & 0x10101010; // (16,17,18,19) -> (4,12,20,28)

    return i32vec2(v0, v1);
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const float da, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(da * (float(q_sum) * dsb.x - (16 / sum_divisor) * dsb.y));
}
#endif

#if defined(DATA_A_Q5_1)
// 4-byte loads for Q5_1 blocks (24 bytes)
i32vec2 repack(uint ib, uint iqs) {
    const u16vec2 quants = u16vec2(data_a_packed16[ib].qs[iqs * 2    ],
                                   data_a_packed16[ib].qs[iqs * 2 + 1]);
    const uint32_t vui = pack32(quants);
    const int32_t qh = int32_t(data_a_packed32[ib].qh >> (4 * iqs));
    const int32_t v0 = int32_t(vui & 0x0F0F0F0F)
                     | ((qh & 0xF) * 0x02040810) & 0x10101010; // (0,1,2,3) -> (4,12,20,28)

    const int32_t v1 = int32_t((vui >> 4) & 0x0F0F0F0F)
                     | (((qh >> 16) & 0xF) * 0x02040810) & 0x10101010; // (16,17,18,19) -> (4,12,20,28)

    return i32vec2(v0, v1);
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const vec2 dma, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(float(q_sum) * dma.x * dsb.x + dma.y * dsb.y / sum_divisor);
}
#endif

#if defined(DATA_A_Q8_0)
// 2-byte loads for Q8_0 blocks (34 bytes)
int32_t repack(uint ib, uint iqs) {
    return pack32(i16vec2(data_a_packed16[ib].qs[iqs * 2    ],
                          data_a_packed16[ib].qs[iqs * 2 + 1]));
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const float da, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(float(q_sum) * da * dsb.x);
}
#endif

#if defined(DATA_A_IQ4_NL)
// arifi lane-302 Route B: IQ4_NL on the q8_1 integer dot. Same 18-byte layout and nibble order as Q4_0
// (low nibbles = weights iqs*4..+3, high = +16), so the Q4_0 2-byte loads; the nibbles index the signed
// int8 LUT instead of an offset of 8 (CUDA vec_dot_iq4_nl_q8_1: d_a * d_b * int-dot, no ds.y term).
i32vec2 repack(uint ib, uint iqs) {
    const u16vec2 quants = u16vec2(data_a_packed16[ib].qs[iqs * 2    ],
                                   data_a_packed16[ib].qs[iqs * 2 + 1]);
    return iq4nl_to_i8x8(pack32(quants));
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const float da, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(da * float(q_sum) * dsb.x);
}
#endif

#if defined(DATA_A_MXFP4) || defined(DATA_A_ROCMFP4_FAST)
// 1-byte loads for mxfp4 blocks (17 bytes). ROCmFP4-FAST (17 bytes, one ue4m3 scale) shares the
// nibble layout and swaps the LUT (arifi lane-209, taken-from rocmfpx/main mul_mat_vecq_funcs.glsl).
i32vec2 repack(uint ib, uint iqs) {
    const uint32_t qs = pack32(u8vec4(data_a[ib].qs[iqs * 4    ],
                                      data_a[ib].qs[iqs * 4 + 1],
                                      data_a[ib].qs[iqs * 4 + 2],
                                      data_a[ib].qs[iqs * 4 + 3]));

    const u8vec4 i_a0 = unpack8( qs       & 0x0F0F0F0F);
    const u8vec4 i_a1 = unpack8((qs >> 4) & 0x0F0F0F0F);

#if defined(DATA_A_ROCMFP4_FAST)
    return i32vec2(pack32(i8vec4(kvalues_rocmfp4[i_a0.x], kvalues_rocmfp4[i_a0.y], kvalues_rocmfp4[i_a0.z], kvalues_rocmfp4[i_a0.w])),
                   pack32(i8vec4(kvalues_rocmfp4[i_a1.x], kvalues_rocmfp4[i_a1.y], kvalues_rocmfp4[i_a1.z], kvalues_rocmfp4[i_a1.w])));
#else
    return i32vec2(pack32(i8vec4(kvalues_mxfp4[i_a0.x], kvalues_mxfp4[i_a0.y], kvalues_mxfp4[i_a0.z], kvalues_mxfp4[i_a0.w])),
                   pack32(i8vec4(kvalues_mxfp4[i_a1.x], kvalues_mxfp4[i_a1.y], kvalues_mxfp4[i_a1.z], kvalues_mxfp4[i_a1.w])));
#endif
}

FLOAT_TYPE mul_q8_1(const int32_t q_sum, const float da, const vec2 dsb, const int32_t sum_divisor) {
    return FLOAT_TYPE(da * dsb.x * float(q_sum) * 0.5);
}
#endif

#if defined(DATA_A_ROCMFP4_FAST)
// arifi lane-209 (taken-from rocmfpx/main): the FP4 LUT values are exact ints, so d_a * d_b * int-dot is the
// same product the f32 dequant path forms (dequant_funcs.glsl DATA_A_ROCMFP4_FAST), no 0.5 factor.
struct mmvq_a_t {
    i32vec2    qs;
    FLOAT_TYPE d;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    mmvq_a_t a;
    a.qs = repack(ib_a, iqs);
    a.d  = FLOAT_TYPE(ue4m3_to_fp32(data_a[ib_a].e));
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    const int32_t q_sum = dotPacked4x8EXT(a.qs.x, cache_b_qs[0]) +
                          dotPacked4x8EXT(a.qs.y, cache_b_qs[1]);

    return FLOAT_TYPE(cache_b_ds.x * float(q_sum) * a.d);
}
#elif defined(DATA_A_Q2_0) || defined(DATA_A_Q2_0_G128)
struct mmvq_a_t {
    i32vec4    qs;
    FLOAT_TYPE dm;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    mmvq_a_t a;
    a.qs = repack4(ib_a, iqs);
    a.dm = get_dm(ib_a);
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t q_sum = 0;
    q_sum += dotPacked4x8EXT(a.qs.x, cache_b_qs[0]);
    q_sum += dotPacked4x8EXT(a.qs.y, cache_b_qs[1]);
    q_sum += dotPacked4x8EXT(a.qs.z, cache_b_qs[2]);
    q_sum += dotPacked4x8EXT(a.qs.w, cache_b_qs[3]);

    // 16 quants per call => divide sums by 32/16 = 2
    return mul_q8_1(q_sum, a.dm, cache_b_ds, 2);
}
#elif defined(DATA_A_QUANT_LEGACY) || defined(DATA_A_MXFP4) || defined(DATA_A_IQ4_NL)
#if defined(DATA_A_Q4_1) || defined(DATA_A_Q5_1)
#define MMVQ_DM_TYPE FLOAT_TYPEV2
#else
#define MMVQ_DM_TYPE FLOAT_TYPE
#endif

struct mmvq_a_t {
    i32vec2      qs;
    MMVQ_DM_TYPE dm;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    mmvq_a_t a;
#if QUANT_R == 2
    a.qs = repack(ib_a, iqs);
#else
    a.qs.x = repack(ib_a, iqs * 2);
    a.qs.y = repack(ib_a, iqs * 2 + 1);
#endif
    a.dm = get_dm(ib_a);
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t q_sum = 0;
    q_sum += dotPacked4x8EXT(a.qs.x,
                             cache_b_qs[0]);
    q_sum += dotPacked4x8EXT(a.qs.y,
                             cache_b_qs[1]);

    // 2 quants per call => divide sums by 8/2 = 4
    return mul_q8_1(q_sum, a.dm, cache_b_ds, 4);
}
#endif

#if defined(DATA_A_Q2_K)
// 4-byte loads for Q2_K blocks (84 bytes)
i32vec4 repack4(uint ib, uint iqs) {
    const uint ib_k = ib / 8;
    const uint iqs_k = (ib % 8) * 8 + iqs;

    const uint qs_idx = (iqs_k / 32) * 8 + (iqs_k % 8);
    const uint qs_shift = ((iqs_k % 32) / 8) * 2;

    return i32vec4((data_a_packed32[ib_k].qs[qs_idx    ] >> qs_shift) & 0x03030303,
                   (data_a_packed32[ib_k].qs[qs_idx + 1] >> qs_shift) & 0x03030303,
                   (data_a_packed32[ib_k].qs[qs_idx + 2] >> qs_shift) & 0x03030303,
                   (data_a_packed32[ib_k].qs[qs_idx + 3] >> qs_shift) & 0x03030303);
}

uint8_t get_scale(uint ib, uint iqs) {
    const uint ib_k = ib / 8;
    const uint iqs_k = (ib % 8) * 8 + iqs;

    return data_a[ib_k].scales[iqs_k / 4];
}

struct mmvq_a_t {
    i32vec4 qs;
    uint8_t scale;
    vec2    dm;
    int32_t scale_m;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    mmvq_a_t a;
    a.qs      = repack4(ib_a, iqs * 4);
    a.scale   = get_scale(ib_a, iqs * 4);
    a.dm      = vec2(get_dm(ib_a));
    a.scale_m = int32_t(a.scale >> 4) * 0x01010101; // Duplicate 8-bit value across 32-bits.
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t sum_d = 0;
    int32_t sum_m = 0;

    sum_d += dotPacked4x8EXT(a.qs.x, cache_b_qs[0]) * (a.scale & 0xF);
    sum_m += dotPacked4x8EXT(a.scale_m, cache_b_qs[0]);

    sum_d += dotPacked4x8EXT(a.qs.y, cache_b_qs[1]) * (a.scale & 0xF);
    sum_m += dotPacked4x8EXT(a.scale_m, cache_b_qs[1]);

    sum_d += dotPacked4x8EXT(a.qs.z, cache_b_qs[2]) * (a.scale & 0xF);
    sum_m += dotPacked4x8EXT(a.scale_m, cache_b_qs[2]);

    sum_d += dotPacked4x8EXT(a.qs.w, cache_b_qs[3]) * (a.scale & 0xF);
    sum_m += dotPacked4x8EXT(a.scale_m, cache_b_qs[3]);

    return FLOAT_TYPE(float(cache_b_ds.x) * (float(a.dm.x) * float(sum_d) - float(a.dm.y) * float(sum_m)));
}
#endif

#if defined(DATA_A_Q3_K)
// 2-byte loads for Q3_K blocks (110 bytes)
i32vec4 repack4(uint ib, uint iqs) {
    const uint ib_k = ib / 8;
    const uint iqs_k = (ib % 8) * 8 + iqs;

    const uint qs_idx = (iqs_k / 32) * 8 + (iqs_k % 8);
    const uint qs_shift = ((iqs_k % 32) / 8) * 2;
    const uint hm_shift = iqs_k / 8;

    const uvec4 qs = uvec4( uint32_t(data_a_packed16[ib_k].qs[qs_idx * 2    ]) |
                           (uint32_t(data_a_packed16[ib_k].qs[qs_idx * 2 + 1]) << 16),
                            uint32_t(data_a_packed16[ib_k].qs[qs_idx * 2 + 2]) |
                           (uint32_t(data_a_packed16[ib_k].qs[qs_idx * 2 + 3]) << 16),
                            uint32_t(data_a_packed16[ib_k].qs[qs_idx * 2 + 4]) |
                           (uint32_t(data_a_packed16[ib_k].qs[qs_idx * 2 + 5]) << 16),
                            uint32_t(data_a_packed16[ib_k].qs[qs_idx * 2 + 6]) |
                           (uint32_t(data_a_packed16[ib_k].qs[qs_idx * 2 + 7]) << 16));

    const uvec4 hmask = uvec4( uint32_t(data_a_packed16[ib_k].hmask[iqs * 2    ]) |
                              (uint32_t(data_a_packed16[ib_k].hmask[iqs * 2 + 1]) << 16),
                               uint32_t(data_a_packed16[ib_k].hmask[iqs * 2 + 2]) |
                              (uint32_t(data_a_packed16[ib_k].hmask[iqs * 2 + 3]) << 16),
                               uint32_t(data_a_packed16[ib_k].hmask[iqs * 2 + 4]) |
                              (uint32_t(data_a_packed16[ib_k].hmask[iqs * 2 + 5]) << 16),
                               uint32_t(data_a_packed16[ib_k].hmask[iqs * 2 + 6]) |
                              (uint32_t(data_a_packed16[ib_k].hmask[iqs * 2 + 7]) << 16));

    // bitwise OR to add 4 if hmask is set, subtract later
    const uint vals0 = ((    qs.x >> qs_shift) & 0x03030303) |
                       (((hmask.x >> hm_shift) & 0x01010101) << 2);
    const uint vals1 = ((    qs.y >> qs_shift) & 0x03030303) |
                       (((hmask.y >> hm_shift) & 0x01010101) << 2);
    const uint vals2 = ((    qs.z >> qs_shift) & 0x03030303) |
                       (((hmask.z >> hm_shift) & 0x01010101) << 2);
    const uint vals3 = ((    qs.w >> qs_shift) & 0x03030303) |
                       (((hmask.w >> hm_shift) & 0x01010101) << 2);

    // Subtract 4 by twiddling bits rather than using re-packing as mesa
    // compiles repacking poorly.
    return i32vec4(int32_t(((vals0 ^ 0x80808080) - 0x04040404) ^ 0x80808080),
                   int32_t(((vals1 ^ 0x80808080) - 0x04040404) ^ 0x80808080),
                   int32_t(((vals2 ^ 0x80808080) - 0x04040404) ^ 0x80808080),
                   int32_t(((vals3 ^ 0x80808080) - 0x04040404) ^ 0x80808080));
}

float get_d_scale(uint ib, uint iqs) {
    const uint ib_k = ib / 8;
    const uint iqs_k = (ib % 8) * 8 + iqs;
    const uint is = iqs_k / 4;

    const int8_t scale = int8_t(((data_a[ib_k].scales[is % 8      ] >> (4 * (is / 8))) & 0x0F0F) |
                               (((data_a[ib_k].scales[8 + (is % 4)] >> (2 * (is / 4))) & 0x0303) << 4));
    return float(data_a[ib_k].d) * float(scale - 32);
}

struct mmvq_a_t {
    i32vec4 qs;
    float   d_scale;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    mmvq_a_t a;
    a.qs      = repack4(ib_a, iqs * 4);
    a.d_scale = get_d_scale(ib_a, iqs * 4);
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t q_sum = 0;

    q_sum += dotPacked4x8EXT(a.qs.x, cache_b_qs[0]);
    q_sum += dotPacked4x8EXT(a.qs.y, cache_b_qs[1]);
    q_sum += dotPacked4x8EXT(a.qs.z, cache_b_qs[2]);
    q_sum += dotPacked4x8EXT(a.qs.w, cache_b_qs[3]);

    return FLOAT_TYPE(float(cache_b_ds.x) * a.d_scale * float(q_sum));
}
#endif

#if defined(DATA_A_Q4_K) || defined(DATA_A_Q5_K)
// 4-byte loads for Q4_K blocks (144 bytes) and Q5_K blocks (176 bytes)
i32vec4 repack4(uint ib, uint iqs) {
    const uint ib_k = ib / 8;
    const uint iqs_k = (ib % 8) * 8 + iqs;

    const uint qs_idx = (iqs_k / 16) * 8 + (iqs_k % 8);
    const uint qs_shift = ((iqs_k % 16) / 8) * 4;

#if defined(DATA_A_Q4_K)
    const uint32_t vals0 = (data_a_packed32[ib_k].qs[qs_idx    ] >> qs_shift) & 0x0F0F0F0F;
    const uint32_t vals1 = (data_a_packed32[ib_k].qs[qs_idx + 1] >> qs_shift) & 0x0F0F0F0F;
    const uint32_t vals2 = (data_a_packed32[ib_k].qs[qs_idx + 2] >> qs_shift) & 0x0F0F0F0F;
    const uint32_t vals3 = (data_a_packed32[ib_k].qs[qs_idx + 3] >> qs_shift) & 0x0F0F0F0F;

    return i32vec4(vals0, vals1, vals2, vals3);
#else // defined(DATA_A_Q5_K)
    const uint qh_idx = iqs;
    const uint qh_shift = iqs_k / 8;

    return i32vec4(((data_a_packed32[ib_k].qs[qs_idx    ] >> qs_shift) & 0x0F0F0F0F) |
                  (((data_a_packed32[ib_k].qh[qh_idx    ] >> qh_shift) & 0x01010101) << 4),
                   ((data_a_packed32[ib_k].qs[qs_idx + 1] >> qs_shift) & 0x0F0F0F0F) |
                  (((data_a_packed32[ib_k].qh[qh_idx + 1] >> qh_shift) & 0x01010101) << 4),
                   ((data_a_packed32[ib_k].qs[qs_idx + 2] >> qs_shift) & 0x0F0F0F0F) |
                  (((data_a_packed32[ib_k].qh[qh_idx + 2] >> qh_shift) & 0x01010101) << 4),
                   ((data_a_packed32[ib_k].qs[qs_idx + 3] >> qs_shift) & 0x0F0F0F0F) |
                  (((data_a_packed32[ib_k].qh[qh_idx + 3] >> qh_shift) & 0x01010101) << 4));
#endif
}

vec2 get_dm_scale(uint ib, uint iqs) {
    const uint ib_k = ib / 8;
    const uint iqs_k = (ib % 8) * 8 + iqs;
    const uint is = iqs_k / 8;

    const uvec3 scales = uvec3(data_a_packed32[ib_k].scales[0],
                               data_a_packed32[ib_k].scales[1],
                               data_a_packed32[ib_k].scales[2]);
    const uint scalesoffs = (is & 3) * 8;

    const uint scidx0 = (is < 4) ? 0 : 2;
    const uint scidxshift0 = scalesoffs;
    const uint scidxshift1 = (is < 4) ? scalesoffs : scalesoffs + 2;
    const uint mbidx0 = (is < 4) ? 1 : 2;
    const uint mbidxshift0 = (is < 4) ? scalesoffs : scalesoffs + 4;
    const uint mbidxshift1 = (is < 4) ? scalesoffs : scalesoffs + 2;

    const uint8_t sc    = uint8_t(((scales[scidx0] >> scidxshift0) & 0xF) | ((scales[0] >> scidxshift1) & 0x30));
    const uint8_t mbyte = uint8_t(((scales[mbidx0] >> mbidxshift0) & 0xF) | ((scales[1] >> mbidxshift1) & 0x30));
    u8vec2 scale_dm = u8vec2(sc, mbyte);

    return FLOAT_TYPEV2(data_a_packed32[ib_k].dm) * FLOAT_TYPEV2(scale_dm);
}

struct mmvq_a_t {
    i32vec4 qs;
    vec2    dm_scale;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    mmvq_a_t a;
    a.qs       = repack4(ib_a, iqs * 4);
    a.dm_scale = get_dm_scale(ib_a, iqs * 4);
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t q_sum = 0;

    q_sum += dotPacked4x8EXT(a.qs.x, cache_b_qs[0]);
    q_sum += dotPacked4x8EXT(a.qs.y, cache_b_qs[1]);
    q_sum += dotPacked4x8EXT(a.qs.z, cache_b_qs[2]);
    q_sum += dotPacked4x8EXT(a.qs.w, cache_b_qs[3]);

    if (ARIFI_MMVQ_PRECISE != 0) {
        precise float r = float(cache_b_ds.x) * float(a.dm_scale.x) * float(q_sum) - float(a.dm_scale.y) * float(cache_b_ds.y / 2);
        return FLOAT_TYPE(r);
    }
    return FLOAT_TYPE(float(cache_b_ds.x) * float(a.dm_scale.x) * float(q_sum) - float(a.dm_scale.y) * float(cache_b_ds.y / 2));
}
#endif

#if defined(DATA_A_Q6_K)
// 2-byte loads for Q6_K blocks (210 bytes)
i32vec4 repack4(uint ib, uint iqs) {
    const uint ib_k = ib / 8;
    const uint iqs_k = (ib % 8) * 8 + iqs;

    const uint ql_idx = (iqs_k / 32) * 16 + iqs_k % 16;
    const uint ql_shift = ((iqs_k % 32) / 16) * 4;

    const uint qh_idx = (iqs_k / 32) * 8 + iqs;
    const uint qh_shift = ((iqs_k % 32) / 8) * 2;

    const uvec4 ql = uvec4( uint32_t(data_a_packed16[ib_k].ql[ql_idx * 2    ]) |
                           (uint32_t(data_a_packed16[ib_k].ql[ql_idx * 2 + 1]) << 16),
                            uint32_t(data_a_packed16[ib_k].ql[ql_idx * 2 + 2]) |
                           (uint32_t(data_a_packed16[ib_k].ql[ql_idx * 2 + 3]) << 16),
                            uint32_t(data_a_packed16[ib_k].ql[ql_idx * 2 + 4]) |
                           (uint32_t(data_a_packed16[ib_k].ql[ql_idx * 2 + 5]) << 16),
                            uint32_t(data_a_packed16[ib_k].ql[ql_idx * 2 + 6]) |
                           (uint32_t(data_a_packed16[ib_k].ql[ql_idx * 2 + 7]) << 16));

    const uvec4 qh = uvec4( uint32_t(data_a_packed16[ib_k].qh[qh_idx * 2    ]) |
                           (uint32_t(data_a_packed16[ib_k].qh[qh_idx * 2 + 1]) << 16),
                            uint32_t(data_a_packed16[ib_k].qh[qh_idx * 2 + 2]) |
                           (uint32_t(data_a_packed16[ib_k].qh[qh_idx * 2 + 3]) << 16),
                            uint32_t(data_a_packed16[ib_k].qh[qh_idx * 2 + 4]) |
                           (uint32_t(data_a_packed16[ib_k].qh[qh_idx * 2 + 5]) << 16),
                            uint32_t(data_a_packed16[ib_k].qh[qh_idx * 2 + 6]) |
                           (uint32_t(data_a_packed16[ib_k].qh[qh_idx * 2 + 7]) << 16));

    const uint vals0 = (( ql.x >> ql_shift) & 0x0F0F0F0F) |
                       (((qh.x >> qh_shift) & 0x03030303) << 4);
    const uint vals1 = (( ql.y >> ql_shift) & 0x0F0F0F0F) |
                       (((qh.y >> qh_shift) & 0x03030303) << 4);
    const uint vals2 = (( ql.z >> ql_shift) & 0x0F0F0F0F) |
                       (((qh.z >> qh_shift) & 0x03030303) << 4);
    const uint vals3 = (( ql.w >> ql_shift) & 0x0F0F0F0F) |
                       (((qh.w >> qh_shift) & 0x03030303) << 4);

    // Subtract 32 by twiddling bits rather than using re-packing as mesa
    // compiles repacking poorly.
    return i32vec4(int32_t(((vals0 ^ 0x80808080) - 0x20202020) ^ 0x80808080),
                   int32_t(((vals1 ^ 0x80808080) - 0x20202020) ^ 0x80808080),
                   int32_t(((vals2 ^ 0x80808080) - 0x20202020) ^ 0x80808080),
                   int32_t(((vals3 ^ 0x80808080) - 0x20202020) ^ 0x80808080));
}

float get_d_scale(uint ib, uint iqs) {
    const uint ib_k = ib / 8;
    const uint iqs_k = (ib % 8) * 8 + iqs;
    return float(data_a[ib_k].d) * float(data_a[ib_k].scales[iqs_k / 4]);
}

struct mmvq_a_t {
    i32vec4 qs;
    float   d_scale;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    mmvq_a_t a;
    a.qs      = repack4(ib_a, iqs * 4);
    a.d_scale = get_d_scale(ib_a, iqs * 4);
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t q_sum = 0;

    q_sum += dotPacked4x8EXT(a.qs.x, cache_b_qs[0]);
    q_sum += dotPacked4x8EXT(a.qs.y, cache_b_qs[1]);
    q_sum += dotPacked4x8EXT(a.qs.z, cache_b_qs[2]);
    q_sum += dotPacked4x8EXT(a.qs.w, cache_b_qs[3]);

    return FLOAT_TYPE(float(cache_b_ds.x) * float(a.d_scale) * float(q_sum));
}
#endif

#if defined(DATA_A_IQ4_XS) && defined(ARIFI_IQ4XS_UPSTREAM)
// arifi lane-296: upstream b1ff4ca23's IQ4_XS mmvq_dot_product, split into the load/dot halves this
// framework calls. Same integer sum and the same final expression, so it is bit-identical to upstream.
struct mmvq_a_t {
    i32vec4 qs0;
    i32vec4 qs1;
    float   d;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    const uint ib = ib_a / 8;
    const uint ib32 = ib_a % 8;

    mmvq_a_t a;
    [[unroll]] for (uint j = 0; j < 4; ++j) {
        const i32vec2 v = iq4nl_to_i8x8(data_a_packed32[ib].qs[4 * ib32 + j]);
        a.qs0[j] = v.x;
        a.qs1[j] = v.y;
    }

    const uint sl = (data_a_packed32[ib].scales_l >> (4 * ib32)) & 0xF;
    const uint sh = (data_a_packed32[ib].scales_h >> (2 * ib32)) & 3;
    a.d = float(data_a[ib].d) * float(int(sl | (sh << 4)) - 32);
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t q_sum = 0;
    [[unroll]] for (uint j = 0; j < 4; ++j) {
        q_sum += dotPacked4x8EXT(a.qs0[j], cache_b_qs[j]);
        q_sum += dotPacked4x8EXT(a.qs1[j], cache_b_qs[j + 4]);
    }

    return FLOAT_TYPE(float(cache_b_ds.x) * a.d * float(q_sum));
}
#elif defined(DATA_A_IQ4_XS)
// arifi lane-262 / R75: IQ4_XS on the q8_1 integer-dot framework. Structural template = MXFP4's
// mmvq (the in-tree LUT-based 4-bit type already on this path, :177) for the nibble->LUT->pack32
// step, and Q4_K's (:421) for the superblock indexing. Arithmetic copied from the CPU/CUDA
// references in this tree, not re-derived:
//   ggml/src/ggml-cpu/arch/x86/quants.c ggml_vec_dot_iq4_xs_q8_K -- ls per 32-weight sub-block =
//     ((scales_l[ib32/2] >> 4*(ib32%2)) & 0xf) | (((scales_h >> 2*ib32) & 3) << 4), dl = d*(ls-32)
//   ggml/src/ggml-cuda/vecdotq.cuh vec_dot_iq4_xs_q8_1 -- sumi = int-dot of LUT values with the
//     q8_1 quants, result = d * (ls-32) * ds.x * sumi. There is NO ds.y (min/sum) term: the
//     kvalues_iq4nl table is a signed LUT with no zero-point, so nothing multiplies the q8_1 sum.
//
// SUMMATION ORDER (documented per the brief): four dotPacked4x8EXT partial sums accumulated into a
// single int32 q_sum in ascending byte order, then ONE float multiply by (ds.x * dl). The f32
// shader instead accumulates a 16-term float fma chain per sub-block and then fma's by dl, so the
// two paths are NOT bit-identical by construction -- the integer path is exact up to the final
// multiply, the f32 one rounds every term.
//
// LAYOUT. In this framework `ib` is a 32-WIDE block index (a_offset is scaled by
// QUANT_K/QUANT_K_Q8_1 in mul_mat_vecq.comp), while block_iq4_xs is a 256-weight superblock:
// ib_k = ib/8 is the superblock, ib32 = ib%8 the sub-block inside it. Within sub-block ib32 the 16
// qs bytes at packed32 words [4*ib32 .. 4*ib32+3] hold weights 0..15 in their LOW nibbles and
// weights 16..31 in their HIGH nibbles (the order mul_mat_vec_iq4_xs.comp uses: y1_idx low,
// y2_idx = y1_idx+16 high). K_PER_ITER is 16, so iqs == 0 is the low half and iqs == 1 the high
// half -- exactly the 16 activations cache_b_block() loads at b_qs_idx.
//
// NOT get_dm(): the shared get_dm() arm at :15 lists DATA_A_IQ4_XS and returns data_a[ib].d, which
// indexes a 32-wide block into a 256-wide struct. That arm is dead for every other path; it would
// compile clean here and read the wrong superblock, so this type reads d itself below.
i32vec4 repack4(uint ib, uint iqs) {
    const uint ib_k    = ib / 8;
    const uint qs_idx  = (ib % 8) * 4;
    const uint shift   = iqs * 4;   // 0 = low nibbles (weights 0..15), 4 = high (weights 16..31)

    const u8vec4 n0 = unpack8((data_a_packed32[ib_k].qs[qs_idx    ] >> shift) & 0x0F0F0F0F);
    const u8vec4 n1 = unpack8((data_a_packed32[ib_k].qs[qs_idx + 1] >> shift) & 0x0F0F0F0F);
    const u8vec4 n2 = unpack8((data_a_packed32[ib_k].qs[qs_idx + 2] >> shift) & 0x0F0F0F0F);
    const u8vec4 n3 = unpack8((data_a_packed32[ib_k].qs[qs_idx + 3] >> shift) & 0x0F0F0F0F);

    return i32vec4(pack32(i8vec4(kvalues_iq4nl_i8[n0.x], kvalues_iq4nl_i8[n0.y], kvalues_iq4nl_i8[n0.z], kvalues_iq4nl_i8[n0.w])),
                   pack32(i8vec4(kvalues_iq4nl_i8[n1.x], kvalues_iq4nl_i8[n1.y], kvalues_iq4nl_i8[n1.z], kvalues_iq4nl_i8[n1.w])),
                   pack32(i8vec4(kvalues_iq4nl_i8[n2.x], kvalues_iq4nl_i8[n2.y], kvalues_iq4nl_i8[n2.z], kvalues_iq4nl_i8[n2.w])),
                   pack32(i8vec4(kvalues_iq4nl_i8[n3.x], kvalues_iq4nl_i8[n3.y], kvalues_iq4nl_i8[n3.z], kvalues_iq4nl_i8[n3.w])));
}

FLOAT_TYPE get_dl(uint ib) {
    const uint ib_k = ib / 8;
    const uint ib32 = ib % 8;

    const uint sl = (data_a_packed32[ib_k].scales_l >> (4 * ib32)) & 0xF;
    const uint sh = (data_a[ib_k].scales_h         >> (2 * ib32)) & 3;

    return FLOAT_TYPE(data_a[ib_k].d) * FLOAT_TYPE(int(sl | (sh << 4)) - 32);
}

struct mmvq_a_t {
    i32vec4    qs;
    FLOAT_TYPE dl;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    mmvq_a_t a;
    a.qs = repack4(ib_a, iqs);
    a.dl = get_dl(ib_a);
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t q_sum = 0;

    q_sum += dotPacked4x8EXT(a.qs.x, cache_b_qs[0]);
    q_sum += dotPacked4x8EXT(a.qs.y, cache_b_qs[1]);
    q_sum += dotPacked4x8EXT(a.qs.z, cache_b_qs[2]);
    q_sum += dotPacked4x8EXT(a.qs.w, cache_b_qs[3]);

    return FLOAT_TYPE(float(cache_b_ds.x) * float(a.dl) * float(q_sum));
}
#endif

#if defined(DATA_A_IQ1_S)
void repack8(uint ib, uint iqs, out i32vec4 out0, out i32vec4 out1) {
    const uint ib32 = iqs / 32;

    const uint qh = data_a[ib].qh[ib32];

    const uint qs16_0 = data_a_packed16[ib].qs[(4 * ib32 + 0) / 2];
    const uint qs16_1 = data_a_packed16[ib].qs[(4 * ib32 + 2) / 2];

    const uint qs0 = qs16_0 & 0xFF;
    const uint qs1 = qs16_0 >> 8;
    const uint qs2 = qs16_1 & 0xFF;
    const uint qs3 = qs16_1 >> 8;

    const uint hi0 = bitfieldExtract(qh, 3 * int(0), 3);
    const uint hi1 = bitfieldExtract(qh, 3 * int(1), 3);
    const uint hi2 = bitfieldExtract(qh, 3 * int(2), 3);
    const uint hi3 = bitfieldExtract(qh, 3 * int(3), 3);

    const int32_t grid0 = int32_t(iq1s_grid_gpu[qs0 | (hi0 << 8)]);
    const int32_t grid1 = int32_t(iq1s_grid_gpu[qs1 | (hi1 << 8)]);
    const int32_t grid2 = int32_t(iq1s_grid_gpu[qs2 | (hi2 << 8)]);
    const int32_t grid3 = int32_t(iq1s_grid_gpu[qs3 | (hi3 << 8)]);

    out0 = i32vec4((grid0 >> 0) & 0x0F0F0F0F,
                   (grid0 >> 4) & 0x0F0F0F0F,
                   (grid1 >> 0) & 0x0F0F0F0F,
                   (grid1 >> 4) & 0x0F0F0F0F);
    out1 = i32vec4((grid2 >> 0) & 0x0F0F0F0F,
                   (grid2 >> 4) & 0x0F0F0F0F,
                   (grid3 >> 0) & 0x0F0F0F0F,
                   (grid3 >> 4) & 0x0F0F0F0F);
}

vec2 get_dm(uint ib, uint iqs) {
    const uint ib32 = iqs / 32;

    const uint qh = data_a[ib].qh[ib32];
    const float delta = ((qh & 0x8000) != 0) ? -IQ1S_DELTA : IQ1S_DELTA;

    const float d = float(data_a[ib].d);
    const float dl = d * float(2 * bitfieldExtract(qh, 12, 3) + 1);

    // the -1 cancels out the bias in iq1s_grid_gpu
    return FLOAT_TYPEV2(dl, dl * (delta - 1));
}

struct mmvq_a_t {
    i32vec4 qs0;
    i32vec4 qs1;
    vec2    dm;
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    const uint ib_k = ib_a / 8;
    const uint iqs_k = (ib_a % 8) * 32 + iqs * 32;

    mmvq_a_t a;
    repack8(ib_k, iqs_k, a.qs0, a.qs1);
    a.dm = get_dm(ib_k, iqs_k);
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    int32_t q_sum = 0;

    q_sum += dotPacked4x8EXT(a.qs0.x, cache_b_qs[0]);
    q_sum += dotPacked4x8EXT(a.qs0.y, cache_b_qs[1]);
    q_sum += dotPacked4x8EXT(a.qs0.z, cache_b_qs[2]);
    q_sum += dotPacked4x8EXT(a.qs0.w, cache_b_qs[3]);
    q_sum += dotPacked4x8EXT(a.qs1.x, cache_b_qs[4]);
    q_sum += dotPacked4x8EXT(a.qs1.y, cache_b_qs[5]);
    q_sum += dotPacked4x8EXT(a.qs1.z, cache_b_qs[6]);
    q_sum += dotPacked4x8EXT(a.qs1.w, cache_b_qs[7]);

    return FLOAT_TYPE(float(cache_b_ds.x) * float(a.dm.x) * float(q_sum) + float(a.dm.y) * float(cache_b_ds.y));
}
#endif

#if defined(DATA_A_IQ1_M)
struct mmvq_a_t {
    int32_t grid[4];
    float   dl[4];
    float   delta[4];
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    const uint ib_k = ib_a / 8;
    const uint iqs_k = (ib_a % 8) * 32 + iqs * 32;

    const uint ib32 = iqs_k / 32;
    const uint ib64 = ib32 / 2;

    const uint16_t[4] scales = data_a[ib_k].scales;
    const u16vec4 s = u16vec4(scales[0], scales[1], scales[2], scales[3]) >> 12;
    const float d = float(unpackHalf2x16(s.x | (s.y << 4) | (s.z << 8) | (s.w << 12)).x);

    const uint qs32 = data_a_packed32[ib_k].qs[ib32];
    const uint qh16 = data_a_packed16[ib_k].qh[ib32];

    const uint sc = data_a[ib_k].scales[ib64];

    mmvq_a_t a;
    [[unroll]] for (int l = 0; l < 4; ++l) {
        const uint ib16 = 2 * ib32 + l / 2;
        a.dl[l] = d * (2 * bitfieldExtract(sc, 3 * int(ib16 & 3), 3) + 1);
        const uint qh = qh16 >> (4 * l);
        const uint qs = (qs32 >> (8 * l)) & 0xFF;
        a.delta[l] = ((qh & 8) != 0) ? -IQ1M_DELTA : IQ1M_DELTA;

        a.grid[l] = int32_t(iq1s_grid_gpu[qs | ((qh & 7) << 8)]);
    }
    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    float sum = 0;
    [[unroll]] for (int l = 0; l < 4; ++l) {
        int32_t q_sum = 0;
        q_sum += dotPacked4x8EXT((a.grid[l] >> 0) & 0x0F0F0F0F, cache_b_qs[2 * l + 0]);
        q_sum += dotPacked4x8EXT((a.grid[l] >> 4) & 0x0F0F0F0F, cache_b_qs[2 * l + 1]);

        int32_t y_sum = 0;
        y_sum += dotPacked4x8EXT(int(0x01010101), cache_b_qs[2 * l + 0]);
        y_sum += dotPacked4x8EXT(int(0x01010101), cache_b_qs[2 * l + 1]);

        // the -1 cancels out the bias in iq1s_grid_gpu
        sum += a.dl[l] * (q_sum + y_sum * (a.delta[l] - 1));
    }
    sum *= float(cache_b_ds.x);

    return sum;
}
#endif

#if defined(DATA_A_SX8)
// S-X8 v4.3 (type-id 57) integer-dot mat-vec. lane-230 / R46.
//
// One call consumes a WHOLE 32-weight block against a whole Q8_1 activation block,
// so the header, the four range strategies and the two planes are read once each.
//
// The affine decode is w = rlo_s + step_s * L, with (rlo_s, step_s) fixed per 8-weight
// sub-block. Over one sub-block that gives
//
//     sum(w * a) = d_b * ( step_s * sum(L * q) + rlo_s * sum(q) )
//
// so TWO integer sums per sub-block, not one. The Q8_1 ds.y term (d_b * sum of all 32
// quants) cannot stand in for sum(q) here: rlo changes every 8 weights, so the four
// partial sums are needed separately. sum(q) is a dot against 0x01010101 -- the same
// instruction, no extra loads.
//
// Levels are in [0,63] and quants in [-127,127], so a signed 8-bit packed dot cannot
// overflow its 32-bit accumulator (worst case 63*127*4 = 32004 per call).
//
// Decode constants stay in sx8_range() / sx8_levels4() (types.glsl), shared with the
// float mat-vec and dequant_sx8 -- the F-110 single-home rule. A planted error there
// must turn every S-X8 path red at once, which is the only way the oracle proves it ran.
struct mmvq_a_t {
    int32_t lv[8];     // the four sub-blocks' two packed level words each
    float   rlo[4];
    float   step[4];
};

mmvq_a_t mmvq_load_a(const uint ib_a, const uint iqs) {
    const float dlo = float(data_a[ib_a].dmin);
    const float dhi = float(data_a[ib_a].dmax);
    const uint  cfg = uint(data_a[ib_a].config);

    mmvq_a_t a;

    [[unroll]] for (uint sb = 0; sb < 4u; ++sb) {
        float rlo, step;
        sx8_range(dlo, dhi, (cfg >> (sb * 2u)) & 3u, rlo, step);
        a.rlo[sb]  = rlo;
        a.step[sb] = step;

        [[unroll]] for (uint g = 0; g < 2u; ++g) {
            const uint qh_pair = uint(data_a[ib_a].qh[sb * 4u + g * 2u])
                               | (uint(data_a[ib_a].qh[sb * 4u + g * 2u + 1u]) << 8u);
            a.lv[sb * 2u + g] = int32_t(sx8_levels4(qh_pair, uint(data_a[ib_a].ql[sb * 2u + g])));
        }
    }

    return a;
}

FLOAT_TYPE mmvq_dot_a(const mmvq_a_t a) {
    float acc = 0.0;

    [[unroll]] for (uint sb = 0; sb < 4u; ++sb) {
        int32_t lq = 0;   // sum of level * quant over the 8 weights
        int32_t sq = 0;   // sum of quant          over the 8 weights

        [[unroll]] for (uint g = 0; g < 2u; ++g) {
            const int32_t bq = cache_b_qs[sb * 2u + g];

            lq += dotPacked4x8EXT(a.lv[sb * 2u + g], bq);
            sq += dotPacked4x8EXT(0x01010101, bq);
        }

        acc += a.step[sb] * float(lq) + a.rlo[sb] * float(sq);
    }

    return FLOAT_TYPE(float(cache_b_ds.x) * acc);
}
#endif

// arifi lane-235 / R48b: the inherited entry point, now a composition of the two halves. The
// OFF arm of mul_mat_vecq.comp calls this; the ON arm calls mmvq_load_a() once per (row,
// k-slice) and mmvq_dot_a() once per column. Both run the same expressions in the same order.
FLOAT_TYPE mmvq_dot_product(const uint ib_a, const uint iqs) {
    return mmvq_dot_a(mmvq_load_a(ib_a, iqs));
}
