// lane-296 int8 op probe: error of ONE Q8_0 matmul on the Vulkan backend against an f64 host reference.
// synth  <tag> [K M N]                 : random Q8_0 weight, activations with controlled per-block amax
// replay <tag> <gguf> <w-name> <x.f32> : real weight from a GGUF, captured activations (int32 K, int32 N, f32 data)
// The path is chosen by the usual GGML_ARIFI_Q8_0_CM1* env switches (read at device init).
#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "gguf.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static const int REF_ROWS = 256;  // ponytail: f64 reference on a row subset, full rows if a defect needs it

struct stats { double rel_fro, max_rel_row, med_rel_row; };

static stats compare(const float * y, const std::vector<double> & ref, int M, int N, const std::vector<int> & rows) {
    double num = 0, den = 0;
    std::vector<double> per_row;
    for (int ri = 0; ri < (int) rows.size(); ri++) {
        double rn = 0, rd = 0;
        for (int n = 0; n < N; n++) {
            const double r = ref[(size_t) ri * N + n];
            const double e = (double) y[(size_t) n * M + rows[ri]] - r;
            rn += e * e; rd += r * r;
        }
        num += rn; den += rd;
        per_row.push_back(std::sqrt(rn / (rd + 1e-300)));
    }
    std::vector<double> s = per_row; std::sort(s.begin(), s.end());
    return { std::sqrt(num / (den + 1e-300)), s.back(), s[s.size() / 2] };
}

static float f16r(float v) { return ggml_fp16_to_fp32(ggml_fp32_to_fp16(v)); }
static float f16z(float v) {  // round toward zero
    ggml_fp16_t h = ggml_fp32_to_fp16(v);
    if (std::fabs(ggml_fp16_to_fp32(h)) > std::fabs(v) && (h & 0x7fff)) h = (ggml_fp16_t) (h - 1);
    return ggml_fp16_to_fp32(h);
}
static bool g_rtz_sub = false;  // residual subtract uses a round-toward-zero f16 scale (packHalf2x16 on AMD, ASSUMED)

// simulate q8_1 activations (32-block, d = amax/127 f32, q = round(x/d), scale stored f16), digits 1 or 2
static std::vector<float> sim_q8(const std::vector<float> & x, int digits, int shift, bool ftz, int bs) {
    std::vector<float> out(x.size());
    for (size_t b = 0; b < x.size(); b += bs) {
        float v[256], acc[256] = {0};
        for (int i = 0; i < bs; i++) v[i] = x[b + i];
        for (int dg = 0; dg < digits; dg++) {
            const float pre = dg == 0 ? 1.0f : std::ldexp(1.0f, shift);
            float amax = 0;
            for (int i = 0; i < bs; i++) amax = std::max(amax, std::fabs(v[i] * pre));
            const float d = amax / 127.0f;
            const float dinv = d != 0 ? 1.0f / d : 0;
            float ds = f16r(d);
            if (ftz && std::fabs(ds) < 6.103515625e-05f) ds = 0;
            float ds_sub = g_rtz_sub ? f16z(d) : f16r(d);  // quantize shader subtracts with its own f16 conversion
            if (ftz && std::fabs(ds_sub) < 6.103515625e-05f) ds_sub = 0;
            for (int i = 0; i < bs; i++) {
                const float q = std::nearbyint(v[i] * pre * dinv);
                acc[i] += q * ds / pre;
                v[i] -= q * ds_sub / pre;
            }
        }
        for (int i = 0; i < bs; i++) out[b + i] = acc[i];
    }
    return out;
}

static std::vector<double> host_ref(const std::vector<float> & wf, const std::vector<float> & x, int K, int N, const std::vector<int> & rows) {
    std::vector<double> ref((size_t) rows.size() * N);
    for (int ri = 0; ri < (int) rows.size(); ri++) {
        const float * w = &wf[(size_t) rows[ri] * K];
        for (int n = 0; n < N; n++) {
            const float * xv = &x[(size_t) n * K];
            double s = 0;
            for (int k = 0; k < K; k++) s += (double) w[k] * xv[k];
            ref[(size_t) ri * N + n] = s;
        }
    }
    return ref;
}

