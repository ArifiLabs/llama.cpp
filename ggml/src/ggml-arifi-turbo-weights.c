// TurboQuant TQ3_1S / TQ4_1S serialized WEIGHT formats (type-ids 45/46).
//
// Ported by ArifiLabs from llama-cpp-turboquant @ c26cbdffc, weight-quant half only.
// The KV-cache turbo types from the same source file are deliberately NOT ported: they
// measured a loss on this estate's hardware (plain q4_0 KV is smaller AND faster) and they
// are runtime-only types that belong in the 200+ block per docs/TYPE-ID-ALLOCATION.md.
//
// Unlike the KV codec, the weight formats are self-contained: dequantize applies the inverse
// randomized Hadamard transform itself, so no graph-level rotation op is required and these
// behave as ordinary drop-in quant types.
//
// Compiled only when GGML_ARIFI_TURBO_WEIGHT_QUANTS is ON. The enum ids stay unconditional
// (ABI); only the implementation is gated - the same rule the ROCmFP formats follow, because
// vec_dot lives in type_traits_cpu[] and is bound at LINK time (LL-250).

#include "ggml-common.h"
#include "ggml-impl.h"
#include "ggml-quants.h"

#include <assert.h>
#include <math.h>
#include <string.h>

/* ================================================================== */
/* TQ3_1S / TQ4_1S: WHT-rotated weight quantization                  */
/* ================================================================== */

/* Lloyd-Max centroids for N(0,1) — shared with Metal shaders */
static const float TQ3_0_CENTROIDS[8] = {
    -1.996684f, -1.291398f, -0.740341f, -0.247508f,
     0.230106f,  0.725222f,  1.277503f,  1.988943f
};

static const float TQ4_0_CENTROIDS[16] = {
    -2.732590f, -2.069017f, -1.618046f, -1.256231f,
    -0.942340f, -0.656759f, -0.388048f, -0.128395f,
     0.128395f,  0.388048f,  0.656759f,  0.942340f,
     1.256231f,  1.618046f,  2.069017f,  2.732590f,
};

/* WHT sign pattern (golden ratio hash, 32-element blocks) — shared by TQ3 and TQ4 */
static const float TQ3_0_SIGNS[32] = {
    +1.0f, -1.0f, +1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f,
    -1.0f, -1.0f, +1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f,
    -1.0f, -1.0f, +1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f,
    -1.0f, +1.0f, +1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f,
};

#define TQ_BLOCK_SIZE 32
#define TQ_INV_SQRT32 0.17677669529663688f  /* 1/sqrt(32) */

/* Forward RHT: sign flips -> WHT butterfly -> normalize */
static void tq3_0_rht_forward(float * buf) {
    for (int i = 0; i < TQ_BLOCK_SIZE; i++) buf[i] *= TQ3_0_SIGNS[i];
    for (int step = 1; step < TQ_BLOCK_SIZE; step <<= 1) {
        for (int i = 0; i < TQ_BLOCK_SIZE; i += step << 1) {
            for (int j = i; j < i + step; j++) {
                float a = buf[j], b = buf[j + step];
                buf[j]     = a + b;
                buf[j + step] = a - b;
            }
        }
    }
    for (int i = 0; i < TQ_BLOCK_SIZE; i++) buf[i] *= TQ_INV_SQRT32;
}

/* Inverse RHT: WHT butterfly -> normalize + unsign */
static void tq3_0_rht_inverse(float * buf) {
    for (int step = 1; step < TQ_BLOCK_SIZE; step <<= 1) {
        for (int i = 0; i < TQ_BLOCK_SIZE; i += step << 1) {
            for (int j = i; j < i + step; j++) {
                float a = buf[j], b = buf[j + step];
                buf[j]     = a + b;
                buf[j + step] = a - b;
            }
        }
    }
    for (int i = 0; i < TQ_BLOCK_SIZE; i++) buf[i] *= TQ_INV_SQRT32 * TQ3_0_SIGNS[i];
}

/* Nearest centroid for TQ3 (8 centroids) */
static int tq3_0_choose_index(float val) {
    /* Binary search on midpoints of TQ3_0_CENTROIDS */
    if (val < -1.644041f) return 0;
    if (val < -1.015870f) return 1;
    if (val < -0.493925f) return 2;
    if (val < -0.008701f) return 3;
    if (val <  0.477664f) return 4;
    if (val <  1.001363f) return 5;
    if (val <  1.633223f) return 6;
    return 7;
}

