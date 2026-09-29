// lane-296 int8 op capture: one real forward pass (first 512 tokens of a text file), dumps the f32 input (src1) of
// every MUL_MAT whose weight name matches blk.<L>.<role>.weight for a role in PROBE_ROLES (comma list).
// output <dir>/<weight-name>.x : int32 K, int32 N, f32 data (column n = token n). First ubatch only.
#include "ggml.h"
#include "ggml-backend.h"
#include "llama.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

struct cap { std::string dir; std::set<std::string> roles, done; };

static std::string role_of(const std::string & n) {
    if (n.rfind("blk.", 0) != 0) return "";
    const size_t a = n.find('.', 4), w = n.rfind(".weight");
    return (a == std::string::npos || w == std::string::npos || w <= a) ? "" : n.substr(a + 1, w - a - 1);
}

static bool cb(ggml_tensor * t, bool ask, void * ud) {
    cap * c = (cap *) ud;
    if (t->op != GGML_OP_MUL_MAT || !t->src[0] || !t->src[1]) return !ask ? true : false;
    const std::string wn = t->src[0]->name;
    if (!c->roles.count(role_of(wn)) || c->done.count(wn)) return !ask ? true : false;
    if (ask) return true;
    const ggml_tensor * x = t->src[1];
    if (x->type != GGML_TYPE_F32 || x->ne[3] != 1) { fprintf(stderr, "skip %s: src1 type/shape\n", wn.c_str()); return true; }
    std::vector<uint8_t> raw(ggml_nbytes(x));
    ggml_backend_tensor_get(x, raw.data(), 0, raw.size());
    const int32_t K = (int32_t) x->ne[0], N = (int32_t) x->ne[1];
    std::vector<float> out((size_t) K * N * x->ne[2]);
    for (int s = 0; s < x->ne[2]; s++) for (int n = 0; n < N; n++) for (int k = 0; k < K; k++)
        out[((size_t) s * N + n) * K + k] = *(const float *) (raw.data() + s * x->nb[2] + n * x->nb[1] + k * x->nb[0]);
    const int32_t Nt = (int32_t) (x->ne[1] * x->ne[2]);
    FILE * f = fopen((c->dir + "/" + wn + ".x").c_str(), "wb");
    fwrite(&K, 4, 1, f); fwrite(&Nt, 4, 1, f); fwrite(out.data(), 4, out.size(), f); fclose(f);
    // the matmul's own in-graph output (engagement witness), same layout: int32 M, int32 N, f32 data
    if (t->type == GGML_TYPE_F32 && ggml_is_contiguous(t)) {
        std::vector<float> y(ggml_nelements(t));
        ggml_backend_tensor_get(t, y.data(), 0, ggml_nbytes(t));
        const int32_t M = (int32_t) t->ne[0];
        FILE * fy = fopen((c->dir + "/" + wn + ".y").c_str(), "wb");
        fwrite(&M, 4, 1, fy); fwrite(&Nt, 4, 1, fy); fwrite(y.data(), 4, y.size(), fy); fclose(fy);
    }
    fprintf(stderr, "captured %s K=%d N=%d ne2=%d contiguous=%d\n", wn.c_str(), K, N, (int) x->ne[2], (int) ggml_is_contiguous(x));
    c->done.insert(wn);
    return true;
}

int main(int argc, char ** argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s <model> <text> <outdir>\n", argv[0]); return 1; }
    cap c; c.dir = argv[3];
    std::stringstream rs(std::getenv("PROBE_ROLES") ? std::getenv("PROBE_ROLES") : "ssm_out,attn_output,ssm_alpha");
    for (std::string r; std::getline(rs, r, ',');) c.roles.insert(r);
    std::ifstream tf(argv[2], std::ios::binary); std::stringstream ss; ss << tf.rdbuf(); std::string text = ss.str();
    llama_backend_init();
    llama_model_params mp = llama_model_default_params(); mp.n_gpu_layers = 999;
    llama_model * model = llama_model_load_from_file(argv[1], mp);
    if (!model) return 1;
    const llama_vocab * vocab = llama_model_get_vocab(model);
    std::vector<llama_token> tok(text.size() + 16);
    int nt = llama_tokenize(vocab, text.c_str(), (int32_t) text.size(), tok.data(), (int32_t) tok.size(), true, false);
    // PROBE_NSEQ=s: s sequences of 512 tokens in one batch (the llama-perplexity --kl-divergence shape, n_seq=4)
    const int ns = std::getenv("PROBE_NSEQ") ? atoi(std::getenv("PROBE_NSEQ")) : 1;
    if (nt < 512 * ns) return 1;
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = 512 * ns; cp.n_batch = 512 * ns; cp.n_ubatch = 512; cp.n_seq_max = ns;
    cp.cb_eval = cb; cp.cb_eval_user_data = &c;
    llama_context * ctx = llama_init_from_model(model, cp);
    if (!ctx) return 1;
    llama_batch b = llama_batch_init(512 * ns, 0, 1);
    for (int s = 0; s < ns; s++) for (int i = 0; i < 512; i++) {
        const int j = s * 512 + i;
        b.token[j] = tok[j]; b.pos[j] = i; b.n_seq_id[j] = 1; b.seq_id[j][0] = s; b.logits[j] = i == 511;
    }
    b.n_tokens = 512 * ns;
    const int rc = llama_decode(ctx, b);
    llama_batch_free(b);
    fprintf(stderr, "decode rc=%d captured=%zu\n", rc, c.done.size());
    llama_free(ctx); llama_model_free(model); llama_backend_free();
    return rc;
}
