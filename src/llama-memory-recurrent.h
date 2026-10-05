#pragma once

#include "llama-batch.h"
#include "llama-graph.h"
#include "llama-memory.h"

#include <map>
#include <set>
#include <vector>

//
// llama_memory_recurrent
//

// TODO: extract the cache state used for graph computation into llama_memory_recurrent_context_i
//       see the implementation of llama_kv_cache_context_i for an example how to do it
class llama_memory_recurrent : public llama_memory_i {
public:
    llama_memory_recurrent(
            const llama_model & model,
                    ggml_type   type_r,
                    ggml_type   type_s,
                         bool   offload,
                     uint32_t   mem_size,
                     uint32_t   n_seq_max,
                     uint32_t   n_rs_seq,
        const layer_filter_cb & filter);

    ~llama_memory_recurrent() = default;

    //
    // llama_memory_i
    //

    llama_memory_context_ptr init_batch(
            llama_batch_allocr & balloc,
            uint32_t n_ubatch,
            bool embd_all) override;

    llama_memory_context_ptr init_full() override;

    llama_memory_context_ptr init_update(llama_context * lctx, bool optimize) override;

    void clear(bool data) override;

    bool seq_rm  (llama_seq_id seq_id,                              llama_pos p0, llama_pos p1) override;
    void seq_cp  (llama_seq_id seq_id_src, llama_seq_id seq_id_dst, llama_pos p0, llama_pos p1) override;
    void seq_keep(llama_seq_id seq_id)                                                          override;
    void seq_add (llama_seq_id seq_id,                              llama_pos p0, llama_pos p1, llama_pos shift) override;
    void seq_div (llama_seq_id seq_id,                              llama_pos p0, llama_pos p1, int d) override;

    llama_pos seq_pos_min(llama_seq_id seq_id) const override;
    llama_pos seq_pos_max(llama_seq_id seq_id) const override;

    std::map<ggml_backend_buffer_type_t, size_t> memory_breakdown() const override;

    bool prepare(const std::vector<llama_ubatch> & ubatches);

    // find a contiguous slot of memory cells and emplace the ubatch there
    bool find_slot(const llama_ubatch & ubatch);

    bool get_can_shift() const override;

    // ring-repair 2026-08-25: FNV hash of the seq's tail-cell rows (r_l+s_l) at the given
    // plane, read host-side off the backend buffer. 0 if no tail. Debug probes only.
    uint64_t debug_hash_row(llama_seq_id seq_id, uint32_t plane) const;

    // state write/load

    void state_write(llama_io_write_i & io, llama_seq_id seq_id = -1, llama_state_seq_flags flags = 0) const override;
    void state_read (llama_io_read_i  & io, llama_seq_id seq_id = -1, llama_state_seq_flags flags = 0) override;

    uint32_t head = 0; // the location where the batch will be placed in the cache (see find_slot())
    uint32_t size = 0; // total number of cells, shared across all sequences
    uint32_t used = 0; // used cells (i.e. at least one seq_id)

    // number of recurrent-state snapshots per seq for rollback; tensors are widened to (1 + n_rs_seq) groups
    uint32_t n_rs_seq = 0;

    // per-seq rollback index
    std::vector<uint32_t> rs_idx;

    void set_rs_idx(llama_seq_id seq_id, uint32_t idx);

    // DEEP-SLOT AGE FIX (ring-repair 2026-08-24): after a partial rollback of r tokens the
    // surviving bank slots keep ages relative to the OLD sequence end; the next ubatch's
    // shift-by-n relabel must become shift-by-(n - r) or deep restores read wrong-age states
    // (measured: DFlash2 depth 5/7 repetition loops, depth 3 clean). rs_pending accumulates r
    // per seq at seq_rm; find_slot snapshots it into rs_shift_cur and clears it; the graph
    // builders subtract it from their shift distance.
    std::vector<uint32_t> rs_pending;
    uint32_t rs_shift_cur = 0;

