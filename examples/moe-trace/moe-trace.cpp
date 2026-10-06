// moe-trace: greedy-decode prompts and write the per-token, per-layer routed expert ids
// (ffn_moe_topk-<il>) of every DECODE step to MOE_TRACE_OUT. Prompts: -f file, separated by lines "@@@".
// Output lines: "<prompt> <step> <layer> <e0> ... <ek-1>". Generated text goes to MOE_TRACE_OUT.txt.
#include "arg.h"
#include "common.h"
#include "log.h"
#include "llama.h"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct trace_state {
    FILE * f = nullptr;
    int prompt = 0;
    int step = 0;
    long long rows = 0;
};

static bool trace_cb(struct ggml_tensor * t, bool ask, void * ud) {
    auto * st = (trace_state *) ud;
    const bool match = strncmp(t->name, "ffn_moe_topk-", 13) == 0 && t->type == GGML_TYPE_I32;
    if (ask) {
        return match && t->ne[1] == 1;
    }
    if (!match || t->ne[1] != 1) {
        return true;
    }
    const int k = (int) t->ne[0];
    std::vector<int32_t> ids(k);
    ggml_backend_tensor_get(t, ids.data(), 0, k * sizeof(int32_t));
    fprintf(st->f, "%d %d %d", st->prompt, st->step, atoi(t->name + 13));
    for (int i = 0; i < k; i++) {
        fprintf(st->f, " %d", ids[i]);
    }
    fputc('\n', st->f);
    st->rows++;
    return true;
}

int main(int argc, char ** argv) {
    std::setlocale(LC_NUMERIC, "C");
    common_params params;
    common_init();
    if (!common_params_parse(argc, argv, params, LLAMA_EXAMPLE_COMMON)) {
        return 1;
    }
    const char * out = getenv("MOE_TRACE_OUT");
    if (!out) {
        LOG_ERR("set MOE_TRACE_OUT\n");
        return 1;
    }
    trace_state st;
    st.f = fopen(out, "w");
    FILE * ftxt = fopen((std::string(out) + ".txt").c_str(), "w");
    if (!st.f || !ftxt) {
        LOG_ERR("cannot open %s\n", out);
        return 1;
    }

    llama_backend_init();
    params.cb_eval = trace_cb;
    params.cb_eval_user_data = &st;
    params.warmup = false;

    auto llama_init = common_init_from_params(params);
    llama_model * model = llama_init->model();
    llama_context * ctx = llama_init->context();
    if (!model || !ctx) {
        LOG_ERR("failed to init\n");
        return 1;
    }
    const llama_vocab * vocab = llama_model_get_vocab(model);

    std::vector<std::string> prompts;
    {
        std::string cur, all = params.prompt;
        size_t pos = 0;
        while (pos <= all.size()) {
            size_t nl = all.find('\n', pos);
            std::string line = all.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            if (line == "@@@") {
                prompts.push_back(cur);
                cur.clear();
            } else {
                cur += line + "\n";
            }
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
        if (!cur.empty()) prompts.push_back(cur);
    }

    llama_sampler * smpl = llama_sampler_init_greedy();
    const int n_predict = params.n_predict > 0 ? params.n_predict : 512;

    for (size_t p = 0; p < prompts.size(); p++) {
        llama_memory_clear(llama_get_memory(ctx), true);
        st.prompt = (int) p;
        st.step = -1;
        std::vector<llama_token> toks = common_tokenize(ctx, prompts[p], true, true);
        if (llama_decode(ctx, llama_batch_get_one(toks.data(), toks.size()))) {
            LOG_ERR("prompt %zu: decode failed\n", p);
            return 1;
        }
        fprintf(ftxt, "=== prompt %zu (%zu tokens)\n", p, toks.size());
        for (int i = 0; i < n_predict; i++) {
            llama_token tok = llama_sampler_sample(smpl, ctx, -1);
            if (llama_vocab_is_eog(vocab, tok)) break;
            fputs(common_token_to_piece(ctx, tok).c_str(), ftxt);
            st.step = i;
            if (llama_decode(ctx, llama_batch_get_one(&tok, 1))) {
                LOG_ERR("prompt %zu step %d: decode failed\n", p, i);
                return 1;
            }
        }
        fputs("\n", ftxt);
        fflush(st.f);
        fflush(ftxt);
        LOG_INF("prompt %zu done, trace rows so far %lld\n", p, st.rows);
    }
    llama_sampler_free(smpl);
    fclose(st.f);
    fclose(ftxt);
    LOG_INF("moe-trace rows=%lld\n", st.rows);
    llama_backend_free();
    return 0;
}
