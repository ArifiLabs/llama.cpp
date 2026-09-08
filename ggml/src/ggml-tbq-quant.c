// TBQ3_0 / TBQ4_0: 128-value blocks, signed Walsh-Hadamard rotation, Lloyd-Max codebook, one f16 norm.
// Reference codec, arch-neutral C. Dequantize returns the ORIGINAL domain, so every generic path
// (to_float, CPY, GET_ROWS, SET_ROWS via from_float, dequant-then-dot) works without a new op.
//
// Taken-from: jtrefon/llama.cpp-turboq-mtp@6a02d0494 (ggml/src/ggml-turboq.c, the TBQ sections).
// Type ids are OURS (58/59, docs/TYPE-ID-ALLOCATION.md 3.1.3): their 42/43 are upstream Q2_0 and our Q2_0_G128.

#define GGML_COMMON_IMPL_C
#include "ggml-common.h"

#include "ggml-tbq-tables.h"
#include "ggml-quants.h"
#include "ggml-impl.h"

#include <math.h>
#include <string.h>
#include <assert.h>

// unnormalized in-place butterfly over 128 values
static void tbq_fwht_128(float * x) {
    for (int h = 1; h < 128; h *= 2) {
        for (int i = 0; i < 128; i += h * 2) {
            for (int j = i; j < i + h; j++) {
                const float a = x[j];
                const float b = x[j + h];
                x[j]     = a + b;
                x[j + h] = a - b;
            }
        }
    }
}

static inline uint8_t tbq_nearest(float val, const float * midpoints, int n) {
    for (int i = 0; i < n; i++) {
        if (val < midpoints[i]) {
            return (uint8_t) i;
        }
    }
    return (uint8_t) n;
}

// ---------------------------------------------------------------------------------------------
// TBQ4_0: normalized rotation (1/sqrt(128) folded into the butterfly), 4-bit nibbles
// ---------------------------------------------------------------------------------------------

#define TBQ_INV_SQRT_128 0.08838834764831845f

static void tbq4_rotate_forward(float * x) {
    for (int i = 0; i < 128; i++) x[i] *= tbq_wht_signs1[i];
    tbq_fwht_128(x);
    for (int i = 0; i < 128; i++) x[i] *= tbq_wht_signs2[i] * TBQ_INV_SQRT_128;
}

static void tbq4_rotate_inverse(float * x) {
    for (int i = 0; i < 128; i++) x[i] *= tbq_wht_signs2[i];
    tbq_fwht_128(x);
    for (int i = 0; i < 128; i++) x[i] *= tbq_wht_signs1[i] * TBQ_INV_SQRT_128;
}

void quantize_row_tbq4_0_ref(const float * GGML_RESTRICT x, block_tbq4_0 * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TBQ4 == 0);
    const int64_t nb = k / QK_TBQ4;

    for (int64_t b = 0; b < nb; b++) {
        const float * xb = x + b * QK_TBQ4;

        float norm_sq = 0.0f;
        for (int j = 0; j < QK_TBQ4; j++) norm_sq += xb[j] * xb[j];
        const float norm     = sqrtf(norm_sq);
        const float inv_norm = norm > 1e-10f ? 1.0f / norm : 0.0f;

        float rot[QK_TBQ4];
        for (int j = 0; j < QK_TBQ4; j++) rot[j] = xb[j] * inv_norm;
        tbq4_rotate_forward(rot);

        float recon_sq = 0.0f;
        for (int j = 0; j < QK_TBQ4; j += 2) {
            const uint8_t i0 = tbq_nearest(rot[j],     tbq_fwht_midpoints_4bit, 15);
            const uint8_t i1 = tbq_nearest(rot[j + 1], tbq_fwht_midpoints_4bit, 15);
            y[b].qs[j / 2] = (uint8_t) ((i1 << 4) | i0);
            recon_sq += tbq_fwht_centroids_4bit[i0] * tbq_fwht_centroids_4bit[i0]
                      + tbq_fwht_centroids_4bit[i1] * tbq_fwht_centroids_4bit[i1];
        }

        const float recon_norm = sqrtf(recon_sq);
        y[b].d = GGML_FP32_TO_FP16(recon_norm > 1e-10f ? norm / recon_norm : norm);
    }
}

void dequantize_row_tbq4_0(const block_tbq4_0 * GGML_RESTRICT x, float * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TBQ4 == 0);
    const int64_t nb = k / QK_TBQ4;

    for (int64_t b = 0; b < nb; b++) {
        const float d = GGML_FP16_TO_FP32(x[b].d);

        float rot[QK_TBQ4];
        for (int j = 0; j < QK_TBQ4; j += 2) {
            const uint8_t q = x[b].qs[j / 2];
            rot[j]     = tbq_fwht_centroids_4bit[q & 0xF];
            rot[j + 1] = tbq_fwht_centroids_4bit[q >> 4];
        }
        tbq4_rotate_inverse(rot);

        for (int j = 0; j < QK_TBQ4; j++) y[b * QK_TBQ4 + j] = rot[j] * d;
    }
}