static std::vector<float> run_gpu(ggml_backend_t be, const std::string & wname, const std::vector<uint8_t> & wq, const std::vector<float> & x,
                                  int K, int M, int N, double & ms) {
    ggml_init_params ip = { 4 * ggml_tensor_overhead() + ggml_graph_overhead(), nullptr, true };
    ggml_context * ctx = ggml_init(ip);
    ggml_tensor * w = ggml_new_tensor_2d(ctx, GGML_TYPE_Q8_0, K, M);
    ggml_set_name(w, wname.c_str());
    // PROBE_NSEQ=s: same memory viewed as [K, N/s, s] (the batched shape of qwen35 ssm_out with s sequences)
    const int s = std::getenv("PROBE_NSEQ") ? atoi(std::getenv("PROBE_NSEQ")) : 1;
    ggml_tensor * xt = s > 1 ? ggml_new_tensor_3d(ctx, GGML_TYPE_F32, K, N / s, s) : ggml_new_tensor_2d(ctx, GGML_TYPE_F32, K, N);
    ggml_set_name(xt, "probe_x");
    ggml_tensor * y = ggml_mul_mat(ctx, w, xt);
    ggml_cgraph * gf = ggml_new_graph(ctx);
    ggml_build_forward_expand(gf, y);
    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors(ctx, be);
    ggml_backend_tensor_set(w, wq.data(), 0, wq.size());
    ggml_backend_tensor_set(xt, x.data(), 0, x.size() * sizeof(float));
    ggml_backend_graph_compute(be, gf);
    const int reps = 10;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < reps; i++) ggml_backend_graph_compute(be, gf);
    ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / reps;
    std::vector<float> out((size_t) M * N);
    ggml_backend_tensor_get(y, out.data(), 0, out.size() * sizeof(float));
    ggml_backend_buffer_free(buf);
    ggml_free(ctx);
    return out;
}

static void block_profile(const std::vector<float> & x, const char * label) {
    size_t nb = x.size() / 32, sub1 = 0, sub2 = 0, zero = 0;
    std::vector<float> amax(nb), ratio(nb);
    for (size_t b = 0; b < nb; b++) {
        float a = 0; std::vector<float> m(32);
        for (int i = 0; i < 32; i++) { a = std::max(a, std::fabs(x[b * 32 + i])); m[i] = std::fabs(x[b * 32 + i]); }
        std::nth_element(m.begin(), m.begin() + 16, m.end());
        amax[b] = a; ratio[b] = m[16] > 0 ? a / m[16] : 0;
        if (a == 0) zero++; else if (a < 127 * 6.103515625e-05f) sub1++;
        if (a > 0 && a * 256 < 127 * 6.103515625e-05f) sub2++;
    }
    std::sort(amax.begin(), amax.end()); std::sort(ratio.begin(), ratio.end());
    printf("PROFILE %s blocks=%zu amax p1=%.3g p50=%.3g p99=%.3g max=%.3g | max/median p50=%.2f p99=%.2f max=%.2f | "
           "zero=%.4f%% d1-subnormal(amax<0.00775)=%.4f%% d2-subnormal@2^8=%.4f%%\n",
           label, nb, amax[nb / 100], amax[nb / 2], amax[nb * 99 / 100], amax.back(), ratio[nb / 2], ratio[nb * 99 / 100], ratio.back(),
           100.0 * zero / nb, 100.0 * sub1 / nb, 100.0 * sub2 / nb);
}

