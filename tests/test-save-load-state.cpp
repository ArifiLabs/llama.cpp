#include "arg.h"
#include "common.h"
#include "log.h"
#include "llama-cpp.h"
#include "server-ckpt-storage.h" // R45: Test 10 allocates ids through the production allocator

#include <algorithm>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <list>
#include <random>
#include <string>
#include <vector>

constexpr double NMSE_THRESHOLD = 1e-5;

enum class test_status {
    PASS,
    FAIL,
    SKIP,
};

static const char * test_status_str(test_status status) {
    switch (status) {
        case test_status::PASS: return "\033[1;32mPASS\033[0m";
        case test_status::FAIL: return "\033[1;31mFAIL\033[0m";
        case test_status::SKIP: return "\033[1;33mSKIP\033[0m";
    }
    return "";
}

// normalized mean squared error = mse(a, b) / mse(a, 0)
static double nmse(const std::vector<float> & a, const std::vector<float> & b) {
    GGML_ASSERT(a.size() == b.size());
    double mse_a_b = 0.0;
    double mse_a_0 = 0.0;

    for (size_t i = 0; i < a.size(); i++) {
        const float a_i = a[i];
        const float b_i = b[i];

        mse_a_b += (double) (a_i - b_i) * (a_i - b_i);
        mse_a_0 += (double) a_i * a_i;
    }

    return mse_a_b / mse_a_0;
}

struct generation_result {
    llama_tokens tokens;
    std::vector<std::vector<float>> logits;

    bool empty() const { return tokens.empty(); }
};

static bool get_current_logits(llama_context * ctx, std::vector<float> & out) {
    const auto * vocab = llama_model_get_vocab(llama_get_model(ctx));
    const int32_t n_vocab = llama_vocab_n_tokens(vocab);
    const float * logits = llama_get_logits_ith(ctx, -1);
    if (logits == nullptr) {
        return false;
    }
    out.assign(logits, logits + n_vocab);
    return true;
}

struct llama_batch_ptr {
    llama_batch batch;

    llama_batch_ptr(int32_t n_tokens, int32_t embd, int32_t n_seq_max)
        : batch{llama_batch_init(n_tokens, embd, n_seq_max)} {}

    ~llama_batch_ptr() { llama_batch_free(batch); }

    llama_batch_ptr(const llama_batch_ptr &) = delete;
    llama_batch_ptr & operator=(const llama_batch_ptr &) = delete;
    llama_batch_ptr(llama_batch_ptr &&) = default;
    llama_batch_ptr & operator=(llama_batch_ptr &&) = default;

    llama_batch & get() { return batch; }
    const llama_batch & get() const { return batch; }
};

static generation_result generate_tokens(llama_context * ctx, llama_sampler * smpl, int & n_past, int32_t n_predict, llama_seq_id seq_id) {
    generation_result result;
    llama_batch_ptr batch(1, 0, 1);

    for (int i = 0; i < n_predict; i++) {
        std::vector<float> logits;
        if (!get_current_logits(ctx, logits)) {
            LOG_ERR("\n%s: failed to get logits\n", __func__);
            return {};
        }

        auto next_token = llama_sampler_sample(smpl, ctx, -1);

        // LOG_LEVEL_INFO gate: visible in single-model mode, silenced in --models table mode
        LOGV(LOG_LEVEL_INFO, "%d ", next_token);
        result.tokens.push_back(next_token);
        result.logits.push_back(std::move(logits));

        common_batch_clear(batch.get());
        common_batch_add(batch.get(), next_token, n_past, {seq_id}, true);

        if (llama_decode(ctx, batch.get())) {
            LOG_ERR("\n%s: failed to evaluate\n", __func__);
            return {};
        }
        n_past++;
    }

    return result;
}

static bool generate_tokens_compare(
        llama_context * ctx, llama_sampler * smpl, int & n_past, int32_t n_predict, llama_seq_id seq_id,
        const generation_result & expected) {
    if (expected.tokens.size() != expected.logits.size() || expected.tokens.size() < (size_t) n_predict) {
        LOG_ERR("\n%s: invalid expected generation\n", __func__);
        return false;
    }

    llama_batch_ptr batch(1, 0, 1);

    for (int i = 0; i < n_predict; i++) {
        std::vector<float> logits;
        if (!get_current_logits(ctx, logits)) {
            LOG_ERR("\n%s: failed to get logits\n", __func__);
            return false;
        }
        if (logits.size() != expected.logits[i].size()) {
            LOG_ERR("\n%s: logits size mismatch at step %d: %zu != %zu\n", __func__, i, logits.size(), expected.logits[i].size());
            return false;
        }

        const double nmse_val = nmse(expected.logits[i], logits);
        LOG_TRC("%s: step %d nmse = %.6e\n", __func__, i, nmse_val);
        if (nmse_val > NMSE_THRESHOLD) {
            LOG_ERR("\n%s: error: NMSE at step %d is %.6e (threshold %.1e)\n", __func__, i, nmse_val, NMSE_THRESHOLD);
            return false;
        }

        const auto next_token = llama_sampler_sample(smpl, ctx, -1);
        const auto expected_token = expected.tokens[i];

        LOGV(LOG_LEVEL_INFO, "%d ", next_token);
        if (next_token != expected_token) {
            LOG_TRC("%s: sampled token %d differs from expected %d, using expected token\n", __func__, next_token, expected_token);
        }

        common_batch_clear(batch.get());
        common_batch_add(batch.get(), expected_token, n_past, {seq_id}, true);

        if (llama_decode(ctx, batch.get())) {
            LOG_ERR("\n%s: failed to evaluate\n", __func__);
            return false;
        }
        n_past++;
    }

    return true;
}

// Test 1: baseline
// - decode all but the last token
// - save state to disk
// - decode the last token
// - generate n_predict tokens
static generation_result test_baseline(struct llama_model * model, const struct common_params & params, const llama_tokens & tokens) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_seq_max = 2;
    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};

    auto sparams = llama_sampler_chain_default_params();
    auto smpl = llama_sampler_ptr{llama_sampler_chain_init(sparams)};
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_dist(params.sampling.seed));

    auto n_past = 0;
    if (!common_prompt_batch_decode(ctx.get(), tokens, (int)tokens.size(), n_past, params.n_batch, params.out_file, true)) {
        LOG_ERR("%s: failed to decode prompt\n", __func__);
        return {};
    }

    LOGV(LOG_LEVEL_INFO, "\n=== Test 1: baseline ===\n");

    auto result = generate_tokens(ctx.get(), smpl.get(), n_past, params.n_predict, 0);
    if (result.empty()) {
        return {};
    }

    LOGV(LOG_LEVEL_INFO, "\n");

    return result;
}


// Test 2: sequence removal isolation
// - decode the same prefix into two sequences
// - remove sequence 0
// - verify that sequence 1 remains unchanged
static bool test_seq_rm_isolated(
        struct llama_model         * model,
        const struct common_params & params,
        const llama_tokens         & tokens) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_ctx      = 256;
    params_ctx.n_seq_max  = 2;
    params_ctx.kv_unified = true;

    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};
    if (!ctx) {
        LOG_ERR("%s: failed to create context\n", __func__);
        return false;
    }

    LOGV(LOG_LEVEL_INFO, "\n=== Test 2: sequence removal isolation ===\n");

    const size_t n_tokens = tokens.size() < 128 ? tokens.size() : 128;
    for (llama_seq_id seq_id = 0; seq_id < 2; ++seq_id) {
        llama_batch_ptr batch(n_tokens, 0, 1);
        for (size_t i = 0; i < n_tokens; ++i) {
            common_batch_add(batch.get(), tokens[i], i, { seq_id }, i == n_tokens - 1);
        }

        if (llama_decode(ctx.get(), batch.get())) {
            LOG_ERR("%s: failed to decode prompt for sequence %d\n", __func__, seq_id);
            return false;
        }
    }

    const auto get_seq_state = [&](llama_seq_id seq_id, std::vector<uint8_t> & state) {
        const size_t state_size = llama_state_seq_get_size(ctx.get(), seq_id);
        if (state_size == 0) {
            LOG_ERR("%s: sequence state is empty\n", __func__);
            return false;
        }

        state.resize(state_size);
        const size_t ncopy = llama_state_seq_get_data(ctx.get(), state.data(), state.size(), seq_id);
        if (ncopy != state.size()) {
            LOG_ERR("%s: sequence state length %zu does not match expected length %zu\n",
                    __func__, ncopy, state.size());
            return false;
        }

        return true;
    };

    std::vector<uint8_t> state_before;
    if (!get_seq_state(1, state_before)) {
        return false;
    }

    if (!llama_memory_seq_rm(llama_get_memory(ctx.get()), 0, -1, -1)) {
        LOG_ERR("%s: failed to remove sequence 0\n", __func__);
        return false;
    }

    std::vector<uint8_t> state_after;
    if (!get_seq_state(1, state_after)) {
        return false;
    }

    if (state_before != state_after) {
        LOG_ERR("%s: removing sequence 0 changed sequence 1\n", __func__);
        return false;
    }

    LOGV(LOG_LEVEL_INFO, "PASS\n");
    return true;
}