/* Nearest centroid for TQ4 (16 centroids) */
static int tq4_0_choose_index(float val) {
    /* Binary search on midpoints of TQ4_0_CENTROIDS */
    if (val < -2.400804f) return 0;
    if (val < -1.843532f) return 1;
    if (val < -1.437139f) return 2;
    if (val < -1.099286f) return 3;
    if (val < -0.799550f) return 4;
    if (val < -0.522404f) return 5;
    if (val < -0.258222f) return 6;
    if (val <  0.000000f) return 7;
    if (val <  0.258222f) return 8;
    if (val <  0.522404f) return 9;
    if (val <  0.799550f) return 10;
    if (val <  1.099286f) return 11;
    if (val <  1.437139f) return 12;
    if (val <  1.843532f) return 13;
    if (val <  2.400804f) return 14;
    return 15;
}

/* ---------- TQ3_1S quantization ---------- */

void quantize_row_tq3_1s_ref(const float * GGML_RESTRICT x, block_tq3_1s * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int nb = k / QK_TQ3_0;

    for (int block = 0; block < nb; block++) {
        const float * src_blk = x + block * QK_TQ3_0;
        block_tq3_1s * blk = &y[block];

        /* 1. Forward RHT */
        float buf[TQ_BLOCK_SIZE];
        memcpy(buf, src_blk, TQ_BLOCK_SIZE * sizeof(float));
        tq3_0_rht_forward(buf);

        /* 2. Split into two halves, compute RMS per half */
        float rms0 = 0.0f, rms1 = 0.0f;
        for (int j = 0; j < 16; j++) rms0 += buf[j] * buf[j];
        for (int j = 16; j < 32; j++) rms1 += buf[j] * buf[j];
        rms0 = sqrtf(rms0 / 16.0f);
        rms1 = sqrtf(rms1 / 16.0f);

        /* 3. Scale search (9 points) */
        static const float scales[] = { 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.35f, 1.5f };
        float best_d0 = rms0, best_d1 = rms1;
        float best_err = 1e30f;

        for (int si = 0; si < 9; si++) {
            float d0 = rms0 * scales[si];
            float d1 = rms1 * scales[si];
            float inv0 = (d0 > 1e-10f) ? 1.0f / d0 : 0.0f;
            float inv1 = (d1 > 1e-10f) ? 1.0f / d1 : 0.0f;

            float err = 0.0f;
            for (int j = 0; j < 16; j++) {
                int idx = tq3_0_choose_index(buf[j] * inv0);
                float diff = buf[j] - TQ3_0_CENTROIDS[idx] * d0;
                err += diff * diff;
            }
            for (int j = 16; j < 32; j++) {
                int idx = tq3_0_choose_index(buf[j] * inv1);
                float diff = buf[j] - TQ3_0_CENTROIDS[idx] * d1;
                err += diff * diff;
            }
            if (err < best_err) {
                best_err = err;
                best_d0 = d0;
                best_d1 = d1;
            }
        }

        /* 4. Iterative refinement (6 iterations) */
        for (int iter = 0; iter < 6; iter++) {
            float inv0 = (best_d0 > 1e-10f) ? 1.0f / best_d0 : 0.0f;
            float inv1 = (best_d1 > 1e-10f) ? 1.0f / best_d1 : 0.0f;

            float num0 = 0.0f, den0 = 0.0f;
            float num1 = 0.0f, den1 = 0.0f;
            for (int j = 0; j < 16; j++) {
                int idx = tq3_0_choose_index(buf[j] * inv0);
                float c = TQ3_0_CENTROIDS[idx];
                num0 += buf[j] * c;
                den0 += c * c;
            }
            for (int j = 16; j < 32; j++) {
                int idx = tq3_0_choose_index(buf[j] * inv1);
                float c = TQ3_0_CENTROIDS[idx];
                num1 += buf[j] * c;
                den1 += c * c;
            }
            if (den0 > 1e-10f) best_d0 = num0 / den0;
            if (den1 > 1e-10f) best_d1 = num1 / den1;
        }

        /* 5. Final quantize + pack */
        float inv0 = (best_d0 > 1e-10f) ? 1.0f / best_d0 : 0.0f;
        float inv1 = (best_d1 > 1e-10f) ? 1.0f / best_d1 : 0.0f;

        blk->d0 = GGML_FP32_TO_FP16(best_d0);
        blk->d1 = GGML_FP32_TO_FP16(best_d1);
        memset(blk->qs, 0, QK_TQ3_0 * 3 / 8);

        /* TQ3 packing: 4 groups of 8 indices packed into 3 bytes each */
        for (int g = 0; g < 4; g++) {
            uint8_t indices[8];
            for (int i = 0; i < 8; i++) {
                int j = g * 8 + i;
                float inv = (j < 16) ? inv0 : inv1;
                indices[i] = (uint8_t)tq3_0_choose_index(buf[j] * inv);
            }
            uint8_t * qp = blk->qs + g * 3;
            qp[0] = (indices[0] & 7) | ((indices[1] & 7) << 3) | ((indices[2] & 3) << 6);
            qp[1] = ((indices[2] >> 2) & 1) | ((indices[3] & 7) << 1) | ((indices[4] & 7) << 4) | ((indices[5] & 1) << 7);
            qp[2] = ((indices[5] >> 1) & 3) | ((indices[6] & 7) << 2) | ((indices[7] & 7) << 5);
        }
    }
}

