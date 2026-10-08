// Draft acceptance must stop at an EOG token (#29638). Model: any small GGUF, e.g. one written by
// `test-llama-archs -a "^llama$" -o <dir>` (llama-dense.gguf). Token EOG_ID is made the EOS by a kv override and is
// forced by a logit bias, so the target samples EOG at every position. Draft = [EOG, OTHER]:
// before the fix the verifier accepted EOG, kept going and returned 2 tokens; after it returns only [EOG].

#include "llama.h"
#include "common.h"
#include "sampling.h"
#include "speculative.h"

#include <cstdio>
#include <cstring>
#include <vector>

static const llama_token EOG_ID   = 5;
static const llama_token OTHER_ID = 7;

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <model.gguf>\n", argv[0]);
        return 2;
    }

    llama_backend_init();

    llama_model_kv_override kvo[2] = {};
    snprintf(kvo[0].key, sizeof(kvo[0].key), "tokenizer.ggml.eos_token_id");
    kvo[0].tag     = LLAMA_KV_OVERRIDE_TYPE_INT;
    kvo[0].val_i64 = EOG_ID;

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;
    mparams.kv_overrides = kvo;
    llama_model * model = llama_model_load_from_file(argv[1], mparams);
    if (!model) {
        fprintf(stderr, "FAIL: model load\n");
        return 1;
    }
    const llama_vocab * vocab = llama_model_get_vocab(model);
    if (!llama_vocab_is_eog(vocab, EOG_ID)) {
        fprintf(stderr, "FAIL: token %d is not EOG after the override (test setup)\n", EOG_ID);
        return 1;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = 64;
    cparams.n_batch = 64;
    cparams.n_ubatch = 64;

    int n_fail = 0;
    for (int with_dists = 0; with_dists < 2; ++with_dists) {
        llama_context * ctx = llama_init_from_model(model, cparams);

        llama_batch batch = llama_batch_init(8, 0, 1);
        const llama_token toks[3] = { 1, EOG_ID, OTHER_ID };
        for (int i = 0; i < 3; ++i) {
            common_batch_add(batch, toks[i], i, { 0 }, true);
        }
        if (llama_decode(ctx, batch) != 0) {
            fprintf(stderr, "FAIL: decode\n");
            return 1;
        }

        common_params_sampling sparams;
        sparams.top_k = 1;
        sparams.logit_bias.push_back({ EOG_ID, 1e4f });
        common_sampler * smpl = common_sampler_init(model, sparams);

        const llama_tokens draft = { EOG_ID, OTHER_ID };
        const std::vector<int> idxs = { 0, 1, 2 };
        std::vector<llama_token> acc;
        if (with_dists) {
            std::vector<common_speculative_token_dist> dists(2);
            dists[0].ids = { EOG_ID };   dists[0].probs = { 1.0f };
            dists[1].ids = { OTHER_ID }; dists[1].probs = { 1.0f };
            acc = common_sampler_sample_and_accept_n(smpl, ctx, idxs, draft, dists);
        } else {
            acc = common_sampler_sample_and_accept_n(smpl, ctx, idxs, draft);
        }

        const bool ok = acc.size() == 1 && acc[0] == EOG_ID;
        printf("%s: overload=%s accepted=%zu first=%d\n", ok ? "OK" : "FAIL",
               with_dists ? "dists" : "greedy", acc.size(), acc.empty() ? -1 : acc[0]);
        n_fail += ok ? 0 : 1;

        common_sampler_free(smpl);
        llama_batch_free(batch);
        llama_free(ctx);
    }

    llama_model_free(model);
    llama_backend_free();
    return n_fail == 0 ? 0 : 1;
}