// Test 3: state load
// - create a new context
// - load state from file
// - replay the last prompt token
// - generate n_predict tokens and compare against expected result
static bool test_state_load(struct llama_model * model, const struct common_params & params, const llama_tokens & tokens, const generation_result & expected_result) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_seq_max = 2;
    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};

    auto sparams = llama_sampler_chain_default_params();
    auto smpl = llama_sampler_ptr{llama_sampler_chain_init(sparams)};
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_dist(params.sampling.seed));

    LOGV(LOG_LEVEL_INFO, "\n=== Test 3: state load ===\n");

    // Load state from file
    llama_tokens unused_sts(tokens.size());
    size_t n_token_count_out = 0;

    if (!llama_state_load_file(ctx.get(), params.out_file.data(), unused_sts.data(), unused_sts.size(), &n_token_count_out)) {
        LOG_ERR("\n%s: failed to load state\n", __func__);
        return false;
    }

    LOG_TRC("%s: loaded state with %zu tokens\n", __func__, n_token_count_out);

    // Replay last token
    int n_past = (int) n_token_count_out - 1;
    if (!common_replay_last_token(ctx.get(), tokens.back(), n_past)) {
        return false;
    }
    n_past++;

    // Generate tokens and compare logits against the baseline
    if (!generate_tokens_compare(ctx.get(), smpl.get(), n_past, params.n_predict, 0, expected_result)) {
        return false;
    }

    LOGV(LOG_LEVEL_INFO, "\nPASS\n");
    return true;
}


// Test 4: seq copy (host)
// - create a multi-seq context
// - load state from file
// - replay the last prompt token
// - migrate KV cache from seq 0 to seq 1 via the CPU path
// - generate n_predict tokens on seq 1 and compare against expected result
static bool test_seq_cp_host(struct llama_model * model, const struct common_params & params, const llama_tokens & tokens, const generation_result & expected_result) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_seq_max = 2;
    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};

    auto sparams = llama_sampler_chain_default_params();
    auto smpl = llama_sampler_ptr{llama_sampler_chain_init(sparams)};
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_dist(params.sampling.seed));

    LOGV(LOG_LEVEL_INFO, "\n=== Test 4: seq copy (host) ===\n");

    // Load state from file
    llama_tokens unused_sts(tokens.size());
    size_t n_token_count_out = 0;

    if (!llama_state_load_file(ctx.get(), params.out_file.data(), unused_sts.data(), unused_sts.size(), &n_token_count_out)) {
        LOG_ERR("\n%s: failed to load state\n", __func__);
        return false;
    }

    LOG_TRC("%s: loaded state with %zu tokens\n", __func__, n_token_count_out);

    // Replay last token
    int n_past = (int) n_token_count_out - 1;
    if (!common_replay_last_token(ctx.get(), tokens.back(), n_past)) {
        return false;
    }
    n_past++;

    // Migrate KV cache from seq 0 to seq 1 (CPU path)
    {
        std::vector<uint8_t> seq_store(llama_state_seq_get_size(ctx.get(), 0));
        const size_t ncopy = llama_state_seq_get_data(ctx.get(), seq_store.data(), seq_store.size(), 0);
        if (ncopy != seq_store.size()) {
            LOG_ERR("\n%s: seq copy data length %zd does not match expected length %zd\n", __func__, ncopy, seq_store.size());
            return false;
        }
        LOG_TRC("%s: seq 0 copied, %zd bytes\n", __func__, ncopy);

        llama_memory_clear(llama_get_memory(ctx.get()), true);
        LOG_TRC("%s: kv cache cleared\n", __func__);

        const size_t nset = llama_state_seq_set_data(ctx.get(), seq_store.data(), seq_store.size(), 1);
        if (nset != seq_store.size()) {
            LOG_ERR("\n%s: seq set data length %zd does not match expected length %zd\n", __func__, nset, seq_store.size());
            return false;
        }
        LOG_TRC("%s: seq 1 restored, %zd bytes\n", __func__, nset);
    }

    // Generate tokens and compare logits against the baseline
    if (!generate_tokens_compare(ctx.get(), smpl.get(), n_past, params.n_predict, 1, expected_result)) {
        return false;
    }

    LOGV(LOG_LEVEL_INFO, "\nPASS\n");
    return true;
}


// Test 5: seq copy (device)
// - create a multi-seq context
// - load state from file
// - replay the last prompt token
// - migrate KV cache from seq 0 to seq 1 via the on-device path
// - generate n_predict tokens on seq 1 and compare against expected result
static bool test_seq_cp_device(struct llama_model * model, const struct common_params & params, const llama_tokens & tokens, const generation_result & expected_result) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_seq_max = 2;
    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};

    auto sparams = llama_sampler_chain_default_params();
    auto smpl = llama_sampler_ptr{llama_sampler_chain_init(sparams)};
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_dist(params.sampling.seed));

    LOGV(LOG_LEVEL_INFO, "\n=== Test 5: seq copy (device) ===\n");

    // Load state from file
    llama_tokens unused_sts(tokens.size());
    size_t n_token_count_out = 0;

    if (!llama_state_load_file(ctx.get(), params.out_file.data(), unused_sts.data(), unused_sts.size(), &n_token_count_out)) {
        LOG_ERR("\n%s: failed to load state\n", __func__);
        return false;
    }

    LOG_TRC("%s: loaded state with %zu tokens\n", __func__, n_token_count_out);

    // Replay last token
    int n_past = (int) n_token_count_out - 1;
    if (!common_replay_last_token(ctx.get(), tokens.back(), n_past)) {
        return false;
    }
    n_past++;

    // Migrate KV cache from seq 0 to seq 1 (on-device path)
    {
        std::vector<uint8_t> seq_store(llama_state_seq_get_size_ext(ctx.get(), 0, LLAMA_STATE_SEQ_FLAGS_ON_DEVICE));
        const size_t ncopy = llama_state_seq_get_data_ext(ctx.get(), seq_store.data(), seq_store.size(), 0, LLAMA_STATE_SEQ_FLAGS_ON_DEVICE);
        if (ncopy != seq_store.size()) {
            LOG_ERR("\n%s: seq copy data length %zd does not match expected length %zd\n", __func__, ncopy, seq_store.size());
            return false;
        }
        LOG_TRC("%s: seq 0 copied, %zd bytes\n", __func__, ncopy);

        llama_memory_clear(llama_get_memory(ctx.get()), true);
        LOG_TRC("%s: kv cache cleared\n", __func__);

        const size_t nset = llama_state_seq_set_data_ext(ctx.get(), seq_store.data(), seq_store.size(), 1, LLAMA_STATE_SEQ_FLAGS_ON_DEVICE);
        if (nset != seq_store.size()) {
            LOG_ERR("\n%s: seq set data length %zd does not match expected length %zd\n", __func__, nset, seq_store.size());
            return false;
        }
        LOG_TRC("%s: seq 1 restored, %zd bytes\n", __func__, nset);
    }

    // Generate tokens and compare logits against the baseline
    if (!generate_tokens_compare(ctx.get(), smpl.get(), n_past, params.n_predict, 1, expected_result)) {
        return false;
    }

    LOGV(LOG_LEVEL_INFO, "\nPASS\n");
    return true;
}


// Test 9: device storage ring (R31/M14, LLAMA_STATE_SEQ_FLAGS_STORAGE)
// - snapshot seq 0 into device storage 1, then advance seq 0 and snapshot into storage 2
// - restoring storage 1 into seq 1 must reproduce the ORIGINAL continuation (storage 2 did not clobber it)
// - restoring storage 2 must still work afterwards (both copies coexist)
// RED on the single-store code (storage bits ignored -> the second get overwrites the first)
static bool test_seq_storage_ring(struct llama_model * model, const struct common_params & params, const llama_tokens & tokens, const generation_result & expected_result) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_seq_max = 2;
    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};

    auto sparams = llama_sampler_chain_default_params();
    auto smpl = llama_sampler_ptr{llama_sampler_chain_init(sparams)};
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_dist(params.sampling.seed));

    LOGV(LOG_LEVEL_INFO, "\n=== Test 9: device storage ring ===\n");

    llama_tokens unused_sts(tokens.size());
    size_t n_token_count_out = 0;

    if (!llama_state_load_file(ctx.get(), params.out_file.data(), unused_sts.data(), unused_sts.size(), &n_token_count_out)) {
        LOG_ERR("\n%s: failed to load state\n", __func__);
        return false;
    }

    int n_past = (int) n_token_count_out - 1;

    const uint32_t fl1 = LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_STORAGE(1);
    const uint32_t fl2 = LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_STORAGE(2);

    auto get_state = [&](uint32_t fl, std::vector<uint8_t> & store) {
        store.resize(llama_state_seq_get_size_ext(ctx.get(), 0, fl));
        const size_t n = llama_state_seq_get_data_ext(ctx.get(), store.data(), store.size(), 0, fl);
        if (n != store.size()) {
            LOG_ERR("\n%s: get_data returned %zd, expected %zd\n", __func__, n, store.size());
            return false;
        }
        return true;
    };

    std::vector<uint8_t> store1;
    std::vector<uint8_t> store2;

    // snapshot 1: the state at n_past, BEFORE the last prompt token is replayed. A seq state carries no logits,
    // so the restored sequence must re-decode the last token itself to sample from (R31 serial fix: the first
    // draft of this test snapshotted after the replay and then sampled from the ADVANCED seq 0's stale logits).
    if (!get_state(fl1, store1)) {
        return false;
    }

    if (!common_replay_last_token(ctx.get(), tokens.back(), n_past)) {
        return false;
    }
    n_past++;

    // advance seq 0, then snapshot 2 into a different storage slot
    {
        auto smpl_tmp = llama_sampler_ptr{llama_sampler_chain_init(sparams)};
        llama_sampler_chain_add(smpl_tmp.get(), llama_sampler_init_dist(params.sampling.seed));
        int n_past_adv = n_past; // generate_tokens advances its n_past; keep the snapshot position
        const auto advanced = generate_tokens(ctx.get(), smpl_tmp.get(), n_past_adv, params.n_predict, 0);
        if (advanced.empty()) {
            return false;
        }
    }
    if (!get_state(fl2, store2)) {
        return false;
    }

    // restore snapshot 1 into seq 1 and continue: must match the baseline continuation
    llama_memory_clear(llama_get_memory(ctx.get()), true);
    {
        const size_t nset = llama_state_seq_set_data_ext(ctx.get(), store1.data(), store1.size(), 1, fl1);
        if (nset != store1.size()) {
            LOG_ERR("\n%s: storage 1 restore returned %zd, expected %zd\n", __func__, nset, store1.size());
            return false;
        }
    }

    // replay the last prompt token on seq 1 (position n_past - 1) so the logits belong to the restored state
    {
        llama_batch_ptr batch(1, 0, 1);
        common_batch_clear(batch.get());
        common_batch_add(batch.get(), tokens.back(), n_past - 1, {1}, true);
        if (llama_decode(ctx.get(), batch.get())) {
            LOG_ERR("\n%s: failed to replay last token on seq 1\n", __func__);
            return false;
        }
    }

    if (!generate_tokens_compare(ctx.get(), smpl.get(), n_past, params.n_predict, 1, expected_result)) {
        LOG_ERR("\n%s: error: storage 1 was clobbered by the storage 2 snapshot (generation differs from expected)\n", __func__);
        return false;
    }

    // storage 2 must still be restorable after storage 1 was used
    llama_memory_clear(llama_get_memory(ctx.get()), true);
    {
        const size_t nset = llama_state_seq_set_data_ext(ctx.get(), store2.data(), store2.size(), 1, fl2);
        if (nset != store2.size()) {
            LOG_ERR("\n%s: storage 2 restore returned %zd, expected %zd\n", __func__, nset, store2.size());
            return false;
        }
    }

    LOGV(LOG_LEVEL_INFO, "\nPASS\n");
    return true;
}

