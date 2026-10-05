// lane-300 determinism probe: run the same prompt R times in one process, teacher-force runs 2..R to run 1's
// greedy ids, and compare per-step logits (and optionally every graph node's output bytes) against run 1.
// env: DETP_TOKENS=<file of comma/space separated ids>  DETP_RUNS=3  DETP_N=32
//      DETP_HASH=0|1|layer   0: logits only (graph untouched), 1: hash every node (one node per submit),
//                            layer: hash only nodes whose name starts with DETP_HASH_NAMES (comma list)
//      DETP_OUT=<jsonl file>
// usage: arifi-det-probe -m model.gguf -ngl 999 -fa on -c 2048 [any common args]
#include "arg.h"
#include "common.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "llama.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct rec { int step; std::string name; int op; int64_t ne[4]; uint64_t h; };

struct probe {
    int mode = 0;                       // 0 off, 1 all nodes, 2 name-filtered
    std::vector<std::string> names;
    int step = 0;
    std::vector<rec> * cur = nullptr;
    std::vector<uint8_t> buf;
};

static uint64_t fnv(const uint8_t * p, size_t n, uint64_t h) {
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

static bool is_index_op(const ggml_tensor * t) {
    return t->op == GGML_OP_ARGSORT || t->op == GGML_OP_TOP_K;
}

// stride-aware: hash each row's live bytes only (never the gaps of a view); index rows hash as a sorted set
static uint64_t hash_tensor(probe * p, const ggml_tensor * t) {
    const size_t nb = ggml_nbytes(t);
    p->buf.resize(nb);
    ggml_backend_tensor_get(t, p->buf.data(), 0, nb);
    const size_t row = ggml_row_size(t->type, t->ne[0]);
    const bool rows_ok = t->nb[0] == ggml_type_size(t->type) || ggml_is_quantized(t->type);
    uint64_t h = 1469598103934665603ull;
    std::vector<int32_t> ids;
    for (int64_t i3 = 0; i3 < t->ne[3]; ++i3)
    for (int64_t i2 = 0; i2 < t->ne[2]; ++i2)
    for (int64_t i1 = 0; i1 < t->ne[1]; ++i1) {
        const uint8_t * r = p->buf.data() + i1*t->nb[1] + i2*t->nb[2] + i3*t->nb[3];
        if (is_index_op(t) && t->type == GGML_TYPE_I32 && rows_ok) {
            ids.assign((const int32_t *) r, (const int32_t *) r + t->ne[0]);
            std::sort(ids.begin(), ids.end());
            h = fnv((const uint8_t *) ids.data(), ids.size()*4, h);
        } else if (rows_ok) {
            h = fnv(r, row, h);
        } else {
            for (int64_t i0 = 0; i0 < t->ne[0]; ++i0) h = fnv(r + i0*t->nb[0], ggml_type_size(t->type), h);
        }
    }
    return h;
}

// DETP_FA=1: for every FLASH_ATTN_EXT, report which KV columns the mask leaves live and the state of the dead ones
static FILE * g_fa_out = nullptr;
static int g_run = 0;

static std::vector<float> to_f32(const ggml_tensor * t) {
    std::vector<uint8_t> raw(ggml_nbytes(t));
    ggml_backend_tensor_get(t, raw.data(), 0, raw.size());
    std::vector<float> f((size_t) ggml_nelements(t));
    size_t k = 0;
    for (int64_t i3 = 0; i3 < t->ne[3]; ++i3)
    for (int64_t i2 = 0; i2 < t->ne[2]; ++i2)
    for (int64_t i1 = 0; i1 < t->ne[1]; ++i1)
    for (int64_t i0 = 0; i0 < t->ne[0]; ++i0) {
        const uint8_t * e = raw.data() + i0*t->nb[0] + i1*t->nb[1] + i2*t->nb[2] + i3*t->nb[3];
        f[k++] = t->type == GGML_TYPE_F16 ? ggml_fp16_to_fp32(*(const ggml_fp16_t *) e) : *(const float *) e;
    }
    return f;
}

static void fa_report(int step, const ggml_tensor * t) {
    const ggml_tensor * q = t->src[0], * k = t->src[1], * v = t->src[2], * m = t->src[3];
    if (!m || (m->type != GGML_TYPE_F16 && m->type != GGML_TYPE_F32) ||
        (k->type != GGML_TYPE_F16 && k->type != GGML_TYPE_F32) || (v->type != GGML_TYPE_F16 && v->type != GGML_TYPE_F32)) {
        fprintf(g_fa_out, "{\"run\":%d,\"fa\":\"%s\",\"step\":%d,\"skip_types\":\"%s/%s/%s\"}\n", g_run, t->name, step,
            ggml_type_name(k->type), ggml_type_name(v->type), m ? ggml_type_name(m->type) : "none");
        return;
    }
    const int64_t n_kv = k->ne[1], n_q = q->ne[1];
    const std::vector<float> mf = to_f32(m), kf = to_f32(k), vf = to_f32(v);
    // live[c] = some real query row leaves column c finite; mask rows beyond n_q are padding
    std::vector<char> live(n_kv, 0);
    long mask_nan = 0, mask_pinf = 0, row0_live = 0, rowl_live = 0, maxcol = -1;
    for (int64_t i2 = 0; i2 < m->ne[2] * m->ne[3]; ++i2)
    for (int64_t r = 0; r < std::min<int64_t>(n_q, m->ne[1]); ++r)
    for (int64_t c = 0; c < std::min<int64_t>(n_kv, m->ne[0]); ++c) {
        const float x = mf[(i2*m->ne[1] + r)*m->ne[0] + c];
        if (std::isnan(x)) ++mask_nan;
        if (x == INFINITY) ++mask_pinf;
        if (x > -INFINITY) { live[c] = 1; if (c > maxcol) maxcol = (long) c; if (i2 == 0 && r == 0) ++row0_live; if (i2 == 0 && r == n_q - 1) ++rowl_live; }
    }
    long n_live = 0; for (char c : live) n_live += c;
    auto side = [&](const std::vector<float> & f, const ggml_tensor * x, bool want_live, long & nonfin, double & maxabs) {
        uint64_t h = 1469598103934665603ull; nonfin = 0; maxabs = 0;
        const int64_t d = x->ne[0], nh = x->ne[2] * x->ne[3];
        for (int64_t h2 = 0; h2 < nh; ++h2)
        for (int64_t c = 0; c < n_kv; ++c) {
            if ((bool) live[c] != want_live) continue;
            const float * row = f.data() + (h2*n_kv + c)*d;
            h = fnv((const uint8_t *) row, sizeof(float)*d, h);
            for (int64_t i = 0; i < d; ++i) { if (!std::isfinite(row[i])) ++nonfin; else maxabs = std::max(maxabs, (double) std::fabs(row[i])); }
        }
        return h;
    };
    long kln, kdn, vln, vdn; double klm, kdm, vlm, vdm;
    const uint64_t klh = side(kf, k, true, kln, klm), kdh = side(kf, k, false, kdn, kdm);
    const uint64_t vlh = side(vf, v, true, vln, vlm), vdh = side(vf, v, false, vdn, vdm);
    std::vector<uint8_t> mraw(ggml_nbytes(m)); ggml_backend_tensor_get(m, mraw.data(), 0, mraw.size());
    fprintf(g_fa_out, "{\"run\":%d,\"fa\":\"%s\",\"step\":%d,\"n_q\":%lld,\"n_kv\":%lld,\"mask_type\":\"%s\",\"mask_ne\":[%lld,%lld,%lld,%lld],"
        "\"n_live\":%ld,\"maxcol\":%ld,\"row0_live\":%ld,\"rowlast_live\":%ld,\"mask_nan\":%ld,\"mask_pinf\":%ld,"
        "\"k_live_h\":\"%016llx\",\"k_dead_h\":\"%016llx\",\"k_live_nonfinite\":%ld,\"k_dead_nonfinite\":%ld,\"k_dead_maxabs\":%.4g,\"k_live_maxabs\":%.4g,"
        "\"v_live_h\":\"%016llx\",\"v_dead_h\":\"%016llx\",\"v_live_nonfinite\":%ld,\"v_dead_nonfinite\":%ld,\"v_dead_maxabs\":%.4g,\"v_live_maxabs\":%.4g,\"mask_h\":\"%016llx\"}\n",
        g_run, t->name, step, (long long) n_q, (long long) n_kv, ggml_type_name(m->type),
        (long long) m->ne[0], (long long) m->ne[1], (long long) m->ne[2], (long long) m->ne[3],
        n_live, maxcol, row0_live, rowl_live, mask_nan, mask_pinf,
        (unsigned long long) klh, (unsigned long long) kdh, kln, kdn, kdm, klm,
        (unsigned long long) vlh, (unsigned long long) vdh, vln, vdn, vdm, vlm,
        (unsigned long long) fnv(mraw.data(), mraw.size(), 1469598103934665603ull));
}

static bool want(const probe * p, const ggml_tensor * t) {
    if (g_fa_out && t->op == GGML_OP_FLASH_ATTN_EXT) return true;
    if (p->mode == 1) return true;
    for (const auto & n : p->names) if (strncmp(t->name, n.c_str(), n.size()) == 0) return true;
    return false;
}

static bool cb(ggml_tensor * t, bool ask, void * ud) {
    probe * p = (probe *) ud;
    if (p->mode == 0 || !p->cur) return ask ? false : true;
    if (ask) return want(p, t);
    if (g_fa_out && t->op == GGML_OP_FLASH_ATTN_EXT) fa_report(p->step, t);
    rec r { p->step, t->name, (int) t->op, { t->ne[0], t->ne[1], t->ne[2], t->ne[3] }, hash_tensor(p, t) };
    p->cur->push_back(r);
    return true;
}

static std::vector<llama_token> read_ids(const char * f) {
    std::ifstream in(f);
    std::stringstream ss; ss << in.rdbuf();
    std::string s = ss.str();
    std::replace(s.begin(), s.end(), ',', ' ');
    std::istringstream is(s);
    std::vector<llama_token> v; long long x;
    while (is >> x) v.push_back((llama_token) x);
    return v;
}

static int envi(const char * k, int d) { const char * v = getenv(k); return v ? atoi(v) : d; }

int main(int argc, char ** argv) {
    common_params params;
    if (!common_params_parse(argc, argv, params, LLAMA_EXAMPLE_COMMON)) return 1;
    common_init();

    const char * tf = getenv("DETP_TOKENS");
    const char * of = getenv("DETP_OUT");
    if (!tf || !of) { fprintf(stderr, "DETP_TOKENS and DETP_OUT required\n"); return 1; }
    const int R = envi("DETP_RUNS", 3), N = envi("DETP_N", 32);

    probe pr;
    const char * hm = getenv("DETP_HASH");
    if (hm && strcmp(hm, "1") == 0) pr.mode = 1;
    if (hm && strcmp(hm, "layer") == 0) {
        pr.mode = 2;
        std::string s = getenv("DETP_HASH_NAMES") ? getenv("DETP_HASH_NAMES") : "l_out,ffn_out,ffn_moe_out,attn_out,result_output";
        std::istringstream is(s); std::string tok;
        while (std::getline(is, tok, ',')) if (!tok.empty()) pr.names.push_back(tok);
    }
    if (const char * ff = getenv("DETP_FA")) {
        g_fa_out = fopen(ff, "w");
        if (pr.mode == 0) pr.mode = 2;   // no names: only FA nodes are asked for (and hashed)
    }
    if (pr.mode != 0) { params.cb_eval = cb; params.cb_eval_user_data = &pr; }
    params.warmup = false;

    llama_backend_init();
    auto init = common_init_from_params(params);
    llama_model * model = init->model();
    llama_context * ctx = init->context();
    if (!model || !ctx) { fprintf(stderr, "load failed\n"); return 1; }
    const int n_vocab = llama_vocab_n_tokens(llama_model_get_vocab(model));

    const std::vector<llama_token> prompt = read_ids(tf);
    FILE * out = fopen(of, "w");
    std::vector<llama_token> ref;                       // run-1 greedy ids
    std::vector<uint64_t> ref_lh;                       // run-1 logits hash per step
    std::vector<rec> ref_nodes;
    int n_bad_runs = 0;

    // history arms: DETP_CLEAR=data|nodata|seq (how memory is reset between runs; seq = llama-server style seq_rm),
    // DETP_PRE=<ids file>: before every odd run, decode this prompt + DETP_PRE_N greedy steps, then reset again
    const char * cm = getenv("DETP_CLEAR");
    const int clear_mode = !cm || strcmp(cm, "data") == 0 ? 0 : strcmp(cm, "nodata") == 0 ? 1 : 2;
    const char * pf = getenv("DETP_PRE");
    const std::vector<llama_token> pre = pf ? read_ids(pf) : std::vector<llama_token>();
    const int pre_n = envi("DETP_PRE_N", 16);
    auto reset = [&]() {
        llama_memory_t mem = llama_get_memory(ctx);
        if (clear_mode == 2) llama_memory_seq_rm(mem, 0, -1, -1);
        else llama_memory_clear(mem, clear_mode == 0);
    };
    auto pollute = [&]() {
        llama_batch b = llama_batch_init((int) pre.size(), 0, 1);
        for (size_t i = 0; i < pre.size(); ++i) common_batch_add(b, pre[i], (llama_pos) i, { 0 }, i + 1 == pre.size());
        if (llama_decode(ctx, b) != 0) { fprintf(stderr, "pre decode failed\n"); exit(3); }
        llama_batch_free(b);
        for (int s = 0; s < pre_n; ++s) {
            const float * lg = llama_get_logits_ith(ctx, -1);
            int a = 0; for (int i = 1; i < n_vocab; ++i) if (lg[i] > lg[a]) a = i;
            llama_batch c = llama_batch_init(1, 0, 1);
            common_batch_add(c, a, (llama_pos) (pre.size() + s), { 0 }, true);
            if (llama_decode(ctx, c) != 0) { fprintf(stderr, "pre step failed\n"); exit(3); }
            llama_batch_free(c);
        }
    };
    // GOAL 3b, DETP_ROWS="1,2,4,5,8,1": row-independence. Build S = prompt + greedy ids (zeroed memory), then for each
    // target position t (DETP_ROWS_POS offsets from the last prompt position) and width w: zero memory, prefill
    // S[0..t-8], single-step S[t-7..t-w], decode S[t-w+1..t] as ONE batch (every row outputs), compare row t's logits.
    if (const char * rw = getenv("DETP_ROWS")) {
        std::vector<int> widths, offs;
        { std::string s = rw; std::replace(s.begin(), s.end(), ',', ' '); std::istringstream is(s); int x; while (is >> x) widths.push_back(x); }
        { std::string s = getenv("DETP_ROWS_POS") ? getenv("DETP_ROWS_POS") : "0,36"; std::replace(s.begin(), s.end(), ',', ' ');
          std::istringstream is(s); int x; while (is >> x) offs.push_back(x); }
        const int max_off = *std::max_element(offs.begin(), offs.end());
        llama_memory_t mem = llama_get_memory(ctx);
        auto dec = [&](const std::vector<llama_token> & S, int p0, int p1, bool all) {   // positions [p0, p1)
            if (p1 <= p0) return;
            llama_batch b = llama_batch_init(p1 - p0, 0, 1);
            for (int i = p0; i < p1; ++i) common_batch_add(b, S[i], (llama_pos) i, { 0 }, all || i + 1 == p1);
            if (llama_decode(ctx, b) != 0) { fprintf(stderr, "rows decode failed\n"); exit(3); }
            llama_batch_free(b);
        };
        std::vector<llama_token> S = prompt;
        llama_memory_clear(mem, true);
        dec(S, 0, (int) S.size(), false);
        for (int s = 0; s < max_off; ++s) {
            const float * lg = llama_get_logits_ith(ctx, -1);
            int a = 0; for (int i = 1; i < n_vocab; ++i) if (lg[i] > lg[a]) a = i;
            S.push_back(a);
            dec(S, (int) S.size() - 1, (int) S.size(), true);
        }
        int bad = 0;
        for (int off : offs) {
            const int t = (int) prompt.size() - 1 + off;
            std::vector<float> ref_lg;
            uint64_t ref_h = 0;
            for (size_t wi = 0; wi < widths.size(); ++wi) {
                const int w = widths[wi];
                llama_memory_clear(mem, true);
                pr.step = (int) wi;
                dec(S, 0, t - 7, false);
                for (int i = t - 7; i <= t - w; ++i) dec(S, i, i + 1, true);
                dec(S, t - w + 1, t + 1, true);
                const float * lg = llama_get_logits_ith(ctx, -1);
                const uint64_t h = fnv((const uint8_t *) lg, sizeof(float) * n_vocab, 1469598103934665603ull);
                int a = 0; for (int i = 1; i < n_vocab; ++i) if (lg[i] > lg[a]) a = i;
                double md = 0;
                if (wi == 0) { ref_lg.assign(lg, lg + n_vocab); ref_h = h; }
                else for (int i = 0; i < n_vocab; ++i) md = std::max(md, (double) std::fabs(lg[i] - ref_lg[i]));
                const bool red = wi > 0 && h != ref_h;
                bad += red;
                fprintf(out, "{\"rows\":true,\"t\":%d,\"w\":%d,\"argmax\":%d,\"lh\":\"%016llx\",\"maxdiff_vs_first\":%.6g,\"red\":%d}\n",
                        t, w, a, (unsigned long long) h, md, (int) red);
                printf("rows t=%d w=%d argmax=%d lh=%016llx maxdiff=%.6g %s\n", t, w, a, (unsigned long long) h, md, red ? "RED" : "ok");
                fflush(out);
            }
        }
        fprintf(out, "{\"summary\":true,\"rows_mode\":true,\"bad\":%d}\n", bad);
        fclose(out);
        if (g_fa_out) fclose(g_fa_out);
        printf("DETP rows bad=%d\n", bad);
        llama_backend_free();
        return 0;
    }

    fprintf(out, "{\"clear_mode\":%d,\"pre\":%zu,\"pre_n\":%d}\n", clear_mode, pre.size(), pre_n);

    for (int r = 0; r < R; ++r) {
        g_run = r;
        if (r == 0) llama_memory_clear(llama_get_memory(ctx), true);
        else reset();
        if (!pre.empty() && (r & 1)) { pr.cur = nullptr; pollute(); reset(); }
        std::vector<rec> nodes;
        pr.cur = &nodes;
        std::vector<llama_token> ids;
        int first_logit_diff = -1, first_id_diff = -1;
        for (int s = 0; s < N; ++s) {
            pr.step = s;
            int rc;
            if (s == 0) {
                llama_batch b = llama_batch_init((int) prompt.size(), 0, 1);
                for (size_t i = 0; i < prompt.size(); ++i) common_batch_add(b, prompt[i], (llama_pos) i, { 0 }, i + 1 == prompt.size());
                rc = llama_decode(ctx, b);
                llama_batch_free(b);
            } else {
                llama_token t = r == 0 ? ids.back() : ref[s - 1];
                llama_batch b = llama_batch_init(1, 0, 1);
                common_batch_add(b, t, (llama_pos) (prompt.size() + s - 1), { 0 }, true);
                rc = llama_decode(ctx, b);
                llama_batch_free(b);
            }
            if (rc != 0) { fprintf(stderr, "decode rc=%d run %d step %d\n", rc, r, s); return 2; }
            const float * lg = llama_get_logits_ith(ctx, -1);
            int a = 0, b2 = -1;
            for (int i = 1; i < n_vocab; ++i) if (lg[i] > lg[a]) a = i;
            for (int i = 0; i < n_vocab; ++i) if (i != a && (b2 < 0 || lg[i] > lg[b2])) b2 = i;
            // log-softmax gap top1-top2 equals the raw logit gap
            const double gap = (double) lg[a] - (double) lg[b2];
            const uint64_t lh = fnv((const uint8_t *) lg, sizeof(float) * n_vocab, 1469598103934665603ull);
            ids.push_back(a);
            if (r == 0) { ref.push_back(a); ref_lh.push_back(lh); }
            else {
                if (first_logit_diff < 0 && lh != ref_lh[s]) first_logit_diff = s;
                if (first_id_diff < 0 && a != ref[s]) first_id_diff = s;
            }
            fprintf(out, "{\"run\":%d,\"step\":%d,\"argmax\":%d,\"second\":%d,\"gap\":%.6f,\"lh\":\"%016llx\"}\n",
                    r, s, a, b2, gap, (unsigned long long) lh);
        }
        pr.cur = nullptr;
        if (r == 0) { ref_nodes = nodes; fprintf(out, "{\"run\":0,\"nodes\":%zu}\n", nodes.size()); }
        else {
            if (first_logit_diff >= 0) ++n_bad_runs;
            // first node whose values differ, in graph order; plus the first 12 differing nodes of that step
            long first = -1; int nd = 0;
            const size_t m = std::min(nodes.size(), ref_nodes.size());
            for (size_t i = 0; i < m; ++i) {
                const rec & x = nodes[i]; const rec & y = ref_nodes[i];
                if (x.name != y.name || x.step != y.step) { fprintf(out, "{\"run\":%d,\"node_misalign\":%zu}\n", r, i); break; }
                // whole-cache views/writes hash stale cells too, so they differ by construction under DETP_PRE
                if (x.h != y.h && strncmp(x.name.c_str(), "cache_", 6) != 0) {
                    if (first < 0) first = (long) i;
                    if (x.step != nodes[first].step) break;
                    if (nd++ < 40) {
                        fprintf(out, "{\"run\":%d,\"diff_node\":%zu,\"step\":%d,\"name\":\"%s\",\"op\":\"%s\",\"ne\":[%lld,%lld,%lld,%lld]}\n",
                            r, i, x.step, x.name.c_str(), ggml_op_name((ggml_op) x.op),
                            (long long) x.ne[0], (long long) x.ne[1], (long long) x.ne[2], (long long) x.ne[3]);
                    }
                }
            }
            fprintf(out, "{\"run\":%d,\"first_logit_diff_step\":%d,\"first_id_diff_step\":%d,\"nodes\":%zu,\"first_diff_node\":%ld}\n",
                    r, first_logit_diff, first_id_diff, nodes.size(), first);
            printf("run %d: first_logit_diff_step=%d first_id_diff_step=%d first_diff_node=%ld %s\n", r, first_logit_diff,
                   first_id_diff, first, first >= 0 ? nodes[first].name.c_str() : "");
        }
        fflush(out);
    }
    fprintf(out, "{\"summary\":true,\"runs\":%d,\"n\":%d,\"bad_runs\":%d,\"hash_mode\":%d}\n", R, N, n_bad_runs, pr.mode);
    fclose(out);
    if (g_fa_out) fclose(g_fa_out);
    printf("DETP runs=%d n=%d bad_runs=%d hash_mode=%d\n", R, N, n_bad_runs, pr.mode);
    llama_backend_free();
    return 0;
}