void dequantize_row_tq3_1s(const block_tq3_1s * GGML_RESTRICT x, float * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int nb = k / QK_TQ3_0;

    for (int blk_i = 0; blk_i < nb; blk_i++) {
        float d0 = GGML_FP16_TO_FP32(x[blk_i].d0);
        float d1 = GGML_FP16_TO_FP32(x[blk_i].d1);

        /* Unpack 3-bit indices */
        float buf[32];
        for (int g = 0; g < 4; g++) {
            const uint8_t * qp = x[blk_i].qs + g * 3;
            uint8_t idx[8];
            idx[0] =  qp[0]       & 7;
            idx[1] = (qp[0] >> 3) & 7;
            idx[2] = ((qp[0] >> 6) | (qp[1] << 2)) & 7;
            idx[3] = (qp[1] >> 1) & 7;
            idx[4] = (qp[1] >> 4) & 7;
            idx[5] = ((qp[1] >> 7) | (qp[2] << 1)) & 7;
            idx[6] = (qp[2] >> 2) & 7;
            idx[7] = (qp[2] >> 5) & 7;

            for (int i = 0; i < 8; i++) {
                int j = g * 8 + i;
                float d = (j < 16) ? d0 : d1;
                buf[j] = TQ3_0_CENTROIDS[idx[i]] * d;
            }
        }

        /* Inverse RHT */
        tq3_0_rht_inverse(buf);

        memcpy(y + blk_i * QK_TQ3_0, buf, QK_TQ3_0 * sizeof(float));
    }
}

size_t quantize_tq3_1s(const float * GGML_RESTRICT src, void * GGML_RESTRICT dst,
                        int64_t nrows, int64_t n_per_row, const float * imatrix) {
    GGML_UNUSED(imatrix);
    assert(n_per_row % QK_TQ3_0 == 0);

    size_t row_size = (n_per_row / QK_TQ3_0) * sizeof(block_tq3_1s);
    for (int64_t row = 0; row < nrows; row++) {
        quantize_row_tq3_1s_ref(
            src + row * n_per_row,
            (block_tq3_1s *)((char *)dst + row * row_size),
            n_per_row
        );
    }
    return nrows * row_size;
}

/* ---------- TQ4_1S quantization ---------- */

void quantize_row_tq4_1s_ref(const float * GGML_RESTRICT x, block_tq4_1s * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ4_1S == 0);
    const int nb = k / QK_TQ4_1S;

    for (int block = 0; block < nb; block++) {
        const float * src_blk = x + block * QK_TQ4_1S;
        block_tq4_1s * blk = &y[block];

        /* 1. Forward RHT */
        float buf[TQ_BLOCK_SIZE];
        memcpy(buf, src_blk, TQ_BLOCK_SIZE * sizeof(float));
        tq3_0_rht_forward(buf);

        /* 2. Split into two halves, compute RMS per half */
        float rms0 = 0.0f, rms1 = 0.0f;
        for (int j = 0; j < 16; j++) rms0 += buf[j] * buf[j];
        for (int j = 16; j < 32; j++) rms1 += buf[j] * buf[j];
        rms0 = sqrtf(rms0 / 16.0f);
        rms1 = sqrtf(rms1 / 16.0f);

        /* 3. Scale search (9 points) */
        static const float scales[] = { 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.35f, 1.5f };
        float best_d0 = rms0, best_d1 = rms1;
        float best_err = 1e30f;

        for (int si = 0; si < 9; si++) {
            float d0 = rms0 * scales[si];
            float d1 = rms1 * scales[si];
            float inv0 = (d0 > 1e-10f) ? 1.0f / d0 : 0.0f;
            float inv1 = (d1 > 1e-10f) ? 1.0f / d1 : 0.0f;

            float err = 0.0f;
            for (int j = 0; j < 16; j++) {
                int idx = tq4_0_choose_index(buf[j] * inv0);
                float diff = buf[j] - TQ4_0_CENTROIDS[idx] * d0;
                err += diff * diff;
            }
            for (int j = 16; j < 32; j++) {
                int idx = tq4_0_choose_index(buf[j] * inv1);
                float diff = buf[j] - TQ4_0_CENTROIDS[idx] * d1;
                err += diff * diff;
            }
            if (err < best_err) {
                best_err = err;
                best_d0 = d0;
                best_d1 = d1;
            }
        }

        /* 4. Iterative refinement (6 iterations) */
        for (int iter = 0; iter < 6; iter++) {
            float inv0 = (best_d0 > 1e-10f) ? 1.0f / best_d0 : 0.0f;
            float inv1 = (best_d1 > 1e-10f) ? 1.0f / best_d1 : 0.0f;

            float num0 = 0.0f, den0 = 0.0f;
            float num1 = 0.0f, den1 = 0.0f;
            for (int j = 0; j < 16; j++) {
                int idx = tq4_0_choose_index(buf[j] * inv0);
                float c = TQ4_0_CENTROIDS[idx];
                num0 += buf[j] * c;
                den0 += c * c;
            }
            for (int j = 16; j < 32; j++) {
                int idx = tq4_0_choose_index(buf[j] * inv1);
                float c = TQ4_0_CENTROIDS[idx];
                num1 += buf[j] * c;
                den1 += c * c;
            }
            if (den0 > 1e-10f) best_d0 = num0 / den0;
            if (den1 > 1e-10f) best_d1 = num1 / den1;
        }

        /* 5. Final quantize + pack (nibble packing) */
        float inv0 = (best_d0 > 1e-10f) ? 1.0f / best_d0 : 0.0f;
        float inv1 = (best_d1 > 1e-10f) ? 1.0f / best_d1 : 0.0f;

        blk->d0 = GGML_FP32_TO_FP16(best_d0);
        blk->d1 = GGML_FP32_TO_FP16(best_d1);
        memset(blk->qs, 0, QK_TQ4_1S / 2);

        for (int j = 0; j < QK_TQ4_1S; j++) {
            float inv = (j < 16) ? inv0 : inv1;
            int idx = tq4_0_choose_index(buf[j] * inv);
            blk->qs[j / 2] |= (uint8_t)((idx & 0xF) << ((j & 1) * 4));
        }
    }
}