// Test 10: device storage-ID OWNERSHIP across a full ring rotation (R45, lane-229)
// - snapshot A on seq 0 at N cells under an ALLOCATOR-ISSUED id, plus a host control of the same state
// - advance seq 0 so the cell count differs, then rotate past a full ring of saves while A stays LIVE
// - restore A into seq 1 and host-save it: the bytes must equal the host control
// RED on the R44 modulo allocator: the rotation wraps onto A's id and overwrites A's device buffers,
// so the restore aborts on the size guard (different cell counts) or silently returns the newer state.
static bool test_seq_storage_ownership(struct llama_model * model, const struct common_params & params,
                                       const llama_tokens & tokens, int test_num,
                                       ggml_type kv_type, const char * kv_name, bool on_device) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_ctx      = 256;
    params_ctx.n_seq_max  = 2;
    params_ctx.kv_unified = true;
    params_ctx.type_k     = kv_type;
    params_ctx.type_v     = kv_type;
    if (kv_type != GGML_TYPE_F16) {
        // a quantized V cache needs flash attention; archs that cannot do FA skip this leg below
        params_ctx.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;
    }

    LOG("\n=== Test %d: storage-id ownership (%s KV, %s) ===\n", test_num, kv_name,
        on_device ? "device" : "host control");

    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};
    if (ctx == nullptr) {
        // e.g. quantized KV on an arch that forces flash attention off - not a defect of this fix
        LOG("\nSKIP (%s KV context unsupported for this arch)\n", kv_name);
        return true;
    }

    if (tokens.size() < 60) {
        LOG_ERR("\n%s: need at least 60 tokens, got %zu\n", __func__, tokens.size());
        return false;
    }

    const auto decode_range = [&](int i0, int i1, llama_seq_id seq) {
        llama_batch_ptr batch(i1 - i0, 0, 1);
        common_batch_clear(batch.get());
        for (int i = i0; i < i1; ++i) {
            common_batch_add(batch.get(), tokens[i], i, { seq }, i == i1 - 1);
        }
        return llama_decode(ctx.get(), batch.get()) == 0;
    };

    const auto get_seq_state = [&](llama_seq_id seq_id, uint32_t fl, std::vector<uint8_t> & state) {
        const size_t state_size = llama_state_seq_get_size_ext(ctx.get(), seq_id, fl);
        if (state_size == 0) {
            LOG_ERR("\n%s: sequence state is empty\n", __func__);
            return false;
        }
        state.resize(state_size);
        const size_t ncopy = llama_state_seq_get_data_ext(ctx.get(), state.data(), state.size(), seq_id, fl);
        if (ncopy != state.size()) {
            LOG_ERR("\n%s: saved %zu bytes, expected %zu\n", __func__, ncopy, state.size());
            return false;
        }
        return true;
    };

    // the live checkpoint list the server would hold; A stays in it for the whole rotation
    std::list<common_prompt_checkpoint> live;
    uint32_t storage_next = 0;
    const uint32_t n_ring = 33;

    // the host control runs the same schedule through independent host images (no storage ids)
    const auto alloc_flags = [&](std::list<common_prompt_checkpoint> & lst) -> uint32_t {
        if (!on_device) {
            return LLAMA_STATE_SEQ_FLAGS_NONE;
        }
        const uint32_t storage = server_ckpt_storage_alloc(lst, storage_next, n_ring);
        if (storage == 0) {
            return 0;
        }
        return (uint32_t) (LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_STORAGE(storage));
    };

    // Everything happens on seq 1: a host state blob carries the sequence's own cell metadata, so a
    // byte comparison is only meaningful between saves of the SAME sequence id (see Tests 6/7).
    const llama_seq_id seq = 1;

    // --- state A: 40 cells ---
    if (!decode_range(0, 40, seq)) {
        LOG_ERR("\n%s: failed to build state A\n", __func__);
        return false;
    }

    std::vector<uint8_t> host_control;
    if (!get_seq_state(seq, LLAMA_STATE_SEQ_FLAGS_NONE, host_control)) {
        return false;
    }

    const uint32_t flags_a = alloc_flags(live);
    if (on_device) {
        if (flags_a == 0) {
            LOG_ERR("\n%s: allocator returned no storage id for A\n", __func__);
            return false;
        }
        auto & ckpt_a = live.emplace_back();
        ckpt_a.flags_tgt = flags_a;
        ckpt_a.flags_dft = flags_a;
    }

    std::vector<uint8_t> store_a;
    if (!get_seq_state(seq, flags_a, store_a)) {
        return false;
    }

    // --- advance to 60 cells: state B has a DIFFERENT byte total than A ---
    if (!decode_range(40, 60, seq)) {
        LOG_ERR("\n%s: failed to advance to state B\n", __func__);
        return false;
    }

    // --- rotate past a full ring while A stays live ---
    std::vector<uint8_t> store_rot;
    for (uint32_t r = 0; r < n_ring + 2; ++r) {
        // the rotating entry is transient: only A is retained, exactly the shape thinning produces
        std::list<common_prompt_checkpoint> live_now = live;
        const uint32_t flags_r = alloc_flags(live_now);
        if (on_device) {
            if (flags_r == 0) {
                LOG_ERR("\n%s: allocator returned no storage id at rotation %u\n", __func__, r);
                return false;
            }
            if ((flags_r & LLAMA_STATE_SEQ_FLAGS_STORAGE_MASK) == (flags_a & LLAMA_STATE_SEQ_FLAGS_STORAGE_MASK)) {
                LOG_ERR("\n%s: rotation %u was handed A's live storage id\n", __func__, r);
                return false;
            }
        }
        if (!get_seq_state(seq, flags_r, store_rot)) {
            return false;
        }
    }

    // --- restore A and compare with the host control ---
    llama_memory_clear(llama_get_memory(ctx.get()), true);
    {
        const size_t nset = llama_state_seq_set_data_ext(ctx.get(), store_a.data(), store_a.size(), seq, flags_a);
        if (nset != store_a.size()) {
            LOG_ERR("\n%s: storage A restore returned %zu, expected %zu\n", __func__, nset, store_a.size());
            return false;
        }
    }

    std::vector<uint8_t> host_after;
    if (!get_seq_state(seq, LLAMA_STATE_SEQ_FLAGS_NONE, host_after)) {
        return false;
    }

    if (host_control.size() != host_after.size()) {
        LOG_ERR("\n%s: error: restored state is %zu bytes, host control is %zu\n",
                __func__, host_after.size(), host_control.size());
        return false;
    }

    size_t n_diff = 0;
    size_t i_diff = 0;
    for (size_t i = 0; i < host_control.size(); ++i) {
        if (host_control[i] != host_after[i]) {
            if (n_diff == 0) {
                i_diff = i;
            }
            n_diff++;
        }
    }

    if (n_diff > 0) {
        LOG_ERR("\n%s: error: state A was clobbered by the ring rotation: %zu of %zu bytes differ, first at offset %zu\n",
                __func__, n_diff, host_control.size(), i_diff);
        return false;
    }

    LOG("\nPASS\n");
    return true;
}

