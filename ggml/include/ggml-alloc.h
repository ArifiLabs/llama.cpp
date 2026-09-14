#pragma once

#include "ggml.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct ggml_backend_buffer_type * ggml_backend_buffer_type_t;
typedef struct      ggml_backend_buffer * ggml_backend_buffer_t;
typedef struct             ggml_backend * ggml_backend_t;

// Tensor allocator
struct ggml_tallocr {
    ggml_backend_buffer_t buffer;
    void * base;
    size_t alignment;
    size_t offset;
};

GGML_API struct ggml_tallocr ggml_tallocr_new(ggml_backend_buffer_t buffer);
GGML_API enum ggml_status    ggml_tallocr_alloc(struct ggml_tallocr * talloc, struct ggml_tensor * tensor);

// Graph allocator
/*
  Example usage:
    ggml_gallocr_t galloc = ggml_gallocr_new(ggml_backend_cpu_buffer_type());

    // optional: create a worst-case graph and reserve the buffers to avoid reallocations
    ggml_gallocr_reserve(galloc, build_graph(max_batch));

    // allocate the graph
    struct ggml_cgraph * graph = build_graph(batch);
    ggml_gallocr_alloc_graph(galloc, graph);

    printf("compute buffer size: %zu bytes\n", ggml_gallocr_get_buffer_size(galloc, 0));

    // evaluate the graph
    ggml_backend_graph_compute(backend, graph);
*/

// special tensor flags for use with the graph allocator:
//   ggml_set_input(): all input tensors are allocated at the beginning of the graph in non-overlapping addresses
//   ggml_set_output(): output tensors are never freed and never overwritten

typedef struct ggml_gallocr * ggml_gallocr_t;

GGML_API ggml_gallocr_t ggml_gallocr_new(ggml_backend_buffer_type_t buft);
GGML_API ggml_gallocr_t ggml_gallocr_new_n(ggml_backend_buffer_type_t * bufts, int n_bufs);
GGML_API void           ggml_gallocr_free(ggml_gallocr_t galloc);

// pre-allocate buffers from a measure graph - does not allocate or modify the graph
// call with a worst-case graph to avoid buffer reallocations
// not strictly required for single buffer usage: ggml_gallocr_alloc_graph will reallocate the buffers automatically if needed
// returns false if the buffer allocation failed
// ggml_gallocr_resrve_n_size writes the buffer sizes per galloc buffer that would be allocated by ggml_gallocr_reserve_n to sizes
GGML_API bool ggml_gallocr_reserve(ggml_gallocr_t galloc, struct ggml_cgraph * graph);
GGML_API void ggml_gallocr_reserve_n_size(
    ggml_gallocr_t galloc,
    struct ggml_cgraph * graph,
    const int * node_buffer_ids,
    const int * leaf_buffer_ids,
    size_t * sizes);
GGML_API bool ggml_gallocr_reserve_n(
    ggml_gallocr_t galloc,
    struct ggml_cgraph * graph,
    const int * node_buffer_ids,
    const int * leaf_buffer_ids);

// automatic reallocation if the topology changes when using a single buffer
// returns false if using multiple buffers and a re-allocation is needed (call ggml_gallocr_reserve_n first to set the node buffers)
GGML_API bool ggml_gallocr_alloc_graph(ggml_gallocr_t galloc, struct ggml_cgraph * graph);

GGML_API size_t ggml_gallocr_get_buffer_size(ggml_gallocr_t galloc, int buffer_id);

// Utils
// Create a buffer and allocate all the tensors in a ggml_context
// ggml_backend_alloc_ctx_tensors_from_buft_size returns the size of the buffer that would be allocated by ggml_backend_alloc_ctx_tensors_from_buft
// ggml_backend_alloc_ctx_tensors_from_buft returns NULL on failure or if all tensors in ctx are already allocated or zero-sized
GGML_API size_t                       ggml_backend_alloc_ctx_tensors_from_buft_size(struct ggml_context * ctx, ggml_backend_buffer_type_t buft);
GGML_API struct ggml_backend_buffer * ggml_backend_alloc_ctx_tensors_from_buft(struct ggml_context * ctx, ggml_backend_buffer_type_t buft);
GGML_API struct ggml_backend_buffer * ggml_backend_alloc_ctx_tensors(struct ggml_context * ctx, ggml_backend_t backend);

// R46b B7b — loader-wide allocation transaction across several (context, buffer type) groups.
//
// ggml_backend_alloc_ctx_tensors_from_buft() is a transaction over ONE group: it decides the whole
// set of buffers for one (ctx, buft) pair before allocating any of them. A model load issues many
// such calls, and each completed call has already allocated before the next group's sizes are even
// known. This object is the owner that spans them:
//
//   init()   -> add(ctx, buft) for every group the load intends to allocate   (census, no allocation)
//   commit()                                                                  (reserve, no allocation)
//   alloc(ctx, buft) per group                                                (consume reservations)
//   free()                                                                    (release what was not consumed)
//
// commit() opens one backend batch plan per buffer type, covering every group of that buffer type
// concatenated in add() order. If ANY buffer type cannot place its whole share, commit() releases
// every plan it opened and returns false, so the load fails before the first plan-owned driver
// allocation - no earlier group has been placed. Buffer types that do not implement planning answer
// UNSUPPORTED and keep their current unplanned behaviour exactly, which is what leaves CPU and every
// non-planning backend in a mixed-backend load semantically unchanged.
//
// free() must be called on every path, including exceptions in the caller's pass 2; it releases the
// reservations of every group that was never consumed, exactly once. Buffers already returned by
// alloc() are owned by the caller and are not touched.
//
// Group ownership states: censused (after add), reserved (after commit), consumed (after a
// successful alloc), released (after free). A group's reservation is charged exactly once by
// commit() and discharged exactly once - either by alloc() adopting it into a buffer, or by free().
//
// SCOPE: this is the model TENSOR load boundary. KV cache and compute buffers are allocated later,
// during context construction, and are not part of this transaction.
struct ggml_backend_alloc_plan;
typedef struct ggml_backend_alloc_plan * ggml_backend_alloc_plan_t;

// returns NULL on allocation failure
GGML_API ggml_backend_alloc_plan_t ggml_backend_alloc_plan_init(void);
// census one group. Returns true if the group joined the transaction. False means it did not (a
// buffer type that cannot be planned by this path, or a context with nothing left to allocate) or
// that the transaction failed; either way ggml_backend_alloc_plan_alloc() still handles the group.
GGML_API bool                         ggml_backend_alloc_plan_add(ggml_backend_alloc_plan_t plan, struct ggml_context * ctx, ggml_backend_buffer_type_t buft);
// decide and reserve every censused group atomically. False = the load must not start.
GGML_API bool                         ggml_backend_alloc_plan_commit(ggml_backend_alloc_plan_t plan);
// allocate one group, consuming its reservation. Groups that never joined the transaction fall
// through to ggml_backend_alloc_ctx_tensors_from_buft(). NULL on failure, exactly as that function.
GGML_API struct ggml_backend_buffer * ggml_backend_alloc_plan_alloc(ggml_backend_alloc_plan_t plan, struct ggml_context * ctx, ggml_backend_buffer_type_t buft);
// release every unconsumed reservation and destroy the transaction. Safe on NULL and on a
// transaction that was never committed.
GGML_API void                         ggml_backend_alloc_plan_free(ggml_backend_alloc_plan_t plan);

#ifdef  __cplusplus
}
#endif
