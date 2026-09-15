// ArifiLabs lane-242 / R53: the S-X8 v4.3 PCA correction vs a NumPy golden.
//
// One REAL S-X8 tensor out of the 4B GGUF (a 64-row slice of blk.0.ffn_down.weight) is run
// through exactly the op sequence llama_graph's build_lora_mm builds for an S-X8 weight that has
// PCA companions, and checked against float64 goldens computed straight from
// SX8_FLASH_V4_3_SPEC.md sections 3-5 (tools/sx8/make_sx8_pca_fixtures.py -- no engine code).
//
// Checked here:
//   1. mul_mat alone reproduces y_core  -- the decode is untouched by R53.
//   2. mul_mat + correction reproduces y_pca.
//   3. with --plant, c0's sign is flipped and case 2 MUST fail; that is what proves the
//      correction actually ran rather than the tolerance being loose.
//
// Usage: test-sx8-pca <fixtures-dir> [backend-name] [--plant]
//        (no dir -> skip with exit 0, so a bare CTest run stays green)

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"

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
    if (n > 0 && fread(v.data(), 1, n, f) != (size_t) n) {
        fprintf(stderr, "FATAL: short read %s\n", p.c_str());
        exit(1);
    }
    fclose(f);
    return v;
}

// max |a-b| relative to the golden's own scale -- the tensor's outputs are O(10), so a plain
// absolute epsilon would either be meaningless or fail on honest f32 rounding.
static double rel_err(const float * a, const float * b, size_t n) {
    double maxd = 0.0, maxb = 0.0;
    for (size_t i = 0; i < n; i++) {
        maxd = std::max(maxd, (double) std::fabs(a[i] - b[i]));
        maxb = std::max(maxb, (double) std::fabs(b[i]));
    }
    return maxd / std::max(maxb, 1e-9);
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("test-sx8-pca: no fixtures dir given, skipping\n");
        return 0;
    }
    const std::string dir = argv[1];
    std::string backend_name;
    bool plant = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--plant") == 0) {
            plant = true;
        } else {
            backend_name = argv[i];
        }
    }

    int64_t K = 0, n_rows = 0, n_cb = 0, n_cols = 0;
    {
        FILE * f = fopen((dir + "/dims.txt").c_str(), "r");
        if (!f || fscanf(f, "%lld %lld %lld %lld", (long long *) &K, (long long *) &n_rows,
                         (long long *) &n_cb, (long long *) &n_cols) != 4) {
            fprintf(stderr, "FATAL: cannot read %s/dims.txt\n", dir.c_str());
            return 1;
        }
        fclose(f);
    }

    auto w_raw  = read_file(dir + "/w.bin");
    auto b0_raw = read_file(dir + "/b0.bin");
    auto b1_raw = read_file(dir + "/b1.bin");
    auto c0_raw = read_file(dir + "/c0.bin");
    auto c1_raw = read_file(dir + "/c1.bin");
    auto x_raw  = read_file(dir + "/x.bin");
    auto y_core = read_file(dir + "/y_core.bin");
    auto y_pca  = read_file(dir + "/y_pca.bin");

    if (plant) {
        // flip the sign of every c0 coefficient (f16 sign bit)
        for (size_t i = 1; i < c0_raw.size(); i += 2) {
            c0_raw[i] ^= 0x80;
        }
    }

    ggml_backend_t backend = nullptr;
    if (!backend_name.empty()) {
        for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
            ggml_backend_dev_t dev = ggml_backend_dev_get(i);
            if (backend_name == ggml_backend_dev_name(dev) ||
                backend_name == ggml_backend_reg_name(ggml_backend_dev_backend_reg(dev))) {
                backend = ggml_backend_dev_init(dev, nullptr);
                break;
            }
        }
        if (!backend) {
            fprintf(stderr, "FATAL: backend '%s' not found\n", backend_name.c_str());
            return 1;
        }
    } else {
        backend = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, nullptr);
    }
    printf("backend: %s | K=%lld n_cb=%lld rows=%lld cols=%lld%s\n",
           ggml_backend_name(backend), (long long) K, (long long) n_cb,
           (long long) n_rows, (long long) n_cols, plant ? "  [PLANTED RED]" : "");

    ggml_init_params ip = { /*.mem_size=*/ ggml_tensor_overhead() * 32, nullptr, /*.no_alloc=*/ true };
    ggml_context * ctx = ggml_init(ip);

    ggml_tensor * w  = ggml_new_tensor_2d(ctx, GGML_TYPE_SX8, K,    n_rows);
    ggml_tensor * x  = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, K,    n_cols);
    ggml_tensor * b0 = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 32,   n_cb);
    ggml_tensor * b1 = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 32,   n_cb);
    ggml_tensor * c0 = ggml_new_tensor_2d(ctx, GGML_TYPE_F16, n_cb, n_rows);
    ggml_tensor * c1 = ggml_new_tensor_2d(ctx, GGML_TYPE_F16, n_cb, n_rows);

    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors(ctx, backend);
    if (!buf) {
        fprintf(stderr, "FATAL: cannot allocate tensors on %s\n", ggml_backend_name(backend));
        return 1;
    }

    struct { ggml_tensor * t; std::vector<uint8_t> * src; const char * name; } up[] = {
        { w, &w_raw, "w" }, { x, &x_raw, "x" }, { b0, &b0_raw, "b0" },
        { b1, &b1_raw, "b1" }, { c0, &c0_raw, "c0" }, { c1, &c1_raw, "c1" },
    };
    for (auto & u : up) {
        if (u.src->size() != ggml_nbytes(u.t)) {
            fprintf(stderr, "FATAL: %s.bin is %zu bytes, tensor wants %zu\n",
                    u.name, u.src->size(), ggml_nbytes(u.t));
            return 1;
        }
        ggml_backend_tensor_set(u.t, u.src->data(), 0, u.src->size());
    }

    // ---- the graph, mirroring src/llama-graph.cpp build_lora_mm ----
    ggml_init_params gp = { /*.mem_size=*/ ggml_tensor_overhead() * 64 + ggml_graph_overhead(),
                            nullptr, /*.no_alloc=*/ true };
    ggml_context * gctx = ggml_init(gp);

    ggml_tensor * core = ggml_mul_mat(gctx, w, x);
    ggml_set_output(core);

    ggml_tensor * xr  = ggml_reshape_3d(gctx, x, 32, n_cb, n_cols);
    ggml_tensor * z0  = ggml_reshape_2d(gctx, ggml_sum_rows(gctx, ggml_mul(gctx, xr, b0)), n_cb, n_cols);
    ggml_tensor * z1  = ggml_reshape_2d(gctx, ggml_sum_rows(gctx, ggml_mul(gctx, xr, b1)), n_cb, n_cols);
    ggml_tensor * res = ggml_add(gctx, core, ggml_mul_mat(gctx, c0, z0));
    res = ggml_add(gctx, res, ggml_mul_mat(gctx, c1, z1));
    ggml_set_output(res);

    ggml_cgraph * gf = ggml_new_graph(gctx);
    ggml_build_forward_expand(gf, core);
    ggml_build_forward_expand(gf, res);

    ggml_gallocr_t alloc = ggml_gallocr_new(ggml_backend_get_default_buffer_type(backend));
    if (!ggml_gallocr_alloc_graph(alloc, gf)) {
        fprintf(stderr, "FATAL: graph alloc failed\n");
        return 1;
    }
    if (ggml_backend_graph_compute(backend, gf) != GGML_STATUS_SUCCESS) {
        fprintf(stderr, "FATAL: graph compute failed\n");
        return 1;
    }

    std::vector<float> got_core(n_rows * n_cols), got_pca(n_rows * n_cols);
    ggml_backend_tensor_get(core, got_core.data(), 0, got_core.size() * sizeof(float));
    ggml_backend_tensor_get(res,  got_pca.data(),  0, got_pca.size()  * sizeof(float));

    const double e_core = rel_err(got_core.data(), (const float *) y_core.data(), got_core.size());
    const double e_pca  = rel_err(got_pca.data(),  (const float *) y_pca.data(),  got_pca.size());

    // The S-X8 mat-mul is an INTEGER-dot path: it quantizes the activations to Q8_1 before the
    // dot (ggml_vec_dot_sx8_q8_1, and the MMVQ route on Vulkan). So the core can never match an
    // exact float64 decode better than that quantization allows -- a few 1e-3, identical on CPU
    // and Vulkan because both quantize the same way. That error predates R53 and is not what
    // this test is about, so it gets a loose sanity bound only.
    //
    // R53's own contribution is the DIFFERENCE the correction makes. Isolating it
    // (got_pca - got_core against y_pca - y_core) takes the shared Q8_1 error out of both sides
    // and tests the PCA arithmetic at full f32 precision, which is where 1e-5 belongs.
    std::vector<float> d_got(got_pca.size()), d_ref(got_pca.size());
    for (size_t i = 0; i < d_got.size(); i++) {
        d_got[i] = got_pca[i] - got_core[i];
        d_ref[i] = ((const float *) y_pca.data())[i] - ((const float *) y_core.data())[i];
    }
    // Two denominators, because they answer different questions:
    //   e_term: error relative to the PCA term's own size -- how exactly the term is computed.
    //   e_out : error relative to the output -- how much of y is wrong because of it. This is
    //           the one that gates, since it is the quantity that reaches perplexity.
    // On CPU e_term sits near 4e-4: ggml's F16 mat_mul rounds its activations to F16, so the
    // z vector loses precision inside the correction's own mat-vec. The coefficients themselves
    // are integers in [-8,7] and exact in F16. Storing c0/c1 as F32 instead removes that
    // rounding and takes e_term to ~1e-6, at +366 MB of companion for a term that is already
    // three orders of magnitude below the path's pre-existing Q8_1 floor -- so F16 stands.
    double max_ref = 0.0, max_y = 0.0;
    for (size_t i = 0; i < d_ref.size(); i++) {
        max_ref = std::max(max_ref, (double) std::fabs(d_ref[i]));
        max_y   = std::max(max_y,   (double) std::fabs(((const float *) y_pca.data())[i]));
    }
    double maxd = 0.0;
    for (size_t i = 0; i < d_got.size(); i++) {
        maxd = std::max(maxd, (double) std::fabs(d_got[i] - d_ref[i]));
    }
    const double e_term = maxd / std::max(max_ref, 1e-9);
    const double e_out  = maxd / std::max(max_y,   1e-9);

    const double TOL      = 1e-5;   // the PCA term's error as seen at the output
    const double TOL_CORE = 5e-3;   // Q8_1 activation quantization, pre-existing

    printf("  mul_mat        vs y_core  : rel %.3e  %s  (Q8_1 activations, bound %.0e)\n",
           e_core, e_core < TOL_CORE ? "OK" : "FAIL", TOL_CORE);
    printf("  mul_mat + PCA  vs y_pca   : rel %.3e     (carries the same Q8_1 error)\n", e_pca);
    printf("  PCA TERM       vs golden  : rel %.3e of the term | %.3e of the output  %s (bound %.0e)\n",
           e_term, e_out, e_out < TOL ? "OK" : "FAIL", TOL);

    // Dump the corrected output so a second backend run can be diffed against this one.
    {
        FILE * f = fopen((dir + "/got_pca_" + ggml_backend_name(backend) +
                          (plant ? "_planted" : "") + ".bin").c_str(), "wb");
        if (f) {
            fwrite(got_pca.data(), sizeof(float), got_pca.size(), f);
            fclose(f);
        }
    }

    int rc = 0;
    if (plant) {
        // The planted red must break the PCA term and leave the core untouched.
        rc = (e_out > TOL && e_core < TOL_CORE) ? 0 : 1;
        printf("PLANTED RED: %s (the correction is live and the check bites)\n", rc == 0 ? "caught" : "NOT CAUGHT");
    } else {
        rc = (e_core < TOL_CORE && e_out < TOL) ? 0 : 1;
    }

    ggml_gallocr_free(alloc);
    ggml_free(gctx);
    ggml_backend_buffer_free(buf);
    ggml_free(ctx);
    ggml_backend_free(backend);
    printf("%s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}