void dequantize_row_tq4_1s(const block_tq4_1s * GGML_RESTRICT x, float * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ4_1S == 0);
    const int nb = k / QK_TQ4_1S;

    for (int blk_i = 0; blk_i < nb; blk_i++) {
        float d0 = GGML_FP16_TO_FP32(x[blk_i].d0);
        float d1 = GGML_FP16_TO_FP32(x[blk_i].d1);

        float buf[32];
        for (int j = 0; j < 32; j++) {
            uint8_t idx = (x[blk_i].qs[j / 2] >> ((j & 1) * 4)) & 0xF;
            float d = (j < 16) ? d0 : d1;
            buf[j] = TQ4_0_CENTROIDS[idx] * d;
        }

        /* Inverse RHT */
        tq3_0_rht_inverse(buf);

        memcpy(y + blk_i * QK_TQ4_1S, buf, QK_TQ4_1S * sizeof(float));
    }
}

size_t quantize_tq4_1s(const float * GGML_RESTRICT src, void * GGML_RESTRICT dst,
                        int64_t nrows, int64_t n_per_row, const float * imatrix) {
    GGML_UNUSED(imatrix);
    assert(n_per_row % QK_TQ4_1S == 0);

    size_t row_size = (n_per_row / QK_TQ4_1S) * sizeof(block_tq4_1s);
    for (int64_t row = 0; row < nrows; row++) {
        quantize_row_tq4_1s_ref(
            src + row * n_per_row,
            (block_tq4_1s *)((char *)dst + row * row_size),
            n_per_row
        );
    }
    return nrows * row_size;
}


/* ================================================================== */
/* tq3 family: TQ3_4S / TQ3_0 / TQ3_4SE / TQ3_1S_SHIFT (ids 48..51)   */
/* ================================================================== */
/*
 * Taken-from: turbo-tan/llama.cpp-tq3@58ad80ffb (ggml/src/ggml-quants.c).
 *
 * Same lineage as the TQ3_1S/TQ4_1S half above: identical 32-value block,
 * identical Lloyd-Max centroid table, identical randomized Hadamard rotation.
 * Only the SCALE representation differs per type. That is why this lands in
 * this file rather than a new carve-out - the rotation and the centroids are
 * already here, and a duplicated table drifts.
 *
 * The ids are OURS, not theirs: tq3 serializes TQ3_4S at 46, which is our
 * TQ4_1S. See docs/TYPE-ID-ALLOCATION.md §3.1.1 - tq3-authored GGUFs must be
 * retagged (tools/gguf-retag-tq3) before they load here.
 */

/* Unpack one group of 8 x 3-bit indices from 3 bytes. The packing is shared by
 * every type in both families, so it is written once. */
