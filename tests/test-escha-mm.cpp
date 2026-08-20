// ArifiLabs Escha-W2 (lane-164): CPU GGML_OP_ESCHA_MM vs Python golden fixtures.
//
// The goldens come from an independent route: the numpy port of the recovered cbA
// decoder (bit-exact vs trial-evidence/escha-decoder/tools/decode_cba.py) plus a
// float64 Hadamard/scale/bias chain. See lane-evidence make_escha_mm_fixtures.py.
//
// Usage: test-escha-mm <fixtures-dir>   (skips with exit 0 if no dir given)

#include "ggml.h"
#include "ggml-cpu.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static std::vector<uint8_t> read_file(const std::string & p) {
    FILE * f = fopen(p.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "FATAL: cannot open %s\n", p.c_str());
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> v(n);
    if (fread(v.data(), 1, n, f) != (size_t) n) {
        fprintf(stderr, "FATAL: short read %s\n", p.c_str());
        exit(1);
    }
    fclose(f);
    return v;
}

static int run_case(const std::string & dir, int K, bool plant_mismatch) {
    const int64_t n_in = 256, n_out = 128, ncols = 3;

    auto codes = read_file(dir + "/k" + std::to_string(K) + "_codes.bin");
    auto xbuf  = read_file(dir + "/k" + std::to_string(K) + "_x.bin");
    auto aux   = read_file(dir + "/k" + std::to_string(K) + "_aux.bin");
    auto ybuf  = read_file(dir + "/k" + std::to_string(K) + "_y.bin");

    if (plant_mismatch) {
        codes[100] ^= 1; // one flipped code bit must be detected (F-043 planted positive)
    }

    struct ggml_init_params ip = { 256*1024*1024, nullptr, false };
    struct ggml_context * ctx = ggml_init(ip);

    struct ggml_tensor * a = ggml_new_tensor_2d(ctx, K == 2 ? GGML_TYPE_ESCHA2 : GGML_TYPE_ESCHA3, n_in, n_out);
    struct ggml_tensor * b = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, n_in, ncols);
    struct ggml_tensor * c = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_in + 2*n_out);

    GGML_ASSERT(ggml_nbytes(a) == codes.size());
    GGML_ASSERT(ggml_nbytes(b) == xbuf.size());
    GGML_ASSERT(ggml_nbytes(c) == aux.size());

    memcpy(a->data, codes.data(), codes.size());
    memcpy(b->data, xbuf.data(),  xbuf.size());
    memcpy(c->data, aux.data(),   aux.size());

    struct ggml_tensor * y = ggml_escha_mm(ctx, a, b, c);
    struct ggml_cgraph * gf = ggml_new_graph(ctx);
    ggml_build_forward_expand(gf, y);
    ggml_graph_compute_with_ctx(ctx, gf, 4);

    const float * got = (const float *) y->data;
    const float * ref = (const float *) ybuf.data();

    double num = 0.0, den = 0.0;
    for (int64_t i = 0; i < n_out*ncols; ++i) {
        num += (got[i] - ref[i]) * (double)(got[i] - ref[i]);
        den += (double) ref[i] * ref[i];
    }
    const double nmse = num / den;
    const bool ok = nmse < 1e-9; // f32-vs-f64 chain noise only; a decode bug is >> this

    printf("K=%d%s: nmse=%.3e -> %s\n", K, plant_mismatch ? " (planted)" : "", nmse,
           plant_mismatch ? (ok ? "NOT DETECTED (BAD)" : "detected (good)") : (ok ? "PASS" : "FAIL"));

    ggml_free(ctx);
    return plant_mismatch ? (ok ? 1 : 0) : (ok ? 0 : 1);
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("test-escha-mm: no fixtures dir given, skipping\n");
        return 0;
    }
    int rc = 0;
    rc |= run_case(argv[1], 2, false);
    rc |= run_case(argv[1], 3, false);
    rc |= run_case(argv[1], 3, true);
    return rc;
}