// Test 6/7: seq copy (scatter)
// - decode the same prefix on two sequences, interleaving seq 0 cells between the seq 1 cells
// - save the seq 1 state, free the interleaved seq 0 cells, and restore via the given io path
// - the restore destination is non-contiguous: scatter reads are batched per contiguous run
// - save again on the host and compare the two blobs byte for byte
static bool test_seq_cp_scatter(struct llama_model * model, const struct common_params & params, const llama_tokens & tokens, int test_num, bool on_device) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_ctx      = 256;
    params_ctx.n_seq_max  = 2;
    params_ctx.kv_unified = true;
    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};

    LOGV(LOG_LEVEL_INFO, "\n=== Test %d: seq copy (%s, scatter) ===\n", test_num, on_device ? "device" : "host");

    const uint32_t flags = on_device ? LLAMA_STATE_SEQ_FLAGS_ON_DEVICE : LLAMA_STATE_SEQ_FLAGS_NONE;

    auto decode_one = [&](llama_token tok, int pos, llama_seq_id seq) {
        llama_batch_ptr batch(1, 0, 1);
        common_batch_add(batch.get(), tok, pos, { seq }, true);
        return llama_decode(ctx.get(), batch.get()) == 0;
    };

    // seq 0 cells 0,1,4 interleave the seq 1 cells 2,3,5
    if (!decode_one(tokens[0], 0, 0) ||
        !decode_one(tokens[1], 1, 0) ||
        !decode_one(tokens[0], 0, 1) ||
        !decode_one(tokens[1], 1, 1) ||
        !decode_one(tokens[2], 2, 0) ||
        !decode_one(tokens[2], 2, 1)) {
        LOG_ERR("%s: failed to build interleaved state\n", __func__);
        return false;
    }

    const auto get_seq_state = [&](llama_seq_id seq_id, uint32_t fl, std::vector<uint8_t> & state) {
        const size_t state_size = llama_state_seq_get_size_ext(ctx.get(), seq_id, fl);
        if (state_size == 0) {
            LOG_ERR("%s: sequence state is empty\n", __func__);
            return false;
        }

        state.resize(state_size);
        const size_t ncopy = llama_state_seq_get_data_ext(ctx.get(), state.data(), state.size(), seq_id, fl);
        if (ncopy != state.size()) {
            LOG_ERR("%s: sequence state length %zu does not match expected length %zu\n",
                    __func__, ncopy, state.size());
            return false;
        }

        return true;
    };

    // host blob: contains the KV data, used for the byte-for-byte comparison
    std::vector<uint8_t> state_before;
    if (!get_seq_state(1, LLAMA_STATE_SEQ_FLAGS_NONE, state_before)) {
        return false;
    }

    // save via the io path under test
    std::vector<uint8_t> state_save;
    if (!get_seq_state(1, flags, state_save)) {
        return false;
    }
    LOG_TRC("%s: seq 1 saved via %s, %zu bytes\n", __func__, on_device ? "device" : "host", state_save.size());

    // free seq 0's cells so the ring is fragmented: the restore destination (seq 1's interleaved cells) stays non-contiguous
    if (!llama_memory_seq_rm(llama_get_memory(ctx.get()), 0, -1, -1)) {
        LOG_ERR("%s: failed to remove sequence 0\n", __func__);
        return false;
    }

    // restore via the io path under test
    const size_t nset = llama_state_seq_set_data_ext(ctx.get(), state_save.data(), state_save.size(), 1, flags);
    if (nset != state_save.size()) {
        LOG_ERR("%s: seq set data length %zu does not match expected length %zu\n", __func__, nset, state_save.size());
        return false;
    }
    LOG_TRC("%s: seq 1 restored via %s, %zu bytes\n", __func__, on_device ? "device" : "host", nset);

    std::vector<uint8_t> state_after;
    if (!get_seq_state(1, LLAMA_STATE_SEQ_FLAGS_NONE, state_after)) {
        return false;
    }

    // the blob is serialized in sequence cell order, so identical bytes iff the restore wrote the same KV
    if (state_before.size() != state_after.size() || memcmp(state_before.data(), state_after.data(), state_before.size()) != 0) {
        LOG_ERR("\n%s: error: restored KV state is not byte-identical to the saved state\n", __func__);
        return false;
    }

    LOGV(LOG_LEVEL_INFO, "\nPASS\n");
    return true;
}


// Test 8: state blob round-trip
// compares blobs rather than generated text: a partially restored cell still decodes to plausible tokens
static bool test_state_roundtrip(struct llama_model * model, const struct common_params & params, const llama_tokens & tokens) {
    auto params_ctx = common_context_params_to_llama(params);
    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};

    LOGV(LOG_LEVEL_INFO, "\n=== Test 8: state blob round-trip ===\n");

    if (llama_decode(ctx.get(), llama_batch_get_one(const_cast<llama_token *>(tokens.data()), (int32_t) tokens.size()))) {
        LOG_ERR("\n%s: failed to decode prompt\n", __func__);
        return false;
    }

    std::vector<uint8_t> blob_a(llama_state_seq_get_size(ctx.get(), 0));
    const size_t n_a = llama_state_seq_get_data(ctx.get(), blob_a.data(), blob_a.size(), 0);
    if (n_a != blob_a.size()) {
        LOG_ERR("\n%s: saved %zu bytes, expected %zu\n", __func__, n_a, blob_a.size());
        return false;
    }

    if (!llama_memory_seq_rm(llama_get_memory(ctx.get()), 0, -1, -1)) {
        LOG_ERR("\n%s: failed to erase seq 0\n", __func__);
        return false;
    }

    if (llama_state_seq_set_data(ctx.get(), blob_a.data(), blob_a.size(), 0) != blob_a.size()) {
        LOG_ERR("\n%s: failed to restore seq 0\n", __func__);
        return false;
    }

    std::vector<uint8_t> blob_b(llama_state_seq_get_size(ctx.get(), 0));
    const size_t n_b = llama_state_seq_get_data(ctx.get(), blob_b.data(), blob_b.size(), 0);
    if (n_b != n_a) {
        LOG_ERR("\n%s: re-saved %zu bytes, expected %zu\n", __func__, n_b, n_a);
        return false;
    }

    size_t n_diff = 0;
    size_t i_diff = 0;
    for (size_t i = 0; i < n_a; i++) {
        if (blob_a[i] != blob_b[i]) {
            if (n_diff == 0) {
                i_diff = i;
            }
            n_diff++;
        }
    }

    if (n_diff > 0) {
        LOG_ERR("\n%s: state changed across a restore: %zu of %zu bytes differ, first at offset %zu\n",
                __func__, n_diff, n_a, i_diff);
        return false;
    }

    LOGV(LOG_LEVEL_INFO, "\nPASS\n");
    return true;
}


// Test 11: device checkpoint finalization failure (R46/R46b, lane-233)
//
// The device image used to be allocated and copied in llama_io_write_device's DESTRUCTOR, with no
// check on the allocator result, after state_seq_get_data had already returned a byte count. A
// failed device allocation therefore reported a successful save and then copied into null-backed
// tensors (GGML_ASSERT in ggml_backend_buffer_get_type, ggml-backend.cpp:330).
//
// The legs below inject a deterministic allocation or copy failure through the R46 seam and assert
// the repaired contract: the save returns 0, the image previously stored UNDER THE SAME STORAGE ID
// is still restorable byte-for-byte, other storage ids are untouched, and a plain retry recovers.
//
// Architecture independence (R46b): every save now allocates a fresh buffer off the live map, so
// attempt 1 of the injected allocation exists on every save, on every architecture. No leg depends
// on a state transition changing the checkpoint byte total - that assumption held for attention
// caches and not for recurrent ones.
//
// RED evidence: these legs cannot be run against the pre-R46 code, which has no injection seam at
// all. What they pin is the current contract; see the paired clean controls below, each of which
// runs the identical save with no variable set.
#ifdef LLAMA_TEST_FAULT_INJECTION
static void ckpt_fail_env(const char * name, const char * value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : ""); // an empty value removes the variable on win32
#else
    if (value) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}
#endif // LLAMA_TEST_FAULT_INJECTION