static inline void tq3_unpack8(const uint8_t * qp, uint8_t * idx) {
    idx[0] =  qp[0]       & 7;
    idx[1] = (qp[0] >> 3) & 7;
    idx[2] = ((qp[0] >> 6) | (qp[1] << 2)) & 7;
    idx[3] = (qp[1] >> 1) & 7;
    idx[4] = (qp[1] >> 4) & 7;
    idx[5] = ((qp[1] >> 7) | (qp[2] << 1)) & 7;
    idx[6] = (qp[2] >> 2) & 7;
    idx[7] = (qp[2] >> 5) & 7;
}

static inline void tq3_pack8(const uint8_t * idx, uint8_t * qp) {
    qp[0] = (uint8_t) ( (idx[0])      | (idx[1] << 3) | (idx[2] << 6));
    qp[1] = (uint8_t) ( (idx[2] >> 2) | (idx[3] << 1) | (idx[4] << 4) | (idx[5] << 7));
    qp[2] = (uint8_t) ( (idx[5] >> 1) | (idx[6] << 2) | (idx[7] << 5));
}

/* E3M5 mini-float scale: 3-bit exponent (bias 9) + 5-bit mantissa.
 * The byte==0 case is NOT representable by the formula and means "zero scale";
 * dropping it is exactly the silently-wrong-numbers failure §3.1.1 warns about. */
float arifi_tq3_4s_decode_scale(uint8_t byte) {
    if (byte == 0) return 0.0f;
    const int   exp      = (byte >> 5) - 9;
    const float mantissa = 1.0f + (float) (byte & 31) / 32.0f;
    return ldexpf(mantissa, exp);
}

static uint8_t tq3_4s_encode_scale(float val) {
    if (val <= 0.0f) return 0;
    const int exp_raw = (int) floorf(log2f(val));
    int exp = exp_raw + 9;
    if (exp < 0) exp = 0;
    if (exp > 7) exp = 7;
    const float mantissa = val / ldexpf(1.0f, exp - 9) - 1.0f;
    int m = (int) roundf(mantissa * 32.0f);
    if (m < 0)  m = 0;
    if (m > 31) m = 31;
    return (uint8_t) ((exp << 5) | m);
}

/* Per-group-of-8 scale fit in the rotated domain: RMS seed, then 4 iterations of
 * least-squares re-fit against the chosen centroids. Shared by TQ3_4S and TQ3_4SE. */
static float tq3_fit_group_scale(const float * rot8, uint8_t * idx8) {
    float sum_sq = 0.0f;
    for (int j = 0; j < 8; ++j) sum_sq += rot8[j] * rot8[j];
    float scale = fmaxf(sqrtf(sum_sq / 8.0f), 1e-10f);

    for (int iter = 0; iter < 4; ++iter) {
        const float inv = 1.0f / fmaxf(scale, 1e-10f);
        float numer = 0.0f, denom = 0.0f;
        for (int j = 0; j < 8; ++j) {
            const int i8 = tq3_0_choose_index(rot8[j] * inv);
            idx8[j] = (uint8_t) i8;
            const float c = TQ3_0_CENTROIDS[i8];
            numer += rot8[j] * c;
            denom += c * c;
        }
        if (denom > 1e-12f) scale = fmaxf(numer / denom, 1e-10f);
    }
    return scale;
}

/* ---------- TQ3_4S (id 48) ---------- */

void quantize_row_tq3_4s_ref(const float * GGML_RESTRICT x, block_tq3_4s * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int64_t nb = k / QK_TQ3_0;

    for (int64_t i = 0; i < nb; ++i) {
        float buf[TQ_BLOCK_SIZE];
        memcpy(buf, x + i * QK_TQ3_0, TQ_BLOCK_SIZE * sizeof(float));
        tq3_0_rht_forward(buf);

        for (int g = 0; g < 4; ++g) {
            uint8_t idx[8];
            const float scale = tq3_fit_group_scale(buf + g * 8, idx);
            y[i].d[g] = tq3_4s_encode_scale(scale);
            tq3_pack8(idx, y[i].qs + g * 3);
        }
    }
}

void dequantize_row_tq3_4s(const block_tq3_4s * GGML_RESTRICT x, float * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int64_t nb = k / QK_TQ3_0;

    for (int64_t i = 0; i < nb; ++i) {
        float buf[TQ_BLOCK_SIZE];
        for (int g = 0; g < 4; ++g) {
            const float d = arifi_tq3_4s_decode_scale(x[i].d[g]);
            uint8_t idx[8];
            tq3_unpack8(x[i].qs + g * 3, idx);
            for (int j = 0; j < 8; ++j) buf[g*8 + j] = TQ3_0_CENTROIDS[idx[j]] * d;
        }
        tq3_0_rht_inverse(buf);
        memcpy(y + i * QK_TQ3_0, buf, QK_TQ3_0 * sizeof(float));
    }
}

