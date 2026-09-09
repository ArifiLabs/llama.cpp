#pragma once

// R45 (lane-229): device storage-ID OWNERSHIP for the server context-checkpoint ring.
//
// R44 handed out ids with `1 + (ckpt_storage_next++ % n_ring)` on the premise that the live
// checkpoint list never exceeds n_ctx_checkpoints entries, all among the last n created, so a
// reused id is dead. That premise is false: thinning keeps the first entry and is exempt for
// tool-call anchors (server-context.cpp create_checkpoint), and prefix invalidation erases
// NEWER entries. The counter therefore wraps onto an id a live checkpoint still owns; the new
// save overwrites that checkpoint's device buffers under the same storage key, and restoring
// the old metadata reads the new buffers -- `~llama_io_read_device: memory buffer mismatch`
// when the totals differ, a silent wrong-state restore when they happen to match.
//
// The repair is allocation-side: hand out only an id no LIVE checkpoint owns. Callers invoke
// this AFTER eviction and AFTER appending the new entry (whose flags are still default, so it
// is not yet an owner), which is exactly where create_checkpoint calls it.

#include "common.h"

#include <cstdint>
#include <list>

// Returns a storage id in [1, n_ring] that no live ON_DEVICE checkpoint in `checkpoints` owns,
// advancing `next` past every id it tried. Returns 0 when no id is free: the ring is larger than
// the 255 usable storage ids (id 0 is reserved), or every id is still owned. Callers must treat 0
// as "use a host checkpoint image", never as a usable id.
//
// With ring = n_ctx_checkpoints + 1 and a live list capped at n_ctx_checkpoints entries (one id
// per entry, target and draft sharing the entry's flags), at least one id is always free.
inline uint32_t server_ckpt_storage_alloc(
        const std::list<common_prompt_checkpoint> & checkpoints,
        uint32_t & next,
        uint32_t n_ring) {
    if (n_ring == 0 || n_ring > 255) {
        return 0;
    }

    for (uint32_t attempt = 0; attempt < n_ring; ++attempt) {
        const uint32_t storage = 1 + (next++ % n_ring);
        const llama_state_seq_flags bits = LLAMA_STATE_SEQ_FLAGS_STORAGE(storage);

        bool live = false;
        for (const auto & ckpt : checkpoints) {
            for (const llama_state_seq_flags f : { ckpt.flags_tgt, ckpt.flags_dft }) {
                if ((f & LLAMA_STATE_SEQ_FLAGS_ON_DEVICE) != 0 &&
                    (f & LLAMA_STATE_SEQ_FLAGS_STORAGE_MASK) == bits) {
                    live = true;
                    break;
                }
            }
            if (live) {
                break;
            }
        }

        if (!live) {
            return storage;
        }
    }

    return 0;
}