static bool test_ckpt_finalize_failure(struct llama_model * model, const struct common_params & params,
                                       const llama_tokens & tokens, int test_num) {
    auto params_ctx = common_context_params_to_llama(params);
    params_ctx.n_ctx      = 256;
    params_ctx.n_seq_max  = 2;
    params_ctx.kv_unified = true;

    LOG("\n=== Test %d: device checkpoint finalization failure ===\n", test_num);

#ifndef LLAMA_TEST_FAULT_INJECTION
    GGML_UNUSED(params_ctx);
    GGML_UNUSED(model);
    GGML_UNUSED(tokens);
    LOG("\nSKIPPED: built without LLAMA_TEST_FAULT_INJECTION, the checkpoint failure seam is not "
        "compiled in. This is the default in every build. This test claims NO coverage here; to "
        "run it, reconfigure with -DLLAMA_TEST_CHECKPOINT_FAULT_INJECTION=ON (a test-only build, "
        "never serve a binary built that way)\n");
    return true;
#else

    LOG("\nbuilt with LLAMA_TEST_FAULT_INJECTION (-DLLAMA_TEST_CHECKPOINT_FAULT_INJECTION=ON): the "
        "checkpoint failure seam is compiled in and this test drives it\n");

    auto ctx = llama_context_ptr{llama_init_from_model(model, params_ctx)};
    if (ctx == nullptr) {
        LOG_ERR("\n%s: failed to create the context\n", __func__);
        return false;
    }

    if (tokens.size() < 60) {
        LOG_ERR("\n%s: need at least 60 tokens, got %zu\n", __func__, tokens.size());
        return false;
    }

    const llama_seq_id seq = 1;

    const uint32_t fl1 = LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_STORAGE(1);
    const uint32_t fl2 = LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_STORAGE(2);
    const uint32_t fl3 = LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY | LLAMA_STATE_SEQ_FLAGS_STORAGE(3);
    const uint32_t fl4 = LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_STORAGE(4);

    const auto decode_range = [&](int i0, int i1) {
        llama_batch_ptr batch(i1 - i0, 0, 1);
        common_batch_clear(batch.get());
        for (int i = i0; i < i1; ++i) {
            common_batch_add(batch.get(), tokens[i], i, { seq }, i == i1 - 1);
        }
        return llama_decode(ctx.get(), batch.get()) == 0;
    };

    // a save that must succeed in full
    const auto save_ok = [&](uint32_t fl, std::vector<uint8_t> & state, const char * what) {
        const size_t state_size = llama_state_seq_get_size_ext(ctx.get(), seq, fl);
        if (state_size == 0) {
            LOG_ERR("\n%s: %s: sequence state is empty\n", __func__, what);
            return false;
        }
        state.resize(state_size);
        const size_t n = llama_state_seq_get_data_ext(ctx.get(), state.data(), state.size(), seq, fl);
        if (n != state.size()) {
            LOG_ERR("\n%s: %s: saved %zu bytes, expected %zu\n", __func__, what, n, state.size());
            return false;
        }
        return true;
    };

    // a save whose outcome is under test: returns the raw byte count. scratch keeps the serialized
    // header even when the device finalization failed, which is exactly the blob a caller that
    // ignored the 0 would later try to restore.
    std::vector<uint8_t> scratch;
    const auto save_raw = [&](uint32_t fl) {
        scratch.assign(llama_state_seq_get_size_ext(ctx.get(), seq, fl), 0);
        return llama_state_seq_get_data_ext(ctx.get(), scratch.data(), scratch.size(), seq, fl);
    };

    const auto device_restore = [&](const std::vector<uint8_t> & store, uint32_t fl) {
        llama_memory_clear(llama_get_memory(ctx.get()), true);
        return llama_state_seq_set_data_ext(ctx.get(), store.data(), store.size(), seq, fl);
    };

    // restore a stored image and compare the resulting sequence with a host control taken from an
    // earlier CLEAN restore of the same storage id. Using each storage id's own control is what
    // makes this valid for PARTIAL_ONLY images, which do not reproduce the full host state.
    const auto restore_matches = [&](const std::vector<uint8_t> & store, uint32_t fl,
                                     const std::vector<uint8_t> & control, const char * what) {
        const size_t nset = device_restore(store, fl);
        if (nset != store.size()) {
            LOG_ERR("\n%s: %s: restore returned %zu, expected %zu\n", __func__, what, nset, store.size());
            return false;
        }

        std::vector<uint8_t> host_after;
        if (!save_ok(LLAMA_STATE_SEQ_FLAGS_NONE, host_after, what)) {
            return false;
        }

        if (host_after.size() != control.size() ||
            memcmp(host_after.data(), control.data(), control.size()) != 0) {
            LOG_ERR("\n%s: %s: restored state differs from its control (%zu vs %zu bytes)\n",
                    __func__, what, host_after.size(), control.size());
            return false;
        }

        return true;
    };

    // --- state A: 40 cells, plus its host control ---
    if (!decode_range(0, 40)) {
        LOG_ERR("\n%s: failed to build state A\n", __func__);
        return false;
    }

    std::vector<uint8_t> host_control_a;
    if (!save_ok(LLAMA_STATE_SEQ_FLAGS_NONE, host_control_a, "host control A")) {
        return false;
    }

    // put the sequence back at state A from the host image - a deterministic reset that does not
    // depend on decoding the same tokens again
    const auto reset_to_a = [&]() {
        llama_memory_clear(llama_get_memory(ctx.get()), true);
        if (llama_state_seq_set_data_ext(ctx.get(), host_control_a.data(), host_control_a.size(),
                                         seq, LLAMA_STATE_SEQ_FLAGS_NONE) != host_control_a.size()) {
            LOG_ERR("\n%s: failed to reset the sequence to state A\n", __func__);
            return false;
        }
        return true;
    };

    // ---------------------------------------------------------------------------------------
    // A: the FIRST allocation for a storage id fails -> 0, and nothing is published, so a later
    // restore of that storage id fails cleanly instead of aborting or returning stale data
    // ---------------------------------------------------------------------------------------
    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_ALLOC", "1");
    const size_t n_first = save_raw(fl1);
    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_ALLOC", nullptr);

    if (n_first != 0) {
        LOG_ERR("\n%s: a failed first allocation reported %zu bytes saved\n", __func__, n_first);
        return false;
    }

    // the caller's output buffer after a failed save: no device image was published and the save
    // scrubbed the serialized header, so restoring it must fail rather than abort or return stale data
    const size_t n_orphan = device_restore(scratch, fl1);
    if (n_orphan != 0) {
        LOG_ERR("\n%s: restoring a storage id whose first save failed returned %zu, expected 0\n", __func__, n_orphan);
        return false;
    }
    LOG_TRC("%s: first-allocation failure returned 0 and published nothing\n", __func__);

    // clean control for the identical save with no injection
    if (!reset_to_a()) {
        return false;
    }

    std::vector<uint8_t> store_1a;
    if (!save_ok(fl1, store_1a, "clean control: first save into storage 1")) {
        return false;
    }

    std::vector<uint8_t> control_1a;
    if (device_restore(store_1a, fl1) != store_1a.size() ||
        !save_ok(LLAMA_STATE_SEQ_FLAGS_NONE, control_1a, "control for storage 1 at state A")) {
        LOG_ERR("\n%s: failed to establish the storage 1 control at state A\n", __func__);
        return false;
    }

    // ---------------------------------------------------------------------------------------
    // B: a REPLACEMENT allocation fails -> the previous image of THAT storage id survives.
    // State B is a different sequence state; no leg depends on it changing the byte total.
    // ---------------------------------------------------------------------------------------
    if (!decode_range(40, 60)) {
        LOG_ERR("\n%s: failed to advance to state B\n", __func__);
        return false;
    }

    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_ALLOC", "1");
    const size_t n_repl = save_raw(fl1);
    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_ALLOC", nullptr);

    if (n_repl != 0) {
        LOG_ERR("\n%s: a failed replacement allocation reported %zu bytes saved\n", __func__, n_repl);
        return false;
    }

    // A caller that ignored the 0 still holds whatever is in its output buffer. Restoring that must
    // fail. Without the scrub in the save's error path this leg restores a MIXED image on a
    // recurrent cache: the failed save's host-side cell metadata paired with the previous save's
    // device data, because the byte total did not change and the header was still well formed.
    if (device_restore(scratch, fl1) != 0) {
        LOG_ERR("\n%s: restoring the output buffer of a failed replacement save did not fail\n", __func__);
        return false;
    }

    if (!restore_matches(store_1a, fl1, control_1a, "storage 1 after a failed replacement allocation")) {
        return false;
    }
    LOG_TRC("%s: storage 1 survived the failed replacement allocation\n", __func__);

    // ---------------------------------------------------------------------------------------
    // C: a COPY fails during a replacement -> the previous image of that storage id survives
    // ---------------------------------------------------------------------------------------
    if (!reset_to_a() || !decode_range(40, 60)) {
        LOG_ERR("\n%s: failed to rebuild state B\n", __func__);
        return false;
    }

    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_COPY", "1");
    const size_t n_copy = save_raw(fl1);
    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_COPY", nullptr);

    if (n_copy != 0) {
        LOG_ERR("\n%s: a failed replacement copy reported %zu bytes saved\n", __func__, n_copy);
        return false;
    }

    if (device_restore(scratch, fl1) != 0) {
        LOG_ERR("\n%s: restoring the output buffer of a failed replacement copy did not fail\n", __func__);
        return false;
    }

    if (!restore_matches(store_1a, fl1, control_1a, "storage 1 after a failed replacement copy")) {
        return false;
    }
    LOG_TRC("%s: storage 1 survived the failed replacement copy\n", __func__);

    // clean control for the replacement itself: it must actually succeed and change the image
    if (!reset_to_a() || !decode_range(40, 60)) {
        LOG_ERR("\n%s: failed to rebuild state B for the clean replacement control\n", __func__);
        return false;
    }

    std::vector<uint8_t> host_control_b;
    if (!save_ok(LLAMA_STATE_SEQ_FLAGS_NONE, host_control_b, "host control B")) {
        return false;
    }

    std::vector<uint8_t> store_1b;
    if (!save_ok(fl1, store_1b, "clean control: replacement save into storage 1")) {
        return false;
    }

    std::vector<uint8_t> control_1b;
    if (device_restore(store_1b, fl1) != store_1b.size() ||
        !save_ok(LLAMA_STATE_SEQ_FLAGS_NONE, control_1b, "control for storage 1 at state B")) {
        LOG_ERR("\n%s: failed to establish the storage 1 control at state B\n", __func__);
        return false;
    }

    if (control_1b.size() != host_control_b.size() ||
        memcmp(control_1b.data(), host_control_b.data(), host_control_b.size()) != 0) {
        LOG_ERR("\n%s: the clean replacement did not reproduce state B\n", __func__);
        return false;
    }
    LOG_TRC("%s: the clean replacement published state B\n", __func__);

    // ---------------------------------------------------------------------------------------
    // D: a copy fails on a FIRST save into an unused storage id -> 0, nothing published, retry works
    // ---------------------------------------------------------------------------------------
    if (!reset_to_a()) {
        return false;
    }

    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_COPY", "1");
    const size_t n_new = save_raw(fl2);
    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_COPY", nullptr);

    if (n_new != 0) {
        LOG_ERR("\n%s: a failed first copy reported %zu bytes saved\n", __func__, n_new);
        return false;
    }

    if (device_restore(scratch, fl2) != 0) {
        LOG_ERR("\n%s: restoring storage 2 after a failed first copy did not fail\n", __func__);
        return false;
    }

    if (!reset_to_a()) {
        return false;
    }

    std::vector<uint8_t> store_2a;
    if (!save_ok(fl2, store_2a, "retry after first-copy failure")) {
        return false;
    }

    std::vector<uint8_t> control_2a;
    if (device_restore(store_2a, fl2) != store_2a.size() ||
        !save_ok(LLAMA_STATE_SEQ_FLAGS_NONE, control_2a, "control for storage 2")) {
        LOG_ERR("\n%s: failed to establish the storage 2 control\n", __func__);
        return false;
    }

    if (control_2a.size() != host_control_a.size() ||
        memcmp(control_2a.data(), host_control_a.data(), host_control_a.size()) != 0) {
        LOG_ERR("\n%s: the recovered storage 2 save did not reproduce state A\n", __func__);
        return false;
    }

    // storage 1 must still hold its own state-B image, untouched by everything storage 2 did
    if (!restore_matches(store_1b, fl1, control_1b, "storage 1 after the storage 2 failures")) {
        return false;
    }
    LOG_TRC("%s: the storage ids stayed independent across the failures\n", __func__);

    // ---------------------------------------------------------------------------------------
    // E: storage 3 (PARTIAL_ONLY) - seeded, controlled and verified as the SAME storage id that
    // gets failed. A partial image does not reproduce the full host state, so its control is one
    // clean restore of its own image rather than the host control.
    // ---------------------------------------------------------------------------------------
    if (!reset_to_a()) {
        return false;
    }

    std::vector<uint8_t> store_3a;
    if (!save_ok(fl3, store_3a, "clean seed of storage 3")) {
        return false;
    }

    std::vector<uint8_t> control_3a;
    if (device_restore(store_3a, fl3) != store_3a.size() ||
        !save_ok(LLAMA_STATE_SEQ_FLAGS_NONE, control_3a, "control for storage 3")) {
        LOG_ERR("\n%s: failed to establish the storage 3 control\n", __func__);
        return false;
    }

    // a replacement into storage 3 fails -> the seeded storage 3 image survives byte-for-byte
    if (!reset_to_a() || !decode_range(40, 60)) {
        LOG_ERR("\n%s: failed to rebuild state B for the storage 3 failure\n", __func__);
        return false;
    }

    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_ALLOC", "1");
    const size_t n_p3 = save_raw(fl3);
    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_ALLOC", nullptr);

    if (n_p3 != 0) {
        LOG_ERR("\n%s: a failed storage 3 replacement reported %zu bytes saved\n", __func__, n_p3);
        return false;
    }

    if (!restore_matches(store_3a, fl3, control_3a, "storage 3 after a failed replacement")) {
        return false;
    }
    LOG_TRC("%s: storage 3 survived the failed replacement\n", __func__);

    // ---------------------------------------------------------------------------------------
    // F: two buffer types in ONE finalization. Attempt 2 only exists on a device that exposes a
    // second checkpoint buffer type. On a single-buffer-type device this leg is NOT coverage -
    // it is reported as not exercised rather than counted as a pass.
    // ---------------------------------------------------------------------------------------
    if (!reset_to_a() || !decode_range(40, 60)) {
        LOG_ERR("\n%s: failed to rebuild state B for the multi-buffer leg\n", __func__);
        return false;
    }

    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_ALLOC", "2");
    const size_t n_multi = save_raw(fl3);
    ckpt_fail_env("LLAMA_R46_CKPT_FAIL_ALLOC", nullptr);

    if (n_multi == 0) {
        LOG("%s: multi-buffer leg EXERCISED - the second buffer type's allocation failure returned 0\n", __func__);

        if (!restore_matches(store_3a, fl3, control_3a, "storage 3 after a second-buffer-type failure")) {
            return false;
        }
    } else {
        if (n_multi != scratch.size()) {
            LOG_ERR("\n%s: partial save of %zu bytes, expected %zu\n", __func__, n_multi, scratch.size());
            return false;
        }
        LOG("%s: multi-buffer leg NOT EXERCISED - this device exposes a single checkpoint buffer "
            "type, so attempt 2 never happens. It needs a backend with two checkpoint buffer types "
            "(e.g. a partly offloaded cache) to cover.\n", __func__);
    }

    // ---------------------------------------------------------------------------------------
    // G: zero-byte device image. A sequence with no cells produces no device tensor at all, so
    // nothing is allocated and no backend copy runs on a null buffer. The defined representation
    // is "no entry for that buffer type", and the round trip must still succeed.
    // ---------------------------------------------------------------------------------------
    llama_memory_clear(llama_get_memory(ctx.get()), true);

    const size_t n_empty = save_raw(fl4);
    const std::vector<uint8_t> store_4 = scratch;

    // without this the two assertions below are satisfied by 0 == 0 and prove nothing
    if (store_4.empty()) {
        LOG_ERR("\n%s: the empty-sequence device save produced no blob at all\n", __func__);
        return false;
    }

    if (n_empty != store_4.size()) {
        LOG_ERR("\n%s: an empty-sequence device save returned %zu, expected %zu\n", __func__, n_empty, store_4.size());
        return false;
    }

    llama_memory_clear(llama_get_memory(ctx.get()), true);

    const size_t n_empty_set = llama_state_seq_set_data_ext(ctx.get(), store_4.data(), store_4.size(), seq, fl4);
    if (n_empty_set != store_4.size()) {
        LOG_ERR("\n%s: an empty-sequence device restore returned %zu, expected %zu\n", __func__, n_empty_set, store_4.size());
        return false;
    }
    LOG_TRC("%s: the zero-byte device image round-tripped (%zu header bytes, no device buffer)\n", __func__, store_4.size());

    // ---------------------------------------------------------------------------------------
    // the storage ids still work normally once no failure is injected
    // ---------------------------------------------------------------------------------------
    if (!reset_to_a()) {
        return false;
    }

    std::vector<uint8_t> store_final;
    if (!save_ok(fl3, store_final, "clean save after all injected failures")) {
        return false;
    }

    if (device_restore(store_final, fl3) != store_final.size()) {
        LOG_ERR("\n%s: the final clean storage 3 restore failed\n", __func__);
        return false;
    }

    LOG("\nPASS\n");
    return true;
#endif // LLAMA_TEST_FAULT_INJECTION
}