size_t quantize_tq3_4s(const float * GGML_RESTRICT src, void * GGML_RESTRICT dst,
                       int64_t nrows, int64_t n_per_row, const float * imatrix) {
    GGML_UNUSED(imatrix);
    assert(n_per_row % QK_TQ3_0 == 0);
    const size_t row_size = (n_per_row / QK_TQ3_0) * sizeof(block_tq3_4s);
    for (int64_t row = 0; row < nrows; ++row) {
        quantize_row_tq3_4s_ref(src + row * n_per_row,
                                (block_tq3_4s *) ((char *) dst + row * row_size), n_per_row);
    }
    return nrows * row_size;
}

/* ---------- TQ3_0 (id 49) ---------- */
/* The one type in the family whose scale is applied OUTSIDE the rotation: the
 * block is normalized by its RMS before the forward transform, so dequantize
 * rotates the bare centroids back and scales afterwards. */

void quantize_row_tq3_0_ref(const float * GGML_RESTRICT x, block_tq3_0 * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int64_t nb = k / QK_TQ3_0;

    for (int64_t i = 0; i < nb; ++i) {
        const float * bx = x + i * QK_TQ3_0;

        float sum_sq = 0.0f;
        for (int j = 0; j < QK_TQ3_0; ++j) sum_sq += bx[j] * bx[j];
        float rms = sqrtf(sum_sq / QK_TQ3_0);
        if (rms < 1e-10f) rms = 1.0f;

        y[i].d = GGML_FP32_TO_FP16(rms);

        float buf[TQ_BLOCK_SIZE];
        const float inv_rms = 1.0f / rms;
        for (int j = 0; j < QK_TQ3_0; ++j) buf[j] = bx[j] * inv_rms;
        tq3_0_rht_forward(buf);

        for (int g = 0; g < 4; ++g) {
            uint8_t idx[8];
            for (int j = 0; j < 8; ++j) idx[j] = (uint8_t) tq3_0_choose_index(buf[g*8 + j]);
            tq3_pack8(idx, y[i].qs + g * 3);
        }
    }
}

void dequantize_row_tq3_0(const block_tq3_0 * GGML_RESTRICT x, float * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int64_t nb = k / QK_TQ3_0;

    for (int64_t i = 0; i < nb; ++i) {
        const float d = GGML_FP16_TO_FP32(x[i].d);

        float buf[TQ_BLOCK_SIZE];
        for (int g = 0; g < 4; ++g) {
            uint8_t idx[8];
            tq3_unpack8(x[i].qs + g * 3, idx);
            for (int j = 0; j < 8; ++j) buf[g*8 + j] = TQ3_0_CENTROIDS[idx[j]];
        }
        tq3_0_rht_inverse(buf);
        for (int j = 0; j < QK_TQ3_0; ++j) y[i * QK_TQ3_0 + j] = buf[j] * d;
    }
}

size_t quantize_tq3_0(const float * GGML_RESTRICT src, void * GGML_RESTRICT dst,
                      int64_t nrows, int64_t n_per_row, const float * imatrix) {
    GGML_UNUSED(imatrix);
    assert(n_per_row % QK_TQ3_0 == 0);
    const size_t row_size = (n_per_row / QK_TQ3_0) * sizeof(block_tq3_0);
    for (int64_t row = 0; row < nrows; ++row) {
        quantize_row_tq3_0_ref(src + row * n_per_row,
                               (block_tq3_0 *) ((char *) dst + row * row_size), n_per_row);
    }
    return nrows * row_size;
}

/* ---------- TQ3_4SE (id 50) ---------- */
/* TQ3_4S plus one shift per half of 16, encoded signed-u8 against a quantum of
 * max(group scale)/8. Note the asymmetry the source carries and this preserves:
 * the shift is FITTED over halves of 16 but APPLIED with h = g/2, i.e. per group
 * pair - which is the same partition, and the round-trip test pins it. */