    // R1 BANK PLANE AS RUNTIME INDEX (lane-176). The SSM bank stops being age-shifted every
    // ubatch and becomes position-labelled: state@g lives at physical plane (-g) mod K. That
    // deletes the K-1 shift copies per layer per ubatch (the lane-168 prize, 2.097 MB/layer/token).
    //
    // The difference from lane-175's C4, and the whole point of this lane: C4 expressed the plane
    // as a CONSTANT ggml view offset baked at graph-build time, which cannot survive graph reuse
    // (F-146). R1 passes it as a RUNTIME INDEX TENSOR instead - exactly how the KV cache writes its
    // per-token row (llama-kv-cache.cpp ggml_set_rows(k, k_cur, k_idxs)), whose can_reuse compares
    // only the index tensor's SHAPE. So the copies disappear AND the graph stays reusable.
    //
    // Session-level, never per-ubatch: the physical bank layout differs between R1 and the shipped
    // COPY scheme, so it must not flip mid-run. LLAMA_RS_R1_OFF=1 returns the SAME binary to the
    // shipped path, which is what makes the A/B interleavable on one build (F-145).
    //
    // SCOPE BOUND, deliberate: n_seq_max == 1. With more sequences find_slot's gather-and-reorder
    // moves cell METADATA between indices while the deep bank planes stay put, so a cell's planes
    // > 0 can hold the previous occupant's states; and rs_z is one scalar that cannot name a
    // per-cell read plane. --parallel 1 is the seated configuration and carries 100% of the prize.
    // ponytail: single-seq gate; lift it with a per-cell rs_z and owner-aware deep-plane
    // invalidation if multi-seq recurrent serving ever matters.
    //
    // SCOPE BOUND 2: only the SSM bank rotates. The conv bank keeps the shipped LOGICAL layout,
    // because its F-136 root fix already lands all K planes in one contiguous write - it has no
    // per-plane shift copies to delete, so rotating it would buy nothing and cost an indirection.
    bool rs_r1 = false;

    // lane-298 deferred commit + replay (LLAMA_GDN_REPLAY=1, needs rs_r1): the SSM bank holds only the committed state;
    // each ubatch replays the accepted tokens of the previous one from a per-layer k|v|g|beta log (l_l, 2 halves of
    // rs_planes() rows) and logs its own. Measured slice: grep-10051756 (4.64 ms/verify on qwen4exp).
    bool rs_replay = false;
    std::vector<ggml_tensor *> l_l;

    // K = number of bank planes.
    // lane-298 replay adds one plane so the committed state (age n) never aliases the final state (age 0) for n == K
    uint32_t rs_planes() const { return n_rs_seq + 1 + (rs_replay ? 1u : 0u); }

    // physical plane holding the state that is `age` tokens older than `anchor`.
    uint32_t rs_plane(llama_pos anchor, int64_t age) const {
        const int64_t K = (int64_t) rs_planes();
        return (uint32_t) (((age - (int64_t) anchor) % K + K) % K);
    }

    // computed before each graph build
    uint32_t n = 0;

    // first zero-ed state
    int32_t rs_z = -1;

    // TODO: optimize for recurrent state needs
    struct mem_cell {
        llama_pos pos  = -1;
        int32_t   src  = -1; // used to know where states should be copied from
        int32_t   src0 = -1; // like src, but only used when setting the inputs (allowing to copy once)
        int32_t   tail = -1;

        // R1 ring anchors (lane-176, carried over VERBATIM from lane-175's C4 - the cell-layer
        // bookkeeping is unchanged and ring_anchor_sim.py already gates it).
        //
        //   pos_bank      = absolute pos of the NEWEST state currently in this cell's bank
        //   pos_bank_prev = its value on entry to the current find_slot (what reads resolve against)
        //
        // NEITHER is derived from `pos`. A single `pos_prev = cell.pos` anchor is DEFECTIVE:
        // seq_rm rewinds cell.pos to p0-1 on a partial rollback while rs_idx already carries the
        // same distance, so a pos-derived anchor reads plane (2r - P) mod K instead of
        // (r - P) mod K - off by exactly the rollback distance.
        llama_pos pos_bank      = -1;
        llama_pos pos_bank_prev = -1;

        // lane-298 deferred commit + replay (LLAMA_GDN_REPLAY): n/par = tokens logged by the newest ubatch and the log
        // half they went to (n = 0: nothing pending, the newest state is in the bank); *_prev = values on entry to the
        // current find_slot (what this ubatch reads); P = tokens this ubatch replays; age = this ubatch's length.
        struct rep_t { int32_t n = 0, par = 0, n_prev = 0, par_prev = 0, P = 0, age = 0; } rep;

        std::set<llama_seq_id> seq_id;

        bool has_seq_id(const llama_seq_id & id) const {
            return seq_id.find(id) != seq_id.end();
        }

        bool is_empty() const {
            return seq_id.empty();
        }

        bool is_same_seq(const mem_cell & other) const {
            return seq_id == other.seq_id;
        }
    };

    std::vector<mem_cell> cells;

    // per layer
    std::vector<ggml_tensor *> r_l;
    std::vector<ggml_tensor *> s_l;
    // a second conv history that must stay replicated across devices, so it cannot share the r row
    std::vector<ggml_tensor *> p_l;

private:
    //const llama_model & model;
    const llama_hparams & hparams;

    const uint32_t n_seq_max = 1;

