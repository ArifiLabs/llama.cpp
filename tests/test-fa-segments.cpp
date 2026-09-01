// ArifiLabs lane-195 — PER-QUERY SOURCE SELECTION, the CPU gate.
//
// `ggml_flash_attn_ext_add_segments()` lets one flash-attention node read its KV window from
// SEVERAL tensors, each at its own dtype, folded into ONE fp32 online softmax — so a KV cache whose
// newest cells live in an exact F16 ring beside a quantized body never has to materialise the
// window at all.
//
// The whole risk in that design is INDEXING: which mask column a segment's row `ic` corresponds to,
// and whether the running (S, M, VKQ) really carries across a segment boundary. Both are decidable
// on the CPU with no GPU time, and this file decides them:
//
//   LEG 1  SPLIT-EQUIVALENCE.  Identical F16 data, attended once as ONE tensor and once as a body
//          plus two segments cut at every awkward boundary. The two must agree. This is the check
//          that fails if a mask base, a row offset or the softmax continuation is wrong.
//   LEG 2  NEGATIVE CONTROL.   Leg 1 re-run with the segment boundary deliberately off by one row.
//          It MUST disagree. Without this, leg 1's green could be vacuous — two code paths that are
//          both wrong in the same way, or a comparison that never actually looked.
//   LEG 3  MIXED DTYPE.        q4_0 body + F16 ring against an INDEPENDENT reference computed in
//          this file (plain softmax, q4_0 dequantised through ggml's own to_float). This is the leg
//          that proves segments are read at their own dtype rather than silently reinterpreted.
//
// Leg 3's reference is deliberately not another ggml graph: a bug shared by both graphs would
// cancel, and the point of a reference is that it cannot share our bug.

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

static std::mt19937 g_rng(1195);

static std::vector<float> rand_vec(size_t n, float lo = -1.0f, float hi = 1.0f) {
    std::uniform_real_distribution<float> d(lo, hi);
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) {
        v[i] = d(g_rng);
    }
    return v;
}

// ---------------------------------------------------------------------------------------------
// A tiny harness: one FA node, optionally with segments, evaluated on the CPU backend.
// ---------------------------------------------------------------------------------------------

struct fa_case {
    int64_t hs   = 64;   // head size (K and V)
    int64_t nh   = 4;    // heads
    int64_t n_q  = 1;    // query rows (decode = 1)
    int64_t kv   = 0;    // TOTAL kv columns = body + seg0 + seg1
    int64_t body = 0;    // rows served by src[1]/src[2]
    int64_t seg0 = 0;
    int64_t seg1 = 0;
    ggml_type body_type = GGML_TYPE_F16;
    ggml_type ring_type = GGML_TYPE_F16;
    // NEGATIVE CONTROL: feed the LAST segment rows that are shifted back by one, while leaving its
    // size and therefore its mask columns alone. That breaks the row<->mask-column correspondence -
    // exactly the class of bug leg 1 exists to catch - without changing any shape, so a build that
    // only checks shapes still looks healthy.
    //
    // (An earlier version of this control shifted the segment SIZES instead. That was not a control
    // at all: the segments stayed contiguous and the mask bases moved with them, so the poisoned
    // build was arithmetically identical to the clean one. It "fired" only because of the unrelated
    // accumulator difference below, which is how it was caught.)
    int64_t poison = 0;
};

// Source data, shared between the monolithic and the segmented build so the comparison is real.
struct fa_data {
    std::vector<float> q;     // [hs, n_q, nh]
    std::vector<float> k;     // [hs, kv, nh]
    std::vector<float> v;     // [hs, kv, nh]
    std::vector<float> mask;  // [kv, n_q]  (as float; converted to F16 on the way in)
};