void quantize_row_tq3_4se_ref(const float * GGML_RESTRICT x, block_tq3_4se * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int64_t nb = k / QK_TQ3_0;

    for (int64_t i = 0; i < nb; ++i) {
        float buf[TQ_BLOCK_SIZE];
        memcpy(buf, x + i * QK_TQ3_0, TQ_BLOCK_SIZE * sizeof(float));
        tq3_0_rht_forward(buf);

        uint8_t all_idx[QK_TQ3_0];
        float   group_scale[4];
        for (int g = 0; g < 4; ++g) {
            group_scale[g] = tq3_fit_group_scale(buf + g * 8, all_idx + g * 8);
            y[i].d[g] = tq3_4s_encode_scale(group_scale[g]);
        }

        const float max_s   = fmaxf(fmaxf(group_scale[0], group_scale[1]),
                                    fmaxf(group_scale[2], group_scale[3]));
        const float quantum = max_s / 8.0f;

        for (int h = 0; h < 2; ++h) {
            float shift_sum = 0.0f;
            for (int j = 0; j < 16; ++j) {
                const int e = h * 16 + j;
                shift_sum += buf[e] - TQ3_0_CENTROIDS[all_idx[e]] * group_scale[e / 8];
            }
            const float shift = shift_sum / 16.0f;
            int sq = (int) roundf(shift / fmaxf(quantum, 1e-10f) * 127.0f) + 128;
            y[i].s[h] = (uint8_t) (sq < 0 ? 0 : (sq > 255 ? 255 : sq));
        }

        for (int g = 0; g < 4; ++g) tq3_pack8(all_idx + g * 8, y[i].qs + g * 3);
    }
}

void dequantize_row_tq3_4se(const block_tq3_4se * GGML_RESTRICT x, float * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int64_t nb = k / QK_TQ3_0;

    for (int64_t i = 0; i < nb; ++i) {
        float ds[4];
        float max_s = 0.0f;
        for (int g = 0; g < 4; ++g) {
            ds[g] = arifi_tq3_4s_decode_scale(x[i].d[g]);
            if (ds[g] > max_s) max_s = ds[g];
        }
        const float quantum = max_s / 8.0f;
        float shifts[2];
        for (int h = 0; h < 2; ++h) shifts[h] = ((int) x[i].s[h] - 128) / 127.0f * quantum;

        float buf[TQ_BLOCK_SIZE];
        for (int g = 0; g < 4; ++g) {
            uint8_t idx[8];
            tq3_unpack8(x[i].qs + g * 3, idx);
            const float sh = shifts[g / 2];
            for (int j = 0; j < 8; ++j) buf[g*8 + j] = TQ3_0_CENTROIDS[idx[j]] * ds[g] + sh;
        }
        tq3_0_rht_inverse(buf);
        memcpy(y + i * QK_TQ3_0, buf, QK_TQ3_0 * sizeof(float));
    }
}

size_t quantize_tq3_4se(const float * GGML_RESTRICT src, void * GGML_RESTRICT dst,
                        int64_t nrows, int64_t n_per_row, const float * imatrix) {
    GGML_UNUSED(imatrix);
    assert(n_per_row % QK_TQ3_0 == 0);
    const size_t row_size = (n_per_row / QK_TQ3_0) * sizeof(block_tq3_4se);
    for (int64_t row = 0; row < nrows; ++row) {
        quantize_row_tq3_4se_ref(src + row * n_per_row,
                                 (block_tq3_4se *) ((char *) dst + row * row_size), n_per_row);
    }
    return nrows * row_size;
}

/* ---------- TQ3_1S_SHIFT (id 51) ---------- */
/* TQ3_1S plus a shared mean in the rotated domain, carried as a third f16. */

static float tq3_shift_fit_half(const float * src, float mean, float init_scale, uint8_t * dst_idx) {
    static const float search[9] = { 0.60f, 0.70f, 0.80f, 0.90f, 1.00f, 1.10f, 1.20f, 1.35f, 1.50f };

    float   best_scale = init_scale;
    float   best_err   = INFINITY;
    uint8_t best_idx[16] = {0};

    for (int mi = 0; mi < 9; ++mi) {
        const float scale = fmaxf(init_scale * search[mi], 1e-10f);
        const float inv   = 1.0f / scale;
        uint8_t tmp_idx[16];
        float err = 0.0f;
        for (int j = 0; j < 16; ++j) {
            const int i8 = tq3_0_choose_index((src[j] - mean) * inv);
            tmp_idx[j] = (uint8_t) i8;
            const float d = src[j] - (TQ3_0_CENTROIDS[i8] * scale + mean);
            err += d * d;
        }
        if (err < best_err) {
            best_err   = err;
            best_scale = scale;
            memcpy(best_idx, tmp_idx, sizeof(best_idx));
        }
    }

    /* Least-squares refinement, keeping the best iterate rather than the last. */
    float   scale = best_scale;
    float   ref_scale = best_scale;
    float   ref_err   = best_err;
    uint8_t ref_idx[16];
    uint8_t trial_idx[16] = {0};
    memcpy(ref_idx, best_idx, sizeof(ref_idx));

    for (int iter = 0; iter < 6; ++iter) {
        const float inv = 1.0f / fmaxf(scale, 1e-10f);
        float numer = 0.0f, denom = 0.0f;
        for (int j = 0; j < 16; ++j) {
            const int i8 = tq3_0_choose_index((src[j] - mean) * inv);
            trial_idx[j] = (uint8_t) i8;
            const float c = TQ3_0_CENTROIDS[i8];
            numer += (src[j] - mean) * c;
            denom += c * c;
        }
        if (denom > 1e-12f) scale = fmaxf(numer / denom, 1e-10f);

        float err = 0.0f;
        for (int j = 0; j < 16; ++j) {
            const float d = src[j] - (TQ3_0_CENTROIDS[trial_idx[j]] * scale + mean);
            err += d * d;
        }
        if (err < ref_err) {
            ref_err   = err;
            ref_scale = scale;
            memcpy(ref_idx, trial_idx, sizeof(ref_idx));
        }
    }

    memcpy(dst_idx, ref_idx, 16);
    return ref_scale;
}