static void evaluate(ggml_backend_t be, const char * tag, const std::string & label, const std::string & wname,
                     const std::vector<uint8_t> & wq, const std::vector<float> & x, int K, int M, int N) {
    std::vector<float> wf((size_t) K * M);
    ggml_get_type_traits(GGML_TYPE_Q8_0)->to_float(wq.data(), wf.data(), (int64_t) K * M);
    std::vector<int> rows;
    for (int i = 0; i < std::min(REF_ROWS, M); i++) rows.push_back((int) ((int64_t) i * M / std::min(REF_ROWS, M)));
    const std::vector<double> ref = host_ref(wf, x, K, N, rows);
    if (const char * dp = std::getenv("PROBE_DST")) {  // in-graph output captured by arifi-op-capture
        FILE * fd = fopen(dp, "rb"); int32_t h[2] = {0, 0};
        if (fd && fread(h, 4, 2, fd) == 2 && h[0] == M && h[1] == N) {
            std::vector<float> yd((size_t) M * N);
            if (fread(yd.data(), 4, yd.size(), fd) == yd.size()) {
                stats d = compare(yd.data(), ref, M, N, rows);
                printf("INGRAPH %s case=%s rel_fro=%.4e max_row=%.4e med_row=%.4e\n", tag, label.c_str(), d.rel_fro, d.max_rel_row, d.med_rel_row);
            }
        } else printf("INGRAPH %s case=%s missing or shape mismatch (%d x %d)\n", tag, label.c_str(), h[0], h[1]);
        if (fd) fclose(fd);
    }
    double ms = 0;
    const std::vector<float> y = run_gpu(be, wname, wq, x, K, M, N, ms);
    stats g = compare(y.data(), ref, M, N, rows);
    std::vector<double> cn(N, 0), cd(N, 0);  // per token (column)
    for (int ri = 0; ri < (int) rows.size(); ri++) for (int n = 0; n < N; n++) {
        const double r = ref[(size_t) ri * N + n], e = y[(size_t) n * M + rows[ri]] - r;
        cn[n] += e * e; cd[n] += r * r;
    }
    int worst = 0; std::vector<double> cr(N);
    for (int n = 0; n < N; n++) { cr[n] = std::sqrt(cn[n] / (cd[n] + 1e-300)); if (cr[n] > cr[worst]) worst = n; }
    double num1 = 0, den1 = 0; for (int n = 1; n < N; n++) { num1 += cn[n]; den1 += cd[n]; }
    printf("GPU %s case=%s rel_fro=%.4e max_row=%.4e med_row=%.4e worst_tok=%d(%.4e) tok0=%.4e rel_fro_excl_tok0=%.4e op_ms=%.3f (indication, untimed law)\n",
           tag, label.c_str(), g.rel_fro, g.max_rel_row, g.med_rel_row, worst, cr[worst], cr[0], std::sqrt(num1 / (den1 + 1e-300)), ms);
    if (std::getenv("PROBE_SIM") == nullptr) return;
    block_profile(x, label.c_str());
    struct { const char * name; int digits, shift; bool ftz; int bs; bool f16; bool rtz_sub; } sims[] = {
        {"f16-act", 0, 0, false, 32, true, false}, {"q8-1d", 1, 0, false, 32, false, false},
        {"q8-2d-s0", 2, 0, false, 32, false, false}, {"q8-2d-s8", 2, 8, false, 32, false, false},
        {"q8-2d-s0-rtzsub", 2, 0, false, 32, false, true}, {"q8-2d-s8-rtzsub", 2, 8, false, 32, false, true},
    };
    for (auto & s : sims) {
        g_rtz_sub = s.rtz_sub;
        std::vector<float> xs;
        if (s.f16) { xs = x; for (auto & v : xs) v = f16r(v); } else xs = sim_q8(x, s.digits, s.shift, s.ftz, s.bs);
        std::vector<double> r2 = host_ref(wf, xs, K, N, rows);
        std::vector<float> yf((size_t) M * N, 0.0f);
        for (int ri = 0; ri < (int) rows.size(); ri++) for (int n = 0; n < N; n++) yf[(size_t) n * M + rows[ri]] = (float) r2[(size_t) ri * N + n];
        stats st = compare(yf.data(), ref, M, N, rows);
        // distance GPU vs this sim = engagement witness
        double num = 0, den = 0;
        for (int ri = 0; ri < (int) rows.size(); ri++) for (int n = 0; n < N; n++) {
            const double a = y[(size_t) n * M + rows[ri]], b = r2[(size_t) ri * N + n];
            num += (a - b) * (a - b); den += b * b;
        }
        printf("SIM %s case=%s sim=%s rel_fro=%.4e max_row=%.4e | gpu-vs-sim=%.4e\n", tag, label.c_str(), s.name, st.rel_fro, st.max_rel_row, std::sqrt(num / (den + 1e-300)));
    }
}

