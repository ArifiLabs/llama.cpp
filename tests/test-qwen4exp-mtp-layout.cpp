// qwen4exp MTP: an upstream-layout GGUF (#29761: blk.N.nextn.hc_head_* mixer, QSA indexer tensors in the MTP block,
// optional MTP-only file without the trunk mixer or PLE) must draft the same ids as our sidecar of the same weights.
// Files: tests/gen-qwen4exp-mtp-layout.py <qwen4exp-moe.gguf from test-llama-archs -o> <dir>.
// ours-wrongmix (sidecar carrying the TRUNK mixer) is the control: its logits must differ, or the test cannot see
// a graph that picks the wrong mixer.
// usage: test-qwen4exp-mtp-layout <dir> [n_gpu_layers]

#include "llama.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static const int N_TOK = 4;

// one MTP pass over N_TOK (token, h) pairs; empty on load/decode failure
static std::vector<float> run(const std::string & path, int ngl, int & n_vocab) {
    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = ngl;
    mparams.load_mtp     = true;
    llama_model * model = llama_model_load_from_file(path.c_str(), mparams);
    if (!model) {
        fprintf(stderr, "  load FAILED: %s\n", path.c_str());
        return {};
    }
    n_vocab = llama_vocab_n_tokens(llama_model_get_vocab(model));
    const int n_h = llama_model_n_embd_out(model);

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx    = 64;
    cparams.n_batch  = 64;
    cparams.n_ubatch = 64;
    cparams.ctx_type = LLAMA_CONTEXT_TYPE_MTP;
    llama_context * ctx = llama_init_from_model(model, cparams);
    if (!ctx) {
        fprintf(stderr, "  context FAILED: %s\n", path.c_str());
        llama_model_free(model);
        return {};
    }

    llama_batch batch = llama_batch_init(N_TOK, n_h, 1);
    batch.token = (llama_token *) malloc(sizeof(llama_token) * N_TOK); // MTP reads token AND h (speculative.cpp)
    std::mt19937 rng(29761);
    std::normal_distribution<float> nd(0.0f, 1.0f);
    for (int i = 0; i < N_TOK; ++i) {
        batch.token[i]     = 1 + 3 * i;
        batch.pos[i]       = i;
        batch.n_seq_id[i]  = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i]    = 1;
        for (int j = 0; j < n_h; ++j) {
            batch.embd[(size_t) i * n_h + j] = nd(rng);
        }
    }
    batch.n_tokens = N_TOK;

    std::vector<float> out;
    if (llama_decode(ctx, batch) == 0) {
        for (int i = 0; i < N_TOK; ++i) {
            const float * l = llama_get_logits_ith(ctx, i);
            out.insert(out.end(), l, l + n_vocab);
        }
    } else {
        fprintf(stderr, "  decode FAILED: %s\n", path.c_str());
    }

    llama_batch_free(batch); // frees token too
    llama_free(ctx);
    llama_model_free(model);
    return out;
}

static std::vector<int> argmax_rows(const std::vector<float> & l, int n_vocab) {
    std::vector<int> ids;
    for (int i = 0; i < N_TOK; ++i) {
        int best = 0;
        for (int t = 1; t < n_vocab; ++t) {
            if (l[(size_t) i * n_vocab + t] > l[(size_t) i * n_vocab + best]) {
                best = t;
            }
        }
        ids.push_back(best);
    }
    return ids;
}

static float max_abs_diff(const std::vector<float> & a, const std::vector<float> & b) {
    float d = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        d = std::fmax(d, std::fabs(a[i] - b[i]));
    }
    return d;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <dir with gen-qwen4exp-mtp-layout.py output> [n_gpu_layers]\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    const int ngl = argc > 2 ? atoi(argv[2]) : 0;

    llama_backend_init();

    int nv_ref = 0;
    const std::vector<float> ref = run(dir + "/ours-sidecar.gguf", ngl, nv_ref);
    if (ref.empty()) {
        fprintf(stderr, "FAIL: reference sidecar did not run (test setup)\n");
        return 1;
    }
    const std::vector<int> ids_ref = argmax_rows(ref, nv_ref);

    int n_fail = 0;

    int nv = 0;
    const std::vector<float> wrong = run(dir + "/ours-wrongmix.gguf", ngl, nv);
    const float d_wrong = wrong.size() == ref.size() ? max_abs_diff(ref, wrong) : -1.0f;
    if (d_wrong < 1e-3f) {
        fprintf(stderr, "FAIL: control ours-wrongmix max|d|=%g - the mixer does not reach the logits (test setup)\n", d_wrong);
        n_fail++;
    } else {
        fprintf(stderr, "OK control ours-wrongmix differs, max|d|=%g\n", d_wrong);
    }

    for (const char * f : { "up-full.gguf", "up-mtponly.gguf" }) {
        const std::vector<float> got = run(dir + "/" + f, ngl, nv);
        if (got.size() != ref.size()) {
            fprintf(stderr, "FAIL: %s did not load or decode\n", f);
            n_fail++;
            continue;
        }
        const std::vector<int> ids = argmax_rows(got, nv);
        const float d = max_abs_diff(ref, got);
        const bool same = ids == ids_ref;
        fprintf(stderr, "%s: %s ids [%d %d %d %d] vs sidecar [%d %d %d %d], max|d|=%g\n", same && d < 1e-4f ? "OK" : "FAIL", f,
                ids[0], ids[1], ids[2], ids[3], ids_ref[0], ids_ref[1], ids_ref[2], ids_ref[3], d);
        if (!same || d >= 1e-4f) {
            n_fail++;
        }
    }

    llama_backend_free();
    fprintf(stderr, "%s\n", n_fail ? "RESULT FAIL" : "RESULT PASS");
    return n_fail ? 1 : 0;
}