// Test 10: state rotation
// a KV state saved with attention rotation enabled must restore only into a context with the same setting;
// note: rotation is only active for quantized KV caches with a head size that is a multiple of 64,
//       for other models the restore into the rotation-disabled context is valid and the test passes vacuously
static bool test_state_rotation(struct llama_model * model, const struct common_params & params) {
    LOGV(LOG_LEVEL_INFO, "\n=== Test 10: state rotation ===\n");

    // Ours: rotation is OFF unless LLAMA_ATTN_ROT_{K,V}_OVERRIDE=1, and LLAMA_ATTN_ROT_K_NROT picks the K tile
    // (default 64, 0 = largest power of 2). Upstream's test is vacuous here (rotation never on, and a mismatched
    // restore only logs), so this version forces rotation on and REQUIRES the mismatched restores to fail when the
    // model's K head size qualifies (q8_0 cache, head_k % 64 == 0): rotation off, and K tile 64 vs the largest.
    const std::string attn_rot_disable = common_get_env("LLAMA_ATTN_ROT_DISABLE");
    const std::string rot_k_ov = common_get_env("LLAMA_ATTN_ROT_K_OVERRIDE");
    const std::string rot_v_ov = common_get_env("LLAMA_ATTN_ROT_V_OVERRIDE");
    const std::string rot_k_nrot = common_get_env("LLAMA_ATTN_ROT_K_NROT");

    int64_t head_k = 0;
    {
        char arch[64] = {};
        char buf[64]  = {};
        if (llama_model_meta_val_str(model, "general.architecture", arch, sizeof(arch)) > 0 &&
            llama_model_meta_val_str(model, (std::string(arch) + ".attention.key_length").c_str(), buf, sizeof(buf)) > 0) {
            head_k = atoll(buf);
        } else if (llama_model_n_head(model) > 0) {
            head_k = llama_model_n_embd(model) / llama_model_n_head(model);
        }
    }
    const bool expect_rot      = head_k > 0 && head_k % 64 == 0;
    const bool expect_tile_mis = head_k > 0 && head_k % 128 == 0;
    LOG("%s: head_k=%lld expect_rot=%d expect_tile_mismatch=%d\n", __func__, (long long) head_k, expect_rot, expect_tile_mis);

    // mode 0: rotation on, K tile 64; mode 1: rotation off; mode 2: rotation on, K tile = largest power of 2
    const auto make_context = [&](ggml_type type_k, ggml_type type_v, int mode) {
        common_set_env("LLAMA_ATTN_ROT_DISABLE", mode == 1 ? "1" : "0");
        common_set_env("LLAMA_ATTN_ROT_K_OVERRIDE", "1");
        common_set_env("LLAMA_ATTN_ROT_V_OVERRIDE", "1");
        common_set_env("LLAMA_ATTN_ROT_K_NROT", mode == 2 ? "0" : "64");
        auto params_ctx = common_context_params_to_llama(params);
        params_ctx.n_ctx    = 32;
        params_ctx.n_batch  = 1;
        params_ctx.n_ubatch = 1;
        params_ctx.type_k   = type_k;
        params_ctx.type_v   = type_v;
        params_ctx.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;
        return llama_context_ptr(llama_init_from_model(model, params_ctx));
    };

    std::vector<std::pair<ggml_type, ggml_type>> type_pairs;
    for (const auto & types : { std::pair{GGML_TYPE_Q8_0, GGML_TYPE_Q8_0} }) {
        if (make_context(types.first, types.second, 0)) {
            type_pairs.push_back(types);
        }
    }
    if (type_pairs.empty()) {
        LOG_WRN("%s: no supported quantized KV cache type combination - skipping\n", __func__);
        return true;
    }

    bool success = true;
    for (const auto & types : type_pairs) {
        auto src = make_context(types.first, types.second, 0);
        if (!src) {
            LOG_ERR("%s: failed to create source context\n", __func__);
            success = false;
            break;
        }

        llama_token token = 0;
        if (llama_decode(src.get(), llama_batch_get_one(&token, 1))) {
            LOG_ERR("%s: failed to decode token\n", __func__);
            success = false;
            break;
        }

        const size_t state_size = llama_state_seq_get_size(src.get(), 0);
        if (state_size == 0) {
            continue; // no KV state to test
        }

        std::vector<uint8_t> state(state_size);
        if (llama_state_seq_get_data(src.get(), state.data(), state.size(), 0) != state.size()) {
            LOG_ERR("%s: failed to save sequence state\n", __func__);
            success = false;
            break;
        }

        auto matching = make_context(types.first, types.second, 0);
        if (!matching || llama_state_seq_set_data(matching.get(), state.data(), state.size(), 0) != state.size()) {
            LOG_ERR("%s: failed to restore matching rotation\n", __func__);
            success = false;
            break;
        }

        for (const int mode : { 1, 2 }) {
            const bool expect_reject = mode == 1 ? expect_rot : expect_tile_mis;
            auto mismatched = make_context(types.first, types.second, mode);
            if (!mismatched) {
                LOG_ERR("%s: failed to create mismatched rotation context (mode %d)\n", __func__, mode);
                success = false;
                break;
            }
            const bool accepted = llama_state_seq_set_data(mismatched.get(), state.data(), state.size(), 0) != 0;
            LOG("%s: restore into %s: %s (expected %s)\n", __func__, mode == 1 ? "rotation-off" : "K-tile-largest",
                accepted ? "accepted" : "rejected", expect_reject ? "rejected" : "either");
            if (expect_reject && accepted) {
                LOG_ERR("%s: state saved with one rotation was restored into a context with another\n", __func__);
                success = false;
            }
        }
    }
    common_set_env("LLAMA_ATTN_ROT_DISABLE", attn_rot_disable);
    common_set_env("LLAMA_ATTN_ROT_K_OVERRIDE", rot_k_ov.empty() ? "0" : rot_k_ov);
    common_set_env("LLAMA_ATTN_ROT_V_OVERRIDE", rot_v_ov.empty() ? "0" : rot_v_ov);
    common_set_env("LLAMA_ATTN_ROT_K_NROT", rot_k_nrot.empty() ? "64" : rot_k_nrot);

    if (!success) {
        return false;
    }

    LOGV(LOG_LEVEL_INFO, "\nPASS\n");
    return true;
}