int main(int argc, char ** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s synth <tag> [K M N] | replay <tag> <gguf> <w-name> <x.f32>\n", argv[0]); return 1; }
    const std::string mode = argv[1];
    const char * tag = argv[2];
    ggml_backend_load_all();
    ggml_backend_dev_t dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_GPU);
    if (!dev) dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_IGPU);
    if (!dev) { fprintf(stderr, "no GPU device\n"); return 1; }
    ggml_backend_t be = ggml_backend_dev_init(dev, nullptr);

    if (mode == "synth") {
        const int K = argc > 3 ? atoi(argv[3]) : 6144, M = argc > 4 ? atoi(argv[4]) : 5120, N = argc > 5 ? atoi(argv[5]) : 512;
        std::mt19937 rng(1234);
        std::normal_distribution<float> nd(0.0f, 1.0f);
        std::vector<float> wf((size_t) K * M);
        for (auto & v : wf) v = 0.02f * nd(rng);
        std::vector<uint8_t> wq(ggml_row_size(GGML_TYPE_Q8_0, K) * M);
        ggml_quantize_chunk(GGML_TYPE_Q8_0, wf.data(), wq.data(), 0, M, K, nullptr);
        std::vector<float> base((size_t) K * N);
        for (auto & v : base) v = nd(rng);
        for (float target : {1.0f, 0.02f, 0.005f, 0.001f, 1e-5f}) {
            std::vector<float> x = base;
            for (size_t b = 0; b < x.size(); b += 32) {
                float a = 0; for (int i = 0; i < 32; i++) a = std::max(a, std::fabs(x[b + i]));
                for (int i = 0; i < 32; i++) x[b + i] *= target / a;
            }
            char lbl[64]; snprintf(lbl, sizeof lbl, "amax=%g", target);
            evaluate(be, tag, lbl, "blk.0.ssm_out.weight", wq, x, K, M, N);
        }
        std::vector<float> x = base;  // heavy tail: 1 value in 64 x30
        for (size_t i = 0; i < x.size(); i += 64) x[i] *= 30.0f;
        evaluate(be, tag, "heavytail", "blk.0.ssm_out.weight", wq, x, K, M, N);
    } else if (mode == "replay" && argc >= 6) {
        const char * gpath = argv[3]; const std::string wname = argv[4];
        FILE * fx = fopen(argv[5], "rb");
        if (!fx) { fprintf(stderr, "no x file\n"); return 1; }
        int32_t hdr[2]; if (fread(hdr, 4, 2, fx) != 2) return 1;
        const int K = hdr[0], N = hdr[1];
        std::vector<float> x((size_t) K * N);
        if (fread(x.data(), 4, x.size(), fx) != x.size()) return 1;
        fclose(fx);
        gguf_init_params gp = { true, nullptr };
        gguf_context * g = gguf_init_from_file(gpath, gp);
        const int64_t ti = gguf_find_tensor(g, wname.c_str());
        if (ti < 0 || gguf_get_tensor_type(g, ti) != GGML_TYPE_Q8_0) { fprintf(stderr, "tensor %s missing or not Q8_0\n", wname.c_str()); return 1; }
        const size_t sz = gguf_get_tensor_size(g, ti);
        const size_t off = gguf_get_data_offset(g) + gguf_get_tensor_offset(g, ti);
        const int M = (int) (sz / ggml_row_size(GGML_TYPE_Q8_0, K));
        std::vector<uint8_t> wq(sz);
        FILE * fw = fopen(gpath, "rb");
        _fseeki64(fw, (long long) off, SEEK_SET);
        if (fread(wq.data(), 1, sz, fw) != sz) return 1;
        fclose(fw); gguf_free(g);
        evaluate(be, tag, wname, wname, wq, x, K, M, N);
    } else if (mode == "multi" && argc >= 6) {
        // multi <tag> <gguf> <w1> <x1> [<w2> <x2> ...]: ONE graph, no callback, every matmul checked vs f64
        // (exercises q8_1 reuse cache, prealloc_y and split-k slice reuse between nodes)
        const char * gpath = argv[3];
        gguf_init_params gp = { true, nullptr };
        gguf_context * g = gguf_init_from_file(gpath, gp);
        FILE * fw = fopen(gpath, "rb");
        ggml_init_params ip = { 64 * ggml_tensor_overhead() + ggml_graph_overhead(), nullptr, true };
        ggml_context * ctx = ggml_init(ip);
        ggml_cgraph * gf = ggml_new_graph(ctx);
        struct item { std::string w; std::vector<uint8_t> wq; std::vector<float> x; int K, M, N; ggml_tensor * wt, * xt, * y; };
        std::vector<item> its;
        for (int a = 4; a + 1 < argc; a += 2) {
            item it; it.w = argv[a];
            FILE * fx = fopen(argv[a + 1], "rb"); int32_t h[2];
            if (!fx || fread(h, 4, 2, fx) != 2) return 1;
            it.K = h[0]; it.N = h[1]; it.x.resize((size_t) it.K * it.N);
            if (fread(it.x.data(), 4, it.x.size(), fx) != it.x.size()) return 1;
            fclose(fx);
            const int64_t ti = gguf_find_tensor(g, it.w.c_str());
            const size_t sz = gguf_get_tensor_size(g, ti);
            it.M = (int) (sz / ggml_row_size(GGML_TYPE_Q8_0, it.K));
            it.wq.resize(sz);
            _fseeki64(fw, (long long) (gguf_get_data_offset(g) + gguf_get_tensor_offset(g, ti)), SEEK_SET);
            if (fread(it.wq.data(), 1, sz, fw) != sz) return 1;
            its.push_back(std::move(it));
        }
        for (auto & it : its) {
            it.wt = ggml_new_tensor_2d(ctx, GGML_TYPE_Q8_0, it.K, it.M); ggml_set_name(it.wt, it.w.c_str());
            it.xt = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, it.K, it.N);
            it.y = ggml_mul_mat(ctx, it.wt, it.xt);
            ggml_build_forward_expand(gf, it.y);
        }
        ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors(ctx, be);
        for (auto & it : its) {
            ggml_backend_tensor_set(it.wt, it.wq.data(), 0, it.wq.size());
            ggml_backend_tensor_set(it.xt, it.x.data(), 0, it.x.size() * 4);
        }
        for (int rep = 0; rep < 2; rep++) ggml_backend_graph_compute(be, gf);
        for (size_t i = 0; i < its.size(); i++) {
            auto & it = its[i];
            std::vector<float> wf((size_t) it.K * it.M), y((size_t) it.M * it.N);
            ggml_get_type_traits(GGML_TYPE_Q8_0)->to_float(it.wq.data(), wf.data(), (int64_t) it.K * it.M);
            std::vector<int> rows;
            for (int r = 0; r < REF_ROWS; r++) rows.push_back((int) ((int64_t) r * it.M / REF_ROWS));
            ggml_backend_tensor_get(it.y, y.data(), 0, y.size() * 4);
            stats st = compare(y.data(), host_ref(wf, it.x, it.K, it.N, rows), it.M, it.N, rows);
            printf("MULTI %s node=%zu case=%s rel_fro=%.4e max_row=%.4e\n", tag, i, it.w.c_str(), st.rel_fro, st.max_rel_row);
        }
        ggml_backend_buffer_free(buf); ggml_free(ctx); fclose(fw); gguf_free(g);
    } else {
        fprintf(stderr, "bad args\n"); return 1;
    }
    ggml_backend_free(be);
    return 0;
}