    // ggml contexts for the KV cache along with the allocated backend buffers:
    std::vector<std::pair<ggml_context_ptr, ggml_backend_buffer_ptr>> ctxs_bufs;

    size_t total_size() const;

    size_t size_r_bytes() const;
    size_t size_s_bytes() const;
    size_t size_p_bytes() const;

    void state_write_meta(llama_io_write_i & io, const std::vector<std::pair<uint32_t, uint32_t>> & cell_ranges, llama_seq_id seq_id = -1) const;

    void state_write_data(llama_io_write_i & io, const std::vector<std::pair<uint32_t, uint32_t>> & cell_ranges) const;

public:
    // F-136 probe: hash the device r/s rows of seq_id's tail cell (bank 0) + report wiring
    uint64_t debug_rs_hash(llama_seq_id seq_id, int32_t * tail_out, int32_t * src0_out, llama_pos * pos_out) const;

    bool state_read_meta(llama_io_read_i & io, uint32_t cell_count, llama_seq_id dest_seq_id = -1);
    bool state_read_data(llama_io_read_i & io, uint32_t cell_count);
};

class llama_memory_recurrent_context : public llama_memory_context_i {
public:
    // used for errors
    llama_memory_recurrent_context(llama_memory_status status);

    // used to create a full-cache or update context
    llama_memory_recurrent_context(
            llama_memory_recurrent * mem);

    // used to create a batch processing context from a batch
    llama_memory_recurrent_context(
            llama_memory_recurrent * mem,
            std::vector<llama_ubatch> ubatches);

    virtual ~llama_memory_recurrent_context();

    //
    // llama_memory_context_i
    //

    bool next()  override;
    bool apply() override;

    llama_memory_status  get_status() const override;
    const llama_ubatch & get_ubatch() const override;

    //
    // llama_memory_recurrent_context specific API
    //

    uint32_t get_n_rs() const;
    uint32_t get_head() const;
    int32_t  get_rs_z() const;
    uint32_t get_size() const;

    ggml_tensor * get_r_l(int32_t il) const;
    ggml_tensor * get_s_l(int32_t il) const;
    ggml_tensor * get_p_l(int32_t il) const;

    int32_t s_copy(int i) const;

    // R1 ring (lane-176).
    //
    // s_copy2() is s_copy() plus the SSM bank's rotated read row, computed in the SAME call.
    // It must be one call: s_copy() CONSUMES the rollback index (mem->rs_idx[seq] = 0), so asking
    // for the two rows in two calls would read idx once and 0 the second time.
    // *bank_row receives the row the ROTATED ssm bank must be gathered from; it equals the logical
    // return value whenever R1 is inactive.
    int32_t s_copy2(int i, int32_t * bank_row) const;

    // Physical row the ssm bank's snapshot `age` must be WRITTEN to this ubatch, or -1 when R1 is
    // inactive. Anchored on pos_bank (the anchor AFTER find_slot advanced it), where the reads are
    // anchored on pos_bank_prev.
    int32_t s_wrow(int64_t age) const;

    // lane-298 replay: session flag, {P, read parity} for the ctl input, the committed-state write row, per-layer log
    bool          get_rs_replay() const;
    int32_t       rep_ctl(int i) const;
    int32_t       rep_crow() const;
    ggml_tensor * get_l_l(int32_t il) const;

    // Is the R1 rotated SSM bank active for this session?
    bool get_rs_r1() const;

    // K = number of bank planes (n_rs_seq + 1).
    uint32_t get_n_rs_planes() const;

    // The rotated twin of get_rs_z(): the in-graph zero for a fresh sequence must land on the plane
    // the ssm gather will READ, not on plane 0. Equals get_rs_z() when R1 is inactive, and -1
    // whenever get_rs_z() is -1 - which is every steady-decode ubatch (rs_z is recomputed per
    // ubatch in find_slot and is -1 while every used cell is its own source).
    int32_t get_rs_z_bank() const;

    // R1 write anchor (the absolute pos of the newest state this ubatch produces).
    // MUST return a constant when R1 is inactive - it is a can_reuse input elsewhere in the tree's
    // history, and a live pos on the shipped path would disable graph reuse there and silently
    // poison every A/B (F-146's binding_fix, learned the hard way in lane-175).
    llama_pos get_rs_pos() const;

    // consumed rollback distance for the current ubatch (see rs_pending in llama_memory_recurrent)
    uint32_t get_rs_shift() const;

private:
    const llama_memory_status status;

    llama_memory_recurrent * mem;

    size_t i_next = 0;

    std::vector<llama_ubatch> ubatches;

    //
    // data needed for building the compute graph for the current ubatch:
    // TODO: extract all the state like `head` and `n` here
    //

    const bool is_full = false;
};
