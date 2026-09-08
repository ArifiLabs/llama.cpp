#pragma once

// TBQ3_0 / TBQ4_0 codebooks and Walsh-Hadamard sign masks.
// Taken-from: jtrefon/llama.cpp-turboq-mtp@6a02d0494 (ggml/src/ggml-turboq-tables.h), verbatim values.
// The sign masks are the seed-42 arrays shared with dflash's turbo-wht.cu; a file written by that
// fork dequantizes bit-for-bit here only while these tables stay byte-identical. Do not regenerate.

// Lloyd-Max centroids for N(0, 1/sqrt(128)) in the FWHT domain, 16 levels (TBQ4_0)
static const float tbq_fwht_centroids_4bit[16] = {
    -0.241556f, -0.182907f, -0.143047f, -0.111065f,
    -0.083317f, -0.058069f, -0.034311f, -0.011353f,
     0.011353f,  0.034311f,  0.058069f,  0.083317f,
     0.111065f,  0.143047f,  0.182907f,  0.241556f,
};
static const float tbq_fwht_midpoints_4bit[15] = {
    -0.212232f, -0.162977f, -0.127056f, -0.097191f, -0.070693f,
    -0.046190f, -0.022832f,  0.000000f,  0.022832f,  0.046190f,
     0.070693f,  0.097191f,  0.127056f,  0.162977f,  0.212232f,
};

// 8 levels (TBQ3_0). These are for the UNNORMALIZED butterfly (no 1/sqrt(128)), which is why they
// sit ~sqrt(128)/2 above the 4-bit table; dequantize_row_tbq3_0 applies the 1/sqrt(128) once.
static const float tbq_fwht_centroids_3bit[8] = {
    -1.646828f, -0.895384f, -0.491349f, -0.157976f,
     0.157976f,  0.491349f,  0.895384f,  1.646828f,
};
static const float tbq_fwht_midpoints_3bit[7] = {
    -1.271106f, -0.693367f, -0.324663f, 0.0f,
     0.324663f,  0.693367f,  1.271106f,
};

// FWHT sign arrays (seed=42) for the 128-element rotation - TBQ4_0.
static const float tbq_wht_signs1[128] = {
    -1, 1, 1,-1,-1, 1,-1, 1,-1,-1, 1, 1, 1, 1, 1, 1, 1,-1, 1,-1, 1,-1,-1, 1, 1, 1,-1, 1, 1,-1,-1,-1,
    -1, 1, 1,-1, 1, 1,-1, 1,-1, 1, 1,-1,-1, 1,-1, 1, 1, 1, 1,-1,-1,-1,-1,-1, 1,-1, 1, 1, 1, 1,-1, 1,
    -1,-1, 1,-1,-1,-1, 1,-1,-1,-1, 1,-1,-1,-1, 1, 1, 1,-1,-1, 1, 1, 1,-1,-1, 1, 1,-1, 1, 1,-1, 1,-1,
    -1, 1, 1,-1, 1,-1, 1,-1, 1, 1, 1, 1,-1, 1,-1, 1, 1,-1, 1, 1,-1,-1,-1,-1,-1, 1, 1,-1, 1, 1,-1, 1};
static const float tbq_wht_signs2[128] = {
     1, 1, 1, 1,-1, 1, 1,-1, 1,-1,-1,-1, 1,-1,-1,-1, 1, 1,-1,-1, 1,-1, 1,-1, 1,-1,-1, 1,-1, 1, 1, 1,
     1, 1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1, 1, 1, 1,-1, 1,-1, 1, 1, 1,-1,-1, 1,-1,-1,-1,-1,-1,-1, 1, 1,
     1,-1, 1,-1,-1,-1,-1, 1,-1, 1,-1, 1,-1,-1, 1, 1,-1, 1,-1, 1, 1,-1, 1,-1,-1,-1,-1, 1,-1,-1, 1,-1,
     1,-1, 1, 1, 1,-1,-1, 1,-1, 1,-1, 1, 1,-1,-1, 1,-1, 1,-1, 1, 1,-1, 1,-1, 1,-1,-1,-1,-1,-1, 1,-1};

// FWHT sign arrays (seed=42) - TBQ3_0 specific; they differ from the TBQ4 arrays above.
static const float tbq_wht_signs1_tbq3[128] = {
    -1, 1, 1, 1,-1, 1,-1, 1,-1,-1, 1, 1, 1, 1,-1,-1,
     1,-1,-1, 1, 1,-1,-1, 1,-1,-1, 1, 1, 1,-1, 1,-1,
     1,-1, 1, 1,-1,-1, 1, 1,-1, 1,-1,-1, 1, 1,-1, 1,
     1, 1, 1,-1, 1,-1,-1, 1,-1, 1, 1, 1,-1,-1, 1,-1,
    -1, 1,-1,-1, 1,-1, 1, 1,-1, 1,-1, 1, 1,-1,-1, 1,
     1,-1,-1,-1, 1, 1,-1,-1,-1,-1,-1,-1, 1, 1,-1, 1,
    -1,-1, 1, 1,-1, 1, 1, 1,-1,-1,-1, 1, 1,-1, 1,-1,
    -1, 1,-1, 1,-1,-1,-1,-1, 1, 1, 1,-1, 1, 1, 1,-1};
static const float tbq_wht_signs2_tbq3[128] = {
     1, 1, 1,-1,-1,-1,-1,-1,-1, 1,-1, 1, 1, 1,-1,-1,
     1,-1,-1, 1, 1,-1, 1,-1,-1, 1, 1,-1,-1, 1,-1,-1,
     1, 1, 1,-1,-1,-1, 1,-1,-1, 1,-1,-1,-1,-1,-1,-1,
    -1, 1, 1, 1, 1, 1, 1,-1, 1,-1,-1,-1, 1,-1, 1, 1,
    -1, 1, 1, 1,-1,-1,-1,-1, 1,-1, 1,-1, 1, 1, 1,-1,
     1,-1,-1,-1,-1, 1, 1,-1, 1,-1, 1, 1,-1,-1, 1,-1,
    -1,-1, 1, 1, 1, 1, 1, 1, 1, 1,-1,-1,-1,-1,-1,-1,
     1,-1,-1, 1, 1, 1,-1,-1, 1,-1, 1, 1,-1,-1,-1,-1};