struct test_suite {
    std::vector<test_status> results;

    bool all_passed() const {
        return std::all_of(results.begin(), results.end(), [](test_status s) { return s == test_status::PASS; });
    }
};

static bool test_owned_snapshot(llama_model * model, const common_params & params) {
    auto cp = common_context_params_to_llama(params);
    cp.n_seq_max = 2;
    cp.kv_unified = true;
    auto ctx = llama_context_ptr{llama_init_from_model(model, cp)};
    if (!ctx) { return false; }
    if (llama_state_seq_snapshot_create(ctx.get(), -1)) { return false; }
    if (llama_state_seq_snapshot_create(ctx.get(), cp.n_seq_max)) { return false; }
    {
        auto empty = std::unique_ptr<llama_state_seq_snapshot, decltype(&llama_state_seq_snapshot_free)>(
            llama_state_seq_snapshot_create(ctx.get(), 0), llama_state_seq_snapshot_free);
        if (!empty || !llama_state_seq_snapshot_restore(ctx.get(), empty.get(), 0)) { return false; }
        llama_token token = 1;
        if (llama_decode(ctx.get(), llama_batch_get_one(&token, 1)) != 0 ||
                llama_memory_seq_pos_max(llama_get_memory(ctx.get()), 0) < 0) { return false; }
        if (!llama_state_seq_snapshot_restore(ctx.get(), empty.get(), 0) ||
                llama_memory_seq_pos_max(llama_get_memory(ctx.get()), 0) != -1) { return false; }
        auto other = llama_context_ptr{llama_init_from_model(model, cp)};
        if (!other || llama_state_seq_snapshot_restore(other.get(), empty.get(), 0)) { return false; }
    }
    const auto save_host = [&]() {
        std::vector<uint8_t> data(llama_state_seq_get_size(ctx.get(), 0));
        if (llama_state_seq_get_data(ctx.get(), data.data(), data.size(), 0) != data.size()) { data.clear(); }
        return data;
    };
    llama_token first[] = {1, 2, 3, 4};
    if (llama_decode(ctx.get(), llama_batch_get_one(first, 4)) != 0) { return false; }
    auto a = std::unique_ptr<llama_state_seq_snapshot, decltype(&llama_state_seq_snapshot_free)>(
        llama_state_seq_snapshot_create(ctx.get(), 0), llama_state_seq_snapshot_free);
    const auto host_a = save_host();
#ifdef LLAMA_TEST_FAULT_INJECTION
    for (const char * seam : {"LLAMA_R46_CKPT_FAIL_ALLOC", "LLAMA_R46_CKPT_FAIL_COPY"}) {
        ckpt_fail_env(seam, "1");
        auto failed = std::unique_ptr<llama_state_seq_snapshot, decltype(&llama_state_seq_snapshot_free)>(
            llama_state_seq_snapshot_create(ctx.get(), 0), llama_state_seq_snapshot_free);
        ckpt_fail_env(seam, nullptr);
        if (failed || save_host() != host_a) { return false; }
        if (!llama_state_seq_snapshot_restore(ctx.get(), a.get(), 0) || save_host() != host_a) { return false; }
    }
#endif
    llama_token next[] = {5, 6};
    if (!a || host_a.empty() || llama_decode(ctx.get(), llama_batch_get_one(next, 2)) != 0) { return false; }
    auto b = std::unique_ptr<llama_state_seq_snapshot, decltype(&llama_state_seq_snapshot_free)>(
        llama_state_seq_snapshot_create(ctx.get(), 0), llama_state_seq_snapshot_free);
    const auto host_b = save_host();
    if (!b || host_b.empty() || host_a == host_b) { return false; }
    // Reusing the legacy storage slot must not overwrite either independently owned image.
    const auto flags = LLAMA_STATE_SEQ_FLAGS_ON_DEVICE;
    std::vector<uint8_t> ring(llama_state_seq_get_size_ext(ctx.get(), 0, flags));
    if (llama_state_seq_get_data_ext(ctx.get(), ring.data(), ring.size(), 0, flags) != ring.size()) { return false; }
    const bool red = getenv("LLAMA_TEST_SNAPSHOT_RED") != nullptr;
    const bool restored = llama_state_seq_snapshot_restore(ctx.get(), red ? b.get() : a.get(), 0);
    const auto restored_a = save_host();
    if (!restored || restored_a != host_a) {
        size_t first_diff = 0;
        while (first_diff < std::min(restored_a.size(), host_a.size()) && restored_a[first_diff] == host_a[first_diff]) { ++first_diff; }
        LOG_ERR("snapshot A: restored=%d before=%zu after=%zu first_diff=%zu\n", restored, host_a.size(), restored_a.size(), first_diff);
        LOG_ERR("owned snapshot A byte equality failed (RED=%d)\n", red);
        return false;
    }
    if (!llama_state_seq_snapshot_restore(ctx.get(), b.get(), 0) || save_host() != host_b) { LOG_ERR("snapshot B byte comparison failed\n"); return false; }
    if (!llama_state_seq_snapshot_restore(ctx.get(), a.get(), 0)) { return false; }
    b.reset(llama_state_seq_snapshot_recreate(ctx.get(), 0, b.release()));
    if (!b || !llama_state_seq_snapshot_restore(ctx.get(), b.get(), 0) || save_host() != host_a) { return false; }
    b.reset();
    if (!llama_state_seq_snapshot_restore(ctx.get(), a.get(), 0) || save_host() != host_a) { LOG_ERR("snapshot A after eviction failed\n"); return false; }
    if (llama_state_seq_snapshot_restore(ctx.get(), nullptr, 0)) { return false; }
    if (llama_state_seq_snapshot_restore(ctx.get(), a.get(), -1)) { return false; }
    if (llama_state_seq_snapshot_restore(ctx.get(), a.get(), cp.n_seq_max)) { return false; }
    if (llama_state_seq_snapshot_size(a.get()) == 0 || llama_state_seq_snapshot_size(nullptr) != 0) { return false; }
    // Compare cross-slot continuation against the existing host-image route in
    // the same slot/layout. Comparing slot 0 against slot 1 also measures the
    // model's sequence-layout rounding, unrelated to snapshot storage.
    llama_batch_ptr batch(1, 0, 1);
    common_batch_clear(batch.get());
    common_batch_add(batch.get(), 7, 4, {1}, true);
    llama_memory_clear(llama_get_memory(ctx.get()), true);
    if (llama_state_seq_set_data(ctx.get(), host_a.data(), host_a.size(), 1) != host_a.size()) { return false; }
    const auto save_slot_one = [&]() {
        std::vector<uint8_t> data(llama_state_seq_get_size(ctx.get(), 1));
        if (llama_state_seq_get_data(ctx.get(), data.data(), data.size(), 1) != data.size()) { data.clear(); }
        return data;
    };
    const auto host_slot_one = save_slot_one();
    if (host_slot_one.empty() || llama_decode(ctx.get(), batch.get()) != 0) { return false; }
    std::vector<float> reference;
    if (!get_current_logits(ctx.get(), reference)) { return false; }
    llama_memory_clear(llama_get_memory(ctx.get()), true);
    if (!llama_state_seq_snapshot_restore(ctx.get(), a.get(), 1) || save_slot_one() != host_slot_one) {
        LOG_ERR("cross-slot host/device state mismatch\n"); return false;
    }
    if (llama_decode(ctx.get(), batch.get()) != 0) { return false; }
    std::vector<float> cross_slot;
    if (!get_current_logits(ctx.get(), cross_slot)) { return false; }
    const double error = nmse(reference, cross_slot);
    LOG("cross-slot host/device logits NMSE=%g\n", error);
    if (error > NMSE_THRESHOLD) { return false; }
    LOG("owned snapshot: A/B bytes, ring independence, eviction, cross-slot logits, empty image and invalid owner/input PASS\n");
    return true;
}