static fa_data make_data(const fa_case & c) {
    fa_data d;
    d.q = rand_vec(c.hs * c.n_q * c.nh);
    d.k = rand_vec(c.hs * c.kv * c.nh);
    d.v = rand_vec(c.hs * c.kv * c.nh);
    // an all-visible mask with a few blocked columns, so the -INFINITY skip path is exercised too
    d.mask.assign(c.kv * c.n_q, 0.0f);
    for (int64_t r = 0; r < c.n_q; ++r) {
        for (int64_t j = 0; j < c.kv; j += 7) {
            d.mask[r * c.kv + j] = -INFINITY;
        }
    }
    return d;
}

// Fill a tensor from float data, quantising when the tensor's type asks for it.
static void set_tensor_from_f32(ggml_tensor * t, const float * src, int64_t n) {
    const ggml_type_traits * tr = ggml_get_type_traits(t->type);
    if (t->type == GGML_TYPE_F32) {
        ggml_backend_tensor_set(t, src, 0, n * sizeof(float));
        return;
    }
    std::vector<uint8_t> buf(ggml_nbytes(t));
    tr->from_float_ref(src, buf.data(), n);
    ggml_backend_tensor_set(t, buf.data(), 0, buf.size());
}

// Round-trip float data through a quantised type, so the reference sees the SAME values the kernel
// will see. Without this, leg 3 would be measuring q4_0's quantisation error, not our indexing.
static std::vector<float> quant_round_trip(ggml_type type, const float * src, int64_t n) {
    if (type == GGML_TYPE_F32) {
        return std::vector<float>(src, src + n);
    }
    const ggml_type_traits * tr = ggml_get_type_traits(type);
    std::vector<uint8_t> q(ggml_row_size(type, n));
    tr->from_float_ref(src, q.data(), n);
    std::vector<float> out(n);
    tr->to_float(q.data(), out.data(), n);
    return out;
}

