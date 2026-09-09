// R45 (lane-229) regression: device storage-ID ownership in the server context-checkpoint ring.
//
// Astra's capacity-32 counterexample, plus the retention shapes that produce it in the server:
// thinning keeps the FIRST entry, tool-call anchors are thinning-exempt, and prefix invalidation
// erases NEWER entries. Under any of those the R44 modulo allocator wraps onto an id a LIVE
// checkpoint still owns, and the newer save overwrites that checkpoint's device buffers.
//
// The test drives BOTH allocators through the same schedules, so one run receipts RED (R44
// violates the ownership invariant) and GREEN (R45 holds it) without rebuilding anything.

#include "server-ckpt-storage.h"

#include <cstdint>
#include <cstdio>
#include <list>
#include <set>
#include <string>
#include <vector>

using ckpt_list = std::list<common_prompt_checkpoint>;
using alloc_fn  = uint32_t (*)(const ckpt_list &, uint32_t &, uint32_t);

// the R44 allocator, kept here only so the regression can go RED on it
static uint32_t alloc_r44(const ckpt_list & /*checkpoints*/, uint32_t & next, uint32_t n_ring) {
    return 1 + (next++ % n_ring);
}

static uint32_t alloc_r45(const ckpt_list & checkpoints, uint32_t & next, uint32_t n_ring) {
    return server_ckpt_storage_alloc(checkpoints, next, n_ring);
}

// a faithful copy of the list bookkeeping in server-context.cpp create_checkpoint()
struct sim_slot {
    ckpt_list checkpoints;
    uint32_t  next = 0;

    int     n_ctx_checkpoints = 32;
    int64_t checkpoint_min_step = 8192;

    alloc_fn alloc = alloc_r45;

    uint32_t create(int id_task, int64_t n_tokens, bool anchor = false) {
        // evict checkpoints within min-step of a previous one, unless created by this task or an anchor
        int64_t last = -1;
        for (auto it = checkpoints.begin(); it != checkpoints.end(); ) {
            if (!it->anchor && it->id_task != id_task && last >= 0 && it->n_tokens <= last + checkpoint_min_step) {
                it = checkpoints.erase(it);
                continue;
            }
            last = it->n_tokens;
            ++it;
        }

        while (checkpoints.size() >= (size_t) n_ctx_checkpoints) {
            checkpoints.erase(checkpoints.begin());
        }

        auto & cur = checkpoints.emplace_back();
        cur.id_task = id_task;
        cur.anchor  = anchor;
        cur.n_tokens = n_tokens;
        cur.pos_min  = (llama_pos) n_tokens;
        cur.pos_max  = (llama_pos) n_tokens;

        const uint32_t n_ring  = (uint32_t) n_ctx_checkpoints + 1;
        const uint32_t storage = alloc(checkpoints, next, n_ring);

        if (storage != 0) {
            const llama_state_seq_flags flags =
                LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY | LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_STORAGE(storage);
            cur.flags_tgt = flags;
            cur.flags_dft = flags;
        }

        return storage;
    }

    // prefix invalidation (server-context.cpp): drop checkpoints past the reuse position
    void invalidate_after(int64_t n_tokens_keep) {
        for (auto it = checkpoints.begin(); it != checkpoints.end(); ) {
            if (it->n_tokens > n_tokens_keep) {
                it = checkpoints.erase(it);
                continue;
            }
            ++it;
        }
    }
};

static int n_fail = 0;

static void check(bool ok, const std::string & what) {
    printf("%-52s %s\n", what.c_str(), ok ? "OK" : "FAIL");
    if (!ok) {
        n_fail++;
    }
}

// THE invariant: no two live ON_DEVICE checkpoints share a storage id, and 0 is never handed out
// as a usable id. A duplicate means one checkpoint's device buffers were overwritten by another.
static bool ids_unique(const ckpt_list & checkpoints, uint32_t * dup_out = nullptr) {
    std::set<uint32_t> seen;
    for (const auto & ckpt : checkpoints) {
        // target and draft of the same entry share the entry's flags, so collect the entry's own
        // ids first and only then check them against the ids other live entries already own
        std::set<uint32_t> own;
        for (const llama_state_seq_flags f : { ckpt.flags_tgt, ckpt.flags_dft }) {
            if ((f & LLAMA_STATE_SEQ_FLAGS_ON_DEVICE) == 0) {
                continue;
            }
            const uint32_t id = (f & LLAMA_STATE_SEQ_FLAGS_STORAGE_MASK) >> LLAMA_STATE_SEQ_FLAGS_STORAGE_SHIFT;
            if (id == 0) {
                if (dup_out) { *dup_out = 0; }
                return false;
            }
            own.insert(id);
        }

        for (const uint32_t id : own) {
            if (!seen.insert(id).second) {
                if (dup_out) { *dup_out = id; }
                return false;
            }
        }
    }
    return true;
}

// Astra's counterexample: retain the first checkpoint, thin every newer one against it, and run
// past one full ring. Each task's checkpoint is thinned by the NEXT task, so the live list stays
// at {first, latest} while the counter cycles through all n_ring ids.
static bool sched_retain_first(alloc_fn alloc, int n_creations, uint32_t * dup_out) {
    sim_slot slot;
    slot.alloc = alloc;

    slot.create(/*id_task =*/ 1, /*n_tokens =*/ 100);
    for (int i = 2; i <= n_creations; ++i) {
        slot.create(i, 100 + i);
        if (!ids_unique(slot.checkpoints, dup_out)) {
            return false;
        }
    }
    return true;
}