// column headers for the --models table, one per test, in the order they are run
static const std::vector<const char *> test_names = getenv("LLAMA_TEST_SNAPSHOT_ONLY")
    ? std::vector<const char *>{ "snapshot" }
    : std::vector<const char *>{ "baseline", "seq_rm", "state_load", "cp_h", "cp_d", "cp_h_s", "cp_d_s", "rt", "ring", "own", "fin", "snapshot", "rot" };

// Run the full save/load test suite for a single model.
// Returns the per-test results.
static test_suite run_save_load_tests_for_model(const std::string & model_path, const struct common_params & base_params) {
    test_suite suite;

    struct common_params params = base_params;
    params.model.path = model_path;

    auto llama_init = common_init_from_params(params, true);
    auto * model = llama_init->model();

    if (model == nullptr) {
        LOG_ERR("%s: failed to init model '%s'\n", __func__, model_path.c_str());
        suite.results.assign(test_names.size(), test_status::SKIP);
        return suite;
    }

    GGML_ASSERT(llama_init->context() == nullptr);

    if (getenv("LLAMA_TEST_SNAPSHOT_ONLY")) {
        suite.results.push_back(test_owned_snapshot(model, params) ? test_status::PASS : test_status::FAIL);
        return suite;
    }

    // Tokenize prompt or generate random tokens
    llama_tokens tokens;
    if (params.prompt.empty()) {
        const int n_prompt = params.n_batch;

        // this path is useful for model files that do not have a tokenizer
        LOG_INF("%s: no prompt provided, generating %d (n_batch) random tokens\n", __func__, n_prompt);

        const auto * vocab = llama_model_get_vocab(model);
        const auto n_vocab = llama_vocab_n_tokens(vocab);

        std::mt19937 rng(params.sampling.seed);
        std::uniform_int_distribution<llama_token> dist(0, n_vocab - 1);
        for (int i = 0; i < n_prompt; i++) {
            tokens.push_back(dist(rng));
        }
    } else {
        LOG_INF("%s: tokenizing prompt '%s'\n", __func__, params.prompt.c_str());

        auto ctx = llama_context_ptr{llama_init_from_model(model, common_context_params_to_llama(params))};
        tokens = common_tokenize(ctx.get(), params.prompt, true);
    }

    LOG_INF("%s: the input prompt is %d tokens\n", __func__, (int)tokens.size());

    // Test 1: baseline (saves state to disk)
    auto result_baseline = test_baseline(model, params, tokens);
    suite.results.push_back(result_baseline.empty() ? test_status::FAIL : test_status::PASS);

    // Test 2: sequence removal isolation
    suite.results.push_back(test_seq_rm_isolated(model, params, tokens) ? test_status::PASS : test_status::FAIL);

    if (!result_baseline.empty()) {
        // Test 3: state load
        suite.results.push_back(test_state_load(model, params, tokens, result_baseline) ? test_status::PASS : test_status::FAIL);

        // Test 4: seq copy (host)
        suite.results.push_back(test_seq_cp_host(model, params, tokens, result_baseline) ? test_status::PASS : test_status::FAIL);

        // Test 5: seq copy (device)
        suite.results.push_back(test_seq_cp_device(model, params, tokens, result_baseline) ? test_status::PASS : test_status::FAIL);
    } else {
        // tests 3-5 depend on the baseline result and the state file it saves
        suite.results.push_back(test_status::SKIP);
        suite.results.push_back(test_status::SKIP);
        suite.results.push_back(test_status::SKIP);
    }

    // Test 6: seq copy (host, scatter)
    suite.results.push_back(test_seq_cp_scatter(model, params, tokens, 6, false) ? test_status::PASS : test_status::FAIL);

    // Test 7: seq copy (device, scatter)
    suite.results.push_back(test_seq_cp_scatter(model, params, tokens, 7, true) ? test_status::PASS : test_status::FAIL);

    // Test 8: state blob round-trip
    suite.results.push_back(test_state_roundtrip(model, params, tokens) ? test_status::PASS : test_status::FAIL);

    // Test 9: device storage ring (R31/M14); compares against the baseline generation
    if (!result_baseline.empty()) {
        suite.results.push_back(test_seq_storage_ring(model, params, tokens, result_baseline) ? test_status::PASS : test_status::FAIL);
    } else {
        suite.results.push_back(test_status::SKIP);
    }

    // Test 10: storage-id ownership across a ring rotation (R45, lane-229). The host-image legs are
    // the control: they run the same schedule with no storage ids, so a failure there is a harness
    // problem, not a device-ownership defect. One suite column: all four legs must pass.
    {
        const bool own_ok =
            test_seq_storage_ownership(model, params, tokens, 10, GGML_TYPE_F16,  "f16",  false) &&
            test_seq_storage_ownership(model, params, tokens, 10, GGML_TYPE_F16,  "f16",  true)  &&
            test_seq_storage_ownership(model, params, tokens, 10, GGML_TYPE_Q8_0, "q8_0", false) &&
            test_seq_storage_ownership(model, params, tokens, 10, GGML_TYPE_Q8_0, "q8_0", true);
        suite.results.push_back(own_ok ? test_status::PASS : test_status::FAIL);
    }

    // Test 11: checkpoint finalization failure (R46, lane-233); passes as a documented no-op unless
    // built with -DLLAMA_TEST_CHECKPOINT_FAULT_INJECTION=ON
    suite.results.push_back(test_ckpt_finalize_failure(model, params, tokens, 11) ? test_status::PASS : test_status::FAIL);
    suite.results.push_back(test_owned_snapshot(model, params) ? test_status::PASS : test_status::FAIL);

    // Test 10: state rotation
    suite.results.push_back(test_state_rotation(model, params) ? test_status::PASS : test_status::FAIL);

    return suite;
}


static void print_usage(int /* argc */, char ** argv) {
    LOG("\nexample usage:\n");
    LOG("\n  %s -m your_model.gguf\n", argv[0]);
    LOG("\n  %s --models tests/test-models\n", argv[0]);
    LOG("\n  %s -m your_model.gguf -lv 5\n", argv[0]);
    LOG("\n");
}

int main(int argc, char ** argv) {
    std::setlocale(LC_NUMERIC, "C");

    common_params params;
    params.prompt = "";
    params.n_batch = 100;
    params.out_file = "dump_state.bin";
    params.sampling.seed = 1234;

    common_init();

    // extract our own --models DIR option before handing the rest to the common arg parser
    std::string models_dir;
    std::vector<char *> filtered_argv;
    filtered_argv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--models") == 0) {
            if (i + 1 >= argc) {
                LOG_ERR("%s: --models requires a directory argument\n", __func__);
                return 1;
            }
            models_dir = argv[i + 1];
            i++;
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }
    filtered_argv.push_back(nullptr);
    const int fargc = (int)filtered_argv.size() - 1;

    // in --models mode there is no single model; set a placeholder so the common parser's
    // "--model is required" check passes (each model is set individually inside the loop)
    if (!models_dir.empty()) {
        params.model.path = models_dir;
    }

    if (!common_params_parse(fargc, filtered_argv.data(), params, LLAMA_EXAMPLE_COMMON, print_usage)) {
        return 1;
    }

    if (params.n_parallel == 1) {
        LOG_TRC("%s: n_parallel == 1, enabling unified kv cache\n", __func__);
        params.kv_unified = true;
    }

    if (params.n_predict < 0) {
        params.n_predict = 16;
    }

    ggml_backend_load_all();

    if (!models_dir.empty()) {
        // run the suite over every dummy model in the directory
        if (!std::filesystem::exists(models_dir) || !std::filesystem::is_directory(models_dir)) {
            LOG_ERR("%s: models directory '%s' does not exist\n", __func__, models_dir.c_str());
            return 1;
        }

        std::vector<std::string> models;
        for (const auto & entry : std::filesystem::directory_iterator(models_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                models.push_back(entry.path().string());
            }
        }
        std::sort(models.begin(), models.end());

        if (models.empty()) {
            LOG_ERR("%s: no .gguf models found in '%s'\n", __func__, models_dir.c_str());
            return 1;
        }

        auto col_width = [](const char * name) { return (int) std::max(strlen(name), (size_t) 4); };

        size_t name_width = 5; // "Model"
        for (const auto & model_path : models) {
            name_width = std::max(name_width, std::filesystem::path(model_path).filename().string().size());
        }

        // silence everything but the table itself (LOG has verbosity LOG_LEVEL_OUTPUT = 0)
        common_log_set_verbosity_thold(0);

        LOG("%-*s", (int) name_width, "Model");
        for (const auto & name : test_names) {
            LOG("  %-*s", col_width(name), name);
        }
        LOG("\n");
        common_log_flush(common_log_main());

        size_t n_pass = 0;
        size_t n_fail = 0;
        for (const auto & model_path : models) {
            const auto name = std::filesystem::path(model_path).filename().string();

            LOG("%-*s", (int) name_width, name.c_str());
            common_log_flush(common_log_main());

            const test_suite suite = run_save_load_tests_for_model(model_path, params);

            for (size_t i = 0; i < suite.results.size(); i++) {
                LOG("  %s%*s", test_status_str(suite.results[i]), col_width(test_names[i]) - 4, "");
            }
            LOG("\n");
            common_log_flush(common_log_main());

            if (suite.all_passed()) {
                n_pass++;
            } else {
                n_fail++;
            }
        }

        common_log_set_verbosity_thold(LOG_DEFAULT_LLAMA);
        common_log_flush(common_log_main());

        LOG_INF("%s: summary: %zu passed, %zu failed (of %zu)\n", __func__, n_pass, n_fail, models.size());

        return n_fail == 0 ? 0 : 1;
    }

    // single-model mode
    const test_suite suite = run_save_load_tests_for_model(params.model.path, params);
    const bool all_passed = suite.all_passed();
    if (all_passed) {
        LOG("\nAll tests passed.\n");
    }
    return all_passed ? 0 : 1;
}