size_t quantize_tbq4_0(const float * GGML_RESTRICT src, void * GGML_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix) {
    GGML_UNUSED(imatrix);
    assert(n_per_row % QK_TBQ4 == 0);
    const size_t row_size = (n_per_row / QK_TBQ4) * sizeof(block_tbq4_0);
    for (int64_t row = 0; row < nrows; row++) {
        quantize_row_tbq4_0_ref(src + row * n_per_row, (block_tbq4_0 *) ((char *) dst + row * row_size), n_per_row);
    }
    return nrows * row_size;
}

// ---------------------------------------------------------------------------------------------
// TBQ3_0: unnormalized rotation, 3-bit indices in 24-bit little-endian lanes (8 values per 3 bytes)
// ---------------------------------------------------------------------------------------------

void quantize_row_tbq3_0_ref(const float * GGML_RESTRICT x, block_tbq3_0 * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TBQ3 == 0);
    const int64_t nb = k / QK_TBQ3;

    for (int64_t b = 0; b < nb; b++) {
        const float * xb = x + b * QK_TBQ3;

        float norm_sq = 0.0f;
        for (int j = 0; j < QK_TBQ3; j++) norm_sq += xb[j] * xb[j];
        float norm = sqrtf(norm_sq);
        if (norm < 1e-10f) norm = 1e-10f;

        float rot[QK_TBQ3];
        for (int j = 0; j < QK_TBQ3; j++) rot[j] = xb[j] / norm * tbq_wht_signs1_tbq3[j];
        tbq_fwht_128(rot);
        for (int j = 0; j < QK_TBQ3; j++) rot[j] *= tbq_wht_signs2_tbq3[j];

        uint8_t idx[QK_TBQ3];
        float recon_sq = 0.0f;
        for (int j = 0; j < QK_TBQ3; j++) {
            idx[j] = tbq_nearest(rot[j], tbq_fwht_midpoints_3bit, 7);
            recon_sq += tbq_fwht_centroids_3bit[idx[j]] * tbq_fwht_centroids_3bit[idx[j]];
        }

        for (int g = 0; g < QK_TBQ3 / 8; g++) {
            uint32_t packed = 0;
            for (int j = 0; j < 8; j++) {
                packed |= (uint32_t) idx[g * 8 + j] << (3 * j);
            }
            y[b].qs[g * 3 + 0] = (uint8_t) (packed & 0xFF);
            y[b].qs[g * 3 + 1] = (uint8_t) ((packed >> 8) & 0xFF);
            y[b].qs[g * 3 + 2] = (uint8_t) ((packed >> 16) & 0xFF);
        }

        float recon_norm = sqrtf(recon_sq);
        if (recon_norm < 1e-10f) recon_norm = 1e-10f;
        y[b].d = GGML_FP32_TO_FP16(norm / recon_norm);
    }
}

void dequantize_row_tbq3_0(const block_tbq3_0 * GGML_RESTRICT x, float * GGML_RESTRICT y, int64_t k) {
    assert(k % QK_TBQ3 == 0);
    const int64_t nb = k / QK_TBQ3;

    for (int64_t b = 0; b < nb; b++) {
        const float d = GGML_FP16_TO_FP32(x[b].d);

        float rot[QK_TBQ3];
        for (int g = 0; g < QK_TBQ3 / 8; g++) {
            const uint32_t packed = (uint32_t) x[b].qs[g * 3 + 0]
                                  | ((uint32_t) x[b].qs[g * 3 + 1] << 8)
                                  | ((uint32_t) x[b].qs[g * 3 + 2] << 16);
            for (int j = 0; j < 8; j++) {
                rot[g * 8 + j] = tbq_fwht_centroids_3bit[(packed >> (3 * j)) & 0x7];
            }
        }

        for (int j = 0; j < QK_TBQ3; j++) rot[j] *= tbq_wht_signs2_tbq3[j];
        tbq_fwht_128(rot);
        for (int j = 0; j < QK_TBQ3; j++) rot[j] *= tbq_wht_signs1_tbq3[j];

        // the butterfly is unnormalized in both directions; one 1/sqrt(128) restores the norm
        for (int j = 0; j < QK_TBQ3; j++) y[b * QK_TBQ3 + j] = rot[j] * TBQ_INV_SQRT_128 * d;
    }
}

size_t quantize_tbq3_0(const float * GGML_RESTRICT src, void * GGML_RESTRICT dst, int64_t nrows, int64_t n_per_row, const float * imatrix) {
    GGML_UNUSED(imatrix);
    assert(n_per_row % QK_TBQ3 == 0);
    const size_t row_size = (n_per_row / QK_TBQ3) * sizeof(block_tbq3_0);
    for (int64_t row = 0; row < nrows; row++) {
        quantize_row_tbq3_0_ref(src + row * n_per_row, (block_tbq3_0 *) ((char *) dst + row * row_size), n_per_row);
    }
    return nrows * row_size;
}