// Run the case and return dst as floats. `segmented` picks which of the two builds to use.
static std::vector<float> run_case(const fa_case & c, const fa_data & d, bool segmented) {
    const int64_t body = segmented ? c.body : c.kv;
    const int64_t s0   = segmented ? c.seg0 : 0;
    const int64_t s1   = segmented ? c.seg1 : 0;

    ggml_init_params ip = { /*.mem_size =*/ ggml_tensor_overhead() * 64 + ggml_graph_overhead(),
                            /*.mem_buffer =*/ nullptr, /*.no_alloc =*/ true };
    ggml_context * ctx = ggml_init(ip);

    ggml_tensor * q = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, c.hs, c.n_q, c.nh);

    // Body and ring are SEPARATE tensors even in the monolithic build (where the ring rows are
    // simply zero-length), so the two builds differ in exactly one thing: where the rows live.
    ggml_tensor * k  = ggml_new_tensor_3d(ctx, c.body_type, c.hs, body, c.nh);
    ggml_tensor * v  = ggml_new_tensor_3d(ctx, c.body_type, c.hs, body, c.nh);
    ggml_tensor * k0 = s0 > 0 ? ggml_new_tensor_3d(ctx, c.ring_type, c.hs, s0, c.nh) : nullptr;
    ggml_tensor * v0 = s0 > 0 ? ggml_new_tensor_3d(ctx, c.ring_type, c.hs, s0, c.nh) : nullptr;
    ggml_tensor * k1 = s1 > 0 ? ggml_new_tensor_3d(ctx, c.ring_type, c.hs, s1, c.nh) : nullptr;
    ggml_tensor * v1 = s1 > 0 ? ggml_new_tensor_3d(ctx, c.ring_type, c.hs, s1, c.nh) : nullptr;

    ggml_tensor * m = ggml_new_tensor_4d(ctx, GGML_TYPE_F16, c.kv, c.n_q, 1, 1);

    ggml_tensor * out = ggml_flash_attn_ext(ctx, q, k, v, m, 1.0f / sqrtf((float) c.hs), 0.0f, 0.0f);
    ggml_flash_attn_ext_set_prec(out, GGML_PREC_F32);
    if (k0) {
        ggml_flash_attn_ext_add_segments(out, k0, v0, k1, v1);
    }

    ggml_backend_t backend = ggml_backend_cpu_init();
    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors(ctx, backend);

    // --- load, splitting the SAME source rows across whatever tensors this build has -------------
    set_tensor_from_f32(q, d.q.data(), c.hs * c.n_q * c.nh);

    // K/V are [hs, rows, nh]: row-major within a head, heads outermost. Copy per head so a split
    // lands on the right rows of the right head.
    auto load_kv = [&](ggml_tensor * t, const std::vector<float> & src, int64_t row0, int64_t nrow) {
        if (!t || nrow <= 0) {
            return;
        }
        std::vector<float> tmp((size_t)(c.hs * nrow * c.nh));
        for (int64_t h = 0; h < c.nh; ++h) {
            const float * s = src.data() + (h * c.kv + row0) * c.hs;
            memcpy(tmp.data() + h * nrow * c.hs, s, (size_t)(nrow * c.hs) * sizeof(float));
        }
        set_tensor_from_f32(t, tmp.data(), c.hs * nrow * c.nh);
    };

    load_kv(k,  d.k, 0,           body);
    load_kv(v,  d.v, 0,           body);
    load_kv(k0, d.k, body,        s0);
    load_kv(v0, d.v, body,        s0);
    // the poison shifts only WHICH rows segment 1 gets, never how many
    load_kv(k1, d.k, body + s0 - c.poison, s1);
    load_kv(v1, d.v, body + s0 - c.poison, s1);

    std::vector<ggml_fp16_t> mh(d.mask.size());
    ggml_fp32_to_fp16_row(d.mask.data(), mh.data(), (int64_t) d.mask.size());
    ggml_backend_tensor_set(m, mh.data(), 0, mh.size() * sizeof(ggml_fp16_t));

    ggml_cgraph * gf = ggml_new_graph(ctx);
    ggml_build_forward_expand(gf, out);
    ggml_backend_graph_compute(backend, gf);

    std::vector<float> res(ggml_nelements(out));
    ggml_backend_tensor_get(out, res.data(), 0, res.size() * sizeof(float));

    ggml_backend_buffer_free(buf);
    ggml_backend_free(backend);
    ggml_free(ctx);
    return res;
}

// The independent reference for leg 3: plain (non-online) softmax attention over rows that have
// been put through the SAME quantisation the tensors will hold.
static std::vector<float> reference(const fa_case & c, const fa_data & d) {
    // dst layout matches ggml_flash_attn_ext: permute(0,2,1,3) -> [hs, nh, n_q]
    std::vector<float> out((size_t)(c.hs * c.nh * c.n_q), 0.0f);

    for (int64_t h = 0; h < c.nh; ++h) {
        // rows [0, body) are body_type; the rest are ring_type
        std::vector<float> kq(c.hs * c.kv), vq(c.hs * c.kv);
        for (int64_t r = 0; r < c.kv; ++r) {
            const ggml_type t = (r < c.body) ? c.body_type : c.ring_type;
            auto kr = quant_round_trip(t, d.k.data() + (h * c.kv + r) * c.hs, c.hs);
            auto vr = quant_round_trip(t, d.v.data() + (h * c.kv + r) * c.hs, c.hs);
            memcpy(kq.data() + r * c.hs, kr.data(), c.hs * sizeof(float));
            memcpy(vq.data() + r * c.hs, vr.data(), c.hs * sizeof(float));
        }

        for (int64_t iq = 0; iq < c.n_q; ++iq) {
            const float * qp = d.q.data() + (h * c.n_q + iq) * c.hs;
            const float scale = 1.0f / sqrtf((float) c.hs);

            std::vector<float> s(c.kv);
            float mx = -INFINITY;
            for (int64_t r = 0; r < c.kv; ++r) {
                // the mask is stored F16 in the graph, so the reference must see the F16 value too
                const float mv = ggml_fp16_to_fp32(ggml_fp32_to_fp16(d.mask[iq * c.kv + r]));
                if (mv == -INFINITY) {
                    s[r] = -INFINITY;
                    continue;
                }
                float acc = 0.0f;
                for (int64_t e = 0; e < c.hs; ++e) {
                    acc += qp[e] * kq[r * c.hs + e];
                }
                s[r] = acc * scale + mv;
                mx = std::fmax(mx, s[r]);
            }

            float sum = 0.0f;
            for (int64_t r = 0; r < c.kv; ++r) {
                s[r] = (s[r] == -INFINITY) ? 0.0f : expf(s[r] - mx);
                sum += s[r];
            }

            float * o = out.data() + (iq * c.nh + h) * c.hs;
            for (int64_t r = 0; r < c.kv; ++r) {
                if (s[r] == 0.0f) {
                    continue;
                }
                for (int64_t e = 0; e < c.hs; ++e) {
                    o[e] += s[r] * vq[r * c.hs + e];
                }
            }
            const float inv = sum == 0.0f ? 0.0f : 1.0f / sum;
            for (int64_t e = 0; e < c.hs; ++e) {
                o[e] *= inv;
            }
        }
    }
    return out;
}