void quantize_row_tq3_1s_shift_ref(const float * GGML_RESTRICT x, block_tq3_1s_shift * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int64_t nb = k / QK_TQ3_0;

    for (int64_t i = 0; i < nb; ++i) {
        float buf[TQ_BLOCK_SIZE];
        memcpy(buf, x + i * QK_TQ3_0, TQ_BLOCK_SIZE * sizeof(float));
        tq3_0_rht_forward(buf);

        float mean = 0.0f;
        for (int j = 0; j < QK_TQ3_0; ++j) mean += buf[j];
        mean /= (float) QK_TQ3_0;

        float sum0 = 0.0f, sum1 = 0.0f;
        for (int j = 0; j < 16; ++j) {
            const float c0 = buf[j]      - mean;
            const float c1 = buf[16 + j] - mean;
            sum0 += c0 * c0;
            sum1 += c1 * c1;
        }
        float rms0 = sqrtf(sum0 / 16.0f);
        float rms1 = sqrtf(sum1 / 16.0f);
        if (rms0 < 1e-10f) rms0 = 1.0f;
        if (rms1 < 1e-10f) rms1 = 1.0f;

        uint8_t indices[QK_TQ3_0];
        const float scale0 = tq3_shift_fit_half(buf,      mean, rms0, indices);
        const float scale1 = tq3_shift_fit_half(buf + 16, mean, rms1, indices + 16);

        y[i].d0 = GGML_FP32_TO_FP16(scale0);
        y[i].d1 = GGML_FP32_TO_FP16(scale1);
        y[i].m  = GGML_FP32_TO_FP16(mean);

        for (int g = 0; g < 4; ++g) tq3_pack8(indices + g * 8, y[i].qs + g * 3);
    }
}

void dequantize_row_tq3_1s_shift(const block_tq3_1s_shift * GGML_RESTRICT x, float * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TQ3_0 == 0);
    const int64_t nb = k / QK_TQ3_0;

    for (int64_t i = 0; i < nb; ++i) {
        const float d0 = GGML_FP16_TO_FP32(x[i].d0);
        const float d1 = GGML_FP16_TO_FP32(x[i].d1);
        const float m  = GGML_FP16_TO_FP32(x[i].m);

        float buf[TQ_BLOCK_SIZE];
        for (int g = 0; g < 4; ++g) {
            uint8_t idx[8];
            tq3_unpack8(x[i].qs + g * 3, idx);
            for (int j = 0; j < 8; ++j) {
                const int e = g * 8 + j;
                buf[e] = TQ3_0_CENTROIDS[idx[j]] * ((e < 16) ? d0 : d1) + m;
            }
        }
        tq3_0_rht_inverse(buf);
        memcpy(y + i * QK_TQ3_0, buf, QK_TQ3_0 * sizeof(float));
    }
}

size_t quantize_tq3_1s_shift(const float * GGML_RESTRICT src, void * GGML_RESTRICT dst,
                             int64_t nrows, int64_t n_per_row, const float * imatrix) {
    GGML_UNUSED(imatrix);
    assert(n_per_row % QK_TQ3_0 == 0);
    const size_t row_size = (n_per_row / QK_TQ3_0) * sizeof(block_tq3_1s_shift);
    for (int64_t row = 0; row < nrows; ++row) {
        quantize_row_tq3_1s_shift_ref(src + row * n_per_row,
                                      (block_tq3_1s_shift *) ((char *) dst + row * row_size), n_per_row);
    }
    return nrows * row_size;
}

/* ---------------------------------------------------------------------------
 * Accessors so the CPU dot product can reach the constant tables without
 * duplicating them. Duplicated tables drift; a shared one cannot.
 * --------------------------------------------------------------------------- */
const float * arifi_tq_signs(void)      { return TQ3_0_SIGNS;     }
const float * arifi_tq3_centroids(void) { return TQ3_0_CENTROIDS; }
const float * arifi_tq4_centroids(void) { return TQ4_0_CENTROIDS; }