// same, with a thinning-exempt tool-call anchor as the retained entry
static bool sched_anchor(alloc_fn alloc, int n_creations, uint32_t * dup_out) {
    sim_slot slot;
    slot.alloc = alloc;

    slot.create(/*id_task =*/ 1, /*n_tokens =*/ 100, /*anchor =*/ true);
    slot.create(/*id_task =*/ 2, /*n_tokens =*/ 101, /*anchor =*/ true);
    for (int i = 3; i <= n_creations; ++i) {
        slot.create(i, 100 + i);
        if (!ids_unique(slot.checkpoints, dup_out)) {
            return false;
        }
    }
    return true;
}

// prefix invalidation erases the NEWER entries, so the surviving old entry's id is behind the counter
static bool sched_invalidation(alloc_fn alloc, int n_creations, uint32_t * dup_out) {
    sim_slot slot;
    slot.alloc = alloc;
    slot.checkpoint_min_step = 0; // no thinning: capacity + invalidation shape the list

    slot.create(1, 100);
    for (int i = 2; i <= n_creations; ++i) {
        slot.create(i, 100 + i);
        if ((i % 4) == 0) {
            slot.invalidate_after(100 + i - 2);
        }
        if (!ids_unique(slot.checkpoints, dup_out)) {
            return false;
        }
    }
    return true;
}

// plain FIFO churn at capacity: the R44 premise actually holds here, both allocators must pass
static bool sched_capacity(alloc_fn alloc, int n_creations, uint32_t * dup_out) {
    sim_slot slot;
    slot.alloc = alloc;
    slot.checkpoint_min_step = 0;

    for (int i = 1; i <= n_creations; ++i) {
        slot.create(i, 100 + (int64_t) i * 10000);
        if (!ids_unique(slot.checkpoints, dup_out)) {
            return false;
        }
    }
    return true;
}

int main() {
    printf("=== R45 context-checkpoint storage ownership ===\n");

    uint32_t dup = 0;

    // --- RED: the R44 allocator must violate ownership on the retention schedules ---
    check(!sched_retain_first(alloc_r44, 40, &dup), "RED  r44 retain-first reuses a live id");
    printf("       (r44 retain-first duplicate id = %u)\n", dup);
    check(!sched_anchor(alloc_r44, 40, &dup), "RED  r44 anchor-retained reuses a live id");
    check(!sched_invalidation(alloc_r44, 80, &dup), "RED  r44 invalidation reuses a live id");

    // --- GREEN: the R45 allocator holds the invariant on every schedule ---
    check(sched_retain_first(alloc_r45, 200, &dup), "GREEN r45 retain-first ids stay distinct");
    check(sched_anchor(alloc_r45, 200, &dup), "GREEN r45 anchor-retained ids stay distinct");
    check(sched_invalidation(alloc_r45, 200, &dup), "GREEN r45 invalidation ids stay distinct");
    check(sched_capacity(alloc_r45, 200, &dup), "GREEN r45 capacity churn ids stay distinct");
    check(sched_capacity(alloc_r44, 200, &dup), "GREEN r44 capacity churn (premise holds here)");

    // --- two sequences: separate slots keep separate counters and lists ---
    {
        sim_slot a; sim_slot b;
        bool ok = true;
        for (int i = 1; i <= 100; ++i) {
            a.create(i, 100 + i);
            b.create(i, 100 + i);
            ok = ok && ids_unique(a.checkpoints) && ids_unique(b.checkpoints);
        }
        check(ok, "GREEN r45 two slots hold independently");
    }

    // --- storage 0 is reserved, and every id stays inside the ring ---
    {
        ckpt_list empty;
        uint32_t next = 0;
        bool ok = true;
        for (int i = 0; i < 100; ++i) {
            const uint32_t id = server_ckpt_storage_alloc(empty, next, 33);
            ok = ok && id >= 1 && id <= 33;
        }
        check(ok, "GREEN r45 ids are in [1, n_ring], never 0");
    }

    // --- capacity guard: a ring past the 8 storage bits must refuse, not alias onto id 0 ---
    {
        ckpt_list empty;
        uint32_t next = 0;
        const bool ok = server_ckpt_storage_alloc(empty, next, 256) == 0 &&
                        server_ckpt_storage_alloc(empty, next, 1000) == 0 &&
                        server_ckpt_storage_alloc(empty, next, 255) != 0;
        check(ok, "GREEN r45 refuses a ring larger than 255 ids");
    }

    // --- exhausted ring: every id owned -> 0 (host image), never a live id ---
    {
        uint32_t next = 0;
        ckpt_list all;
        for (uint32_t id = 1; id <= 5; ++id) {
            auto & c = all.emplace_back();
            c.flags_tgt = LLAMA_STATE_SEQ_FLAGS_ON_DEVICE | LLAMA_STATE_SEQ_FLAGS_STORAGE(id);
            c.flags_dft = c.flags_tgt;
        }
        check(server_ckpt_storage_alloc(all, next, 5) == 0, "GREEN r45 fully owned ring returns 0");
    }

    printf("=== %s (%d failure%s) ===\n", n_fail == 0 ? "PASS" : "FAIL", n_fail, n_fail == 1 ? "" : "s");
    return n_fail == 0 ? 0 : 1;
}