static double nmse(const std::vector<float> & a, const std::vector<float> & b) {
    if (a.size() != b.size()) {
        return INFINITY;
    }
    double num = 0.0, den = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        const double d = (double) a[i] - (double) b[i];
        num += d * d;
        den += (double) b[i] * (double) b[i];
    }
    return den == 0.0 ? num : num / den;
}

// ---------------------------------------------------------------------------------------------

static int g_fail = 0;

static void report(const char * leg, const char * what, double err, double tol, bool want_pass) {
    const bool passed = (err <= tol);
    const bool ok     = (passed == want_pass);
    printf("  %-6s %-46s nmse=%-14.6g tol=%-9.3g %s%s\n",
           leg, what, err, tol,
           passed ? "AGREE " : "DIFFER",
           ok ? "  OK" : "  <<<< UNEXPECTED");
    if (!ok) {
        g_fail++;
    }
}

int main() {
    printf("lane-195: flash-attention per-query source selection - CPU gate\n\n");

    // Boundaries chosen to be awkward on purpose: not multiples of any block size, a segment of 1,
    // and a case where the body is smaller than the ring.
    const struct { int64_t kv, body, seg0, seg1; } splits[] = {
        { 96, 64, 32,  0 },
        { 96, 64, 20, 12 },
        { 96,  1, 90,  5 },
        { 96, 31,  1, 64 },
        {130, 66, 33, 31 },
        {257, 129, 65, 63 },
    };

    // LEG 0 is the REGRESSION leg, and it is first on purpose. The segment support was added by
    // editing the hot loop of the shipped kernel: `acc_f16` replaced three inline `v->type ==
    // GGML_TYPE_F16` tests, the hoisted type traits were removed, and the k/v broadcast indices
    // moved to per-segment computation. Every one of those edits is on the path a real model takes
    // with an F16 V and NO segments — so that path is exercised here, against the same independent
    // reference LEG 3 uses. test-backend-ops cannot do this job: it skips the CPU backend it uses
    // as its own reference, so a CPU-only run of it is a 0-executed green.
    printf("LEG 0 - REGRESSION: n_seg == 0 with F16 K/V (the shipped path) vs the independent reference.\n");
    printf("        MUST AGREE: this is the path every model takes when the tail is off.\n");
    for (const auto & s : splits) {
        fa_case c;
        c.kv = s.kv; c.body = s.kv; c.seg0 = 0; c.seg1 = 0;   // no segments at all
        c.body_type = GGML_TYPE_F16; c.ring_type = GGML_TYPE_F16;
        fa_data d = make_data(c);
        char name[128];
        snprintf(name, sizeof(name), "f16 monolithic kv=%lld (no segments)", (long long) s.kv);
        // F16 K/V and an F16 accumulator: tolerance is set by half precision, not by this lane
        report("LEG0", name, nmse(run_case(c, d, false), reference(c, d)), 1e-4, true);
    }

    printf("\n");

    // LEG 1/2 run in F32. That is not cosmetic: with an F16 V and no segments the CPU kernel keeps
    // an F16 accumulator, while any segmented node is forced to F32 (a mixed-dtype window cannot
    // carry an accumulator whose type flips mid-row). Comparing those two arms measures the
    // ACCUMULATOR, not the indexing - it lands at ~2e-6 whatever the segments do, which is loud
    // enough to bury a real defect and did exactly that on the first run of this file. F32 on both
    // sides removes the confound, so the only remaining difference is where the rows live.
    printf("LEG 1 - split-equivalence (F32 body + F32 ring vs one monolithic F32 tensor)\n");
    printf("        MUST AGREE: same rows, same values, only the tensor boundary moves.\n");
    for (const auto & s : splits) {
        fa_case c;
        c.kv = s.kv; c.body = s.body; c.seg0 = s.seg0; c.seg1 = s.seg1;
        c.body_type = GGML_TYPE_F32; c.ring_type = GGML_TYPE_F32;
        fa_data d = make_data(c);
        char name[128];
        snprintf(name, sizeof(name), "kv=%lld body=%lld seg0=%lld seg1=%lld",
                 (long long) s.kv, (long long) s.body, (long long) s.seg0, (long long) s.seg1);
        report("LEG1", name, nmse(run_case(c, d, true), run_case(c, d, false)), 1e-8, true);
    }

    printf("\nLEG 2 - NEGATIVE CONTROL: segment 1 is fed rows shifted back by one, same sizes.\n");
    printf("        MUST DIFFER. If any of these agrees, LEG 1 was not looking at anything.\n");
    for (const auto & s : splits) {
        if (s.seg1 < 1) {
            continue;  // no second segment to poison
        }
        fa_case c;
        c.kv = s.kv; c.body = s.body; c.seg0 = s.seg0; c.seg1 = s.seg1;
        c.body_type = GGML_TYPE_F32; c.ring_type = GGML_TYPE_F32;
        c.poison = 1;
        fa_data d = make_data(c);
        char name[128];
        snprintf(name, sizeof(name), "kv=%lld body=%lld seg0=%lld seg1=%lld rows-1",
                 (long long) s.kv, (long long) s.body, (long long) s.seg0, (long long) s.seg1);
        report("LEG2", name, nmse(run_case(c, d, true), run_case(c, d, false)), 1e-8, false);
    }

    printf("\nLEG 3 - mixed dtype: q4_0 body + F16 ring vs an INDEPENDENT plain-softmax reference.\n");
    printf("        MUST AGREE: each segment must be read at its OWN dtype.\n");
    for (const auto & s : splits) {
        fa_case c;
        c.kv = s.kv; c.body = s.body; c.seg0 = s.seg0; c.seg1 = s.seg1;
        c.hs = 64;  // a multiple of the q4_0 block
        c.body_type = GGML_TYPE_Q4_0;
        c.ring_type = GGML_TYPE_F16;
        if (c.body % 1 != 0 || c.hs % ggml_blck_size(GGML_TYPE_Q4_0) != 0) {
            continue;
        }
        fa_data d = make_data(c);
        char name[128];
        snprintf(name, sizeof(name), "q4_0 body=%lld + f16 ring=%lld/%lld",
                 (long long) s.body, (long long) s.seg0, (long long) s.seg1);
        // tolerance is set by q4_0's own grid, not by the segment machinery
        report("LEG3", name, nmse(run_case(c, d, true), reference(c, d)), 5e-4, true);
    }

    printf("\n%s (%d unexpected)\n", g_fail == 0 ? "ALL LEGS AS EXPECTED" : "FAILURES", g_fail);
    return g_fail == 0 ? 0 : 1;
}
