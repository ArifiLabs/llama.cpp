#include "ggml-alloc.h"
#include "../ggml/src/ggml-backend-impl.h"
#include "ggml-cpp.h"
#include "../ggml/src/ggml-impl.h"
#include "ggml.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <vector>

//
// dummy backend with configurable max_buffer_size, tracks allocations

uint8_t * const alloc_base = (uint8_t *) 16;

struct dummy_backend_context {
    size_t max_buffer_size = 64;
    size_t alignment       = 8;
    // R46b B7b-R2: batch planning seam. `plan_status` is what plan_begin answers; the counters
    // record what the caller in ggml-alloc.c did with that answer.
    ggml_backend_plan_status plan_status      = GGML_BACKEND_PLAN_FEASIBLE;
    size_t                   alloc_calls      = 0;   // every buffer allocation, planned or not
    size_t                   plan_begin_calls = 0;
    size_t                   plan_free_calls  = 0;
    size_t                   plan_n           = 0;   // buffers in the last plan_begin

    // R46b B7b loader: a capacity ledger, so a transaction that spans several groups of this buffer
    // type can be observed admitting or refusing. `budget` is what this "device" can hold; `reserved`
    // is what OPEN plans hold and have not yet handed to a buffer. Live buffer bytes come from
    // allocated_total(), so a freed buffer returns capacity without any bookkeeping here.
    size_t              budget          = SIZE_MAX;   // default: unlimited, every older test unchanged
    size_t              reserved        = 0;
    size_t              released        = 0;          // bytes plan_free gave back (never consumed)
    size_t              fail_alloc_at   = SIZE_MAX;   // plan_alloc_buffer call index that returns NULL
    size_t              plan_alloc_call = 0;
    std::vector<size_t> plan_alloc_order;             // plan entry index of each plan_alloc_buffer call

    // R46b B7c: a SECOND pool this buffer type may spill into when the first is full, and the two
    // things that keep that spill bounded. Defaults leave the backend exactly as B7b left it:
    // host_budget 0 means there is nowhere to spill, so every older test takes the same decisions.
    size_t host_budget     = 0;          // capacity of the spill pool
    size_t staging_reserve = 0;          // carved out of host_budget before any weight is admitted
    size_t host_split_max  = SIZE_MAX;   // bound on what ONE plan may spill
    size_t host_reserved   = 0;          // open plans' spill bytes
    size_t host_consumed   = 0;          // spill bytes now owned by live buffers
    size_t host_released   = 0;          // spill bytes plan_free gave back
    std::vector<ggml_backend_buffer_t> host_buffers;   // which live buffers are on the spill pool

    ggml_backend_buffer_i              buffer_interface;
    ggml_backend_device                device;
    ggml_backend                       backend;
    std::vector<ggml_backend_buffer_t> buffers;

    // Live bytes in the FIRST pool only: spill buffers are counted by host_consumed instead, so one
    // buffer is never charged to both pools.
    size_t allocated_total() const {
        size_t n = 0;
        for (ggml_backend_buffer_t buf : buffers) {
            n += ggml_backend_buffer_get_size(buf);
        }
        return n - host_consumed;
    }
};

// ggml_backend_buffer_type interface

static const char * dummy_backend_buffer_type_get_name(ggml_backend_buffer_type_t) {
    return "dummy_buffer_type";
}

static ggml_backend_buffer_t dummy_backend_buffer_type_alloc_buffer(ggml_backend_buffer_type_t buft, size_t size) {
    dummy_backend_context * ctx    = (dummy_backend_context *) buft->context;
    ctx->alloc_calls++;
    ggml_backend_buffer_t & buffer = ctx->buffers.emplace_back();
    buffer                         = ggml_backend_buffer_init(buft, ctx->buffer_interface, ctx, size);
    return buffer;
}

static size_t dummy_backend_buffer_type_get_alignment(ggml_backend_buffer_type_t buft) {
    dummy_backend_context * ctx = (dummy_backend_context *) buft->context;
    return ctx->alignment;
}

static size_t dummy_backend_buffer_type_get_max_size(ggml_backend_buffer_type_t buft) {
    dummy_backend_context * ctx = (dummy_backend_context *) buft->context;
    return ctx->max_buffer_size;
}

static bool dummy_backend_buffer_type_is_host(ggml_backend_buffer_type_t) {
    return true;
}

// R46b B7b-R2 batch planning hooks. The plan holds nothing but the sizes it was handed; this
// backend has no capacity to reserve. Its only job is to answer with a configured status so the
// CALLER's contract in ggml_backend_alloc_ctx_tensors_from_buft() can be tested.
struct ggml_backend_buffer_type_plan {
    std::vector<size_t> sizes;
    std::vector<bool>   consumed;   // R46b B7b loader: exactly-once adoption is asserted, not assumed
    std::vector<bool>   host;       // R46b B7c: this entry was placed on the spill pool
    size_t              reserved = 0;
};

static ggml_backend_buffer_type_plan_t dummy_backend_buffer_type_plan_begin(
        ggml_backend_buffer_type_t buft, const size_t * sizes, size_t n, ggml_backend_plan_status * status) {
    dummy_backend_context * ctx = (dummy_backend_context *) buft->context;
    ctx->plan_begin_calls++;
    ctx->plan_n = n;
    *status     = ctx->plan_status;
    if (ctx->plan_status != GGML_BACKEND_PLAN_FEASIBLE) {
        return nullptr;
    }

    // R46b B7b loader: decide the WHOLE batch against this buffer type's remaining capacity, which
    // already counts every open plan's reservation. Overflow is refused, never wrapped.
    size_t need = 0;
    for (size_t i = 0; i < n; i++) {
        if (sizes[i] > SIZE_MAX - need) {
            *status = GGML_BACKEND_PLAN_INFEASIBLE;
            return nullptr;
        }
        need += sizes[i];
    }
    // R46b B7c: place the batch, first pool first, in plan order. What does not fit the first pool
    // spills onto the second - but only within BOTH the free bytes left after the staging reserve
    // and the declared split bound. With host_budget 0 (the default) the spill capacity is 0, so
    // this is the B7b decision unchanged: the batch fits the first pool or it is refused.
    const size_t in_use = ctx->reserved + ctx->allocated_total();
    size_t dev_free = ctx->budget > in_use ? ctx->budget - in_use : 0;

    const size_t host_in_use = ctx->staging_reserve + ctx->host_reserved + ctx->host_consumed;
    const size_t host_free   = ctx->host_budget > host_in_use ? ctx->host_budget - host_in_use : 0;
    size_t host_left = std::min(ctx->host_split_max, host_free);

    std::vector<bool> host(n, false);
    size_t dev_need = 0, host_need = 0;
    for (size_t i = 0; i < n; i++) {
        if (sizes[i] <= dev_free) {
            dev_free  -= sizes[i];
            dev_need  += sizes[i];
        } else if (sizes[i] <= host_left) {
            host_left -= sizes[i];
            host_need += sizes[i];
            host[i]    = true;
        } else {
            *status = GGML_BACKEND_PLAN_INFEASIBLE;
            return nullptr;
        }
    }
    ctx->reserved      += dev_need;
    ctx->host_reserved += host_need;

    ggml_backend_buffer_type_plan_t plan = new ggml_backend_buffer_type_plan;
    plan->sizes.assign(sizes, sizes + n);
    plan->consumed.assign(n, false);
    plan->host     = host;
    plan->reserved = need;
    return plan;
}

static ggml_backend_buffer_t dummy_backend_buffer_type_plan_alloc_buffer(
        ggml_backend_buffer_type_t buft, ggml_backend_buffer_type_plan_t plan, size_t i) {
    dummy_backend_context * ctx = (dummy_backend_context *) buft->context;
    GGML_ASSERT(i < plan->sizes.size());
    GGML_ASSERT(!plan->consumed[i]);   // an entry must never be adopted twice
    ctx->plan_alloc_order.push_back(i);
    if (ctx->plan_alloc_call++ == ctx->fail_alloc_at) {
        return nullptr;   // the entry stays unconsumed and must be released by plan_free
    }
    // hand the reservation to the buffer: reserved drops, allocated_total() rises by the same bytes
    plan->consumed[i] = true;
    plan->reserved   -= plan->sizes[i];
    if (plan->host[i]) {
        ctx->host_reserved -= plan->sizes[i];
        ctx->host_consumed += plan->sizes[i];
    } else {
        ctx->reserved -= plan->sizes[i];
    }
    ggml_backend_buffer_t buf = dummy_backend_buffer_type_alloc_buffer(buft, plan->sizes[i]);
    if (plan->host[i]) {
        ctx->host_buffers.push_back(buf);   // so freeing it returns spill capacity, not first-pool
    }
    return buf;
}

static void dummy_backend_buffer_type_plan_free(ggml_backend_buffer_type_t buft, ggml_backend_buffer_type_plan_t plan) {
    dummy_backend_context * ctx = (dummy_backend_context *) buft->context;
    ctx->plan_free_calls++;
    // release exactly what was never consumed
    for (size_t i = 0; i < plan->sizes.size(); i++) {
        if (!plan->consumed[i]) {
            if (plan->host[i]) {
                ctx->host_reserved -= plan->sizes[i];
                ctx->host_released += plan->sizes[i];
            } else {
                ctx->reserved -= plan->sizes[i];
                ctx->released += plan->sizes[i];
            }
            plan->reserved -= plan->sizes[i];
        }
    }
    GGML_ASSERT(plan->reserved == 0);
    delete plan;
}

// ggml_backend_buffer interface

static void dummy_backend_buffer_free_buffer(ggml_backend_buffer_t buffer) {
    dummy_backend_context * ctx = (dummy_backend_context *) buffer->context;

    auto h = std::find(ctx->host_buffers.begin(), ctx->host_buffers.end(), buffer);
    if (h != ctx->host_buffers.end()) {
        ctx->host_consumed -= ggml_backend_buffer_get_size(buffer);
        ctx->host_buffers.erase(h);
    }

    auto i = std::find(ctx->buffers.begin(), ctx->buffers.end(), buffer);
    GGML_ASSERT(i != ctx->buffers.end());
    ctx->buffers.erase(i);
}

static void * dummy_backend_buffer_get_base(ggml_backend_buffer_t) {
    return alloc_base;
}

static ggml_status dummy_backend_buffer_init_tensor(ggml_backend_buffer_t, ggml_tensor *) {
    return GGML_STATUS_SUCCESS;
}

static void dummy_backend_buffer_memset_tensor(ggml_backend_buffer_t, ggml_tensor *, uint8_t, size_t, size_t) {}

static void dummy_backend_buffer_set_tensor(ggml_backend_buffer_t, ggml_tensor *, const void *, size_t, size_t) {}

static void dummy_backend_buffer_get_tensor(ggml_backend_buffer_t, const ggml_tensor *, void *, size_t, size_t) {}

static void dummy_backend_buffer_clear(ggml_backend_buffer_t, uint8_t) {}

// ggml_backend_device interface

static enum ggml_backend_dev_type dummy_backend_device_get_type(ggml_backend_dev_t) {
    return GGML_BACKEND_DEVICE_TYPE_CPU;
}

static bool dummy_backend_device_supports_op(ggml_backend_dev_t, const ggml_tensor *) {
    return true;
}

static bool dummy_backend_device_supports_buft(ggml_backend_dev_t device, ggml_backend_buffer_type_t buft) {
    return device->context == buft->context;
}

// ggml_backend interface

static const char * dummy_backend_get_name(ggml_backend_t) {
    return "dummy_backend";
}

// dummy_backend

struct dummy_backend {
    std::unique_ptr<dummy_backend_context> context;
    ggml_backend_buffer_type               buffer_type;
};

static dummy_backend dummy_backend_init(size_t max_buffer_size, size_t alignment = 8) {
    dummy_backend b{};
    b.context                  = std::make_unique<dummy_backend_context>();
    b.context->alignment       = alignment;
    b.context->max_buffer_size = max_buffer_size;

    b.context->buffer_interface.free_buffer   = dummy_backend_buffer_free_buffer;
    b.context->buffer_interface.get_base      = dummy_backend_buffer_get_base;
    b.context->buffer_interface.init_tensor   = dummy_backend_buffer_init_tensor;
    b.context->buffer_interface.memset_tensor = dummy_backend_buffer_memset_tensor;
    b.context->buffer_interface.set_tensor    = dummy_backend_buffer_set_tensor;
    b.context->buffer_interface.get_tensor    = dummy_backend_buffer_get_tensor;
    b.context->buffer_interface.clear         = dummy_backend_buffer_clear;

    b.context->device.context             = b.context.get();
    b.context->device.iface.get_type      = dummy_backend_device_get_type;
    b.context->device.iface.supports_op   = dummy_backend_device_supports_op;
    b.context->device.iface.supports_buft = dummy_backend_device_supports_buft;

    b.context->backend.context        = b.context.get();
    b.context->backend.device         = &b.context->device;
    b.context->backend.iface.get_name = dummy_backend_get_name;

    b.buffer_type.device              = &b.context->device;
    b.buffer_type.context             = b.context.get();
    b.buffer_type.iface.get_name      = dummy_backend_buffer_type_get_name;
    b.buffer_type.iface.alloc_buffer  = dummy_backend_buffer_type_alloc_buffer;
    b.buffer_type.iface.get_alignment = dummy_backend_buffer_type_get_alignment;
    b.buffer_type.iface.get_max_size  = dummy_backend_buffer_type_get_max_size;
    b.buffer_type.iface.is_host       = dummy_backend_buffer_type_is_host;
    return b;
}

//
// test utilities

struct test_context_with_graph {
    ggml_context *   ctx;
    ggml_cgraph *    graph;
    ggml_context_ptr ctx_ptr;
};

static test_context_with_graph make_context() {
    ggml_init_params params{};
    params.mem_size = 48 * ggml_tensor_overhead() + ggml_graph_overhead();
    params.no_alloc = true;

    ggml_context *   ctx     = ggml_init(params);
    ggml_context_ptr ctx_ptr = ggml_context_ptr(ctx);
    ggml_cgraph *    graph   = ggml_new_graph(ctx);
    return { ctx, graph, std::move(ctx_ptr) };
}

static ggml_tensor * make_input_1d(ggml_context * ctx, int64_t n_elements) {
    ggml_tensor * t = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_elements);
    ggml_set_input(t);
    return t;
}

static ggml_tensor * make_input_with_size(ggml_context * ctx, size_t size_bytes) {
    GGML_ASSERT(size_bytes % 4 == 0);
    return make_input_1d(ctx, size_bytes / 4);
}

static void assign_names(ggml_context * ctx, const char * prefix = "x") {
    int i = 0;
    for (ggml_tensor * t = ggml_get_first_tensor(ctx); t; t = ggml_get_next_tensor(ctx, t)) {
        ggml_format_name(t, "%s%d", prefix, i++);
    }
}

static int get_leaf_id(ggml_cgraph * graph, const char * tensor_name) {
    for (int i = 0; i < graph->n_leafs; ++i) {
        if (strncmp(graph->leafs[i]->name, tensor_name, GGML_MAX_NAME) == 0) {
            return i;
        }
    }
    fprintf(stderr, "leaf not found: %s\n", tensor_name);
    return -1;
}

static int get_node_id(ggml_cgraph * graph, const char * tensor_name) {
    for (int i = 0; i < graph->n_nodes; ++i) {
        if (strncmp(graph->nodes[i]->name, tensor_name, GGML_MAX_NAME) == 0) {
            return i;
        }
    }
    fprintf(stderr, "node not found: %s", tensor_name);
    return -1;
}

static ggml_gallocr_ptr allocate_graph(ggml_cgraph * graph, ggml_tensor * out, ggml_backend_buffer_type_t buft) {
    ggml_set_output(out);
    ggml_build_forward_expand(graph, out);

    ggml_gallocr_ptr galloc = ggml_gallocr_ptr(ggml_gallocr_new(buft));
    bool             result = ggml_gallocr_alloc_graph(galloc.get(), graph);
    GGML_ASSERT(result);
    return galloc;
}

//
// correctness checks for result allocations

static void check_all_allocated(ggml_cgraph * graph) {
    for (int i = 0; i < ggml_graph_n_nodes(graph); ++i) {
        ggml_tensor * t = ggml_graph_node(graph, i);
        GGML_ASSERT(t->buffer != nullptr);
        GGML_ASSERT(t->data != nullptr);
    }
}

static void check_max_size(ggml_context * ctx) {
    for (ggml_tensor * t = ggml_get_first_tensor(ctx); t; t = ggml_get_next_tensor(ctx, t)) {
        auto   buft     = ggml_backend_buffer_get_type(t->buffer);
        size_t max_size = ggml_backend_buft_get_max_size(buft);
        size_t offset   = (char *) t->data - (char *) ggml_backend_buffer_get_base(t->buffer);
        GGML_ASSERT(t->data >= ggml_backend_buffer_get_base(t->buffer));
        GGML_ASSERT((size_t) offset + ggml_nbytes(t) <= max_size);
    }
}

static bool can_reuse_memory(ggml_cgraph * graph, int current_i, ggml_tensor * current, ggml_tensor * other) {
    if (other->flags & GGML_TENSOR_FLAG_OUTPUT) {
        return false;
    }
    // Check if `other` is still "alive", ie. an input to any node after the `current` op
    for (int i = current_i; i < ggml_graph_n_nodes(graph); ++i) {
        ggml_tensor * t = ggml_graph_node(graph, i);
        for (int s = 0; s < GGML_MAX_SRC; s++) {
            if (t == current && ggml_op_can_inplace(t->op)) {
                continue;
            }
            if (t->src[s] == other) {
                return false;
            }
            if (t->src[s] && t->src[s]->view_src == other) {
                return false;
            }
        }
    }
    return true;
}

static bool memory_overlap(ggml_tensor * a, ggml_tensor * b) {
    if (a->buffer != b->buffer) {
        return false;
    }
    int64_t a0 = (int64_t) a->data;
    int64_t a1 = a0 + ggml_nbytes(a);
    int64_t b0 = (int64_t) b->data;
    int64_t b1 = b0 + ggml_nbytes(b);
    return a1 > b0 && b1 > a0;
}

static ggml_tensor * get_view_source(ggml_tensor * t) {
    while (t->view_src) {
        t = t->view_src;
    }
    return t;
}

static void check_no_overlap(ggml_cgraph * graph) {
    for (int i = 0; i < ggml_graph_n_nodes(graph); ++i) {
        for (int j = 0; j < i; ++j) {
            ggml_tensor * t = ggml_graph_node(graph, i);
            ggml_tensor * o = ggml_graph_node(graph, j);
            GGML_ASSERT(t != o);

            if (get_view_source(t) == get_view_source(o)) {
                continue;
            }
            if (memory_overlap(t, o)) {
                GGML_ASSERT(can_reuse_memory(graph, i, t, o));
            }
        }
    }
}

//
// test cases

// Scenario where the first backend buffer is completely exhausted and there are further
// tensors which require a second buffer
static void test_max_size_too_many_tensors() {
    dummy_backend backend      = dummy_backend_init(16);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[7];
    x[0] = make_input_with_size(ctx, 8);
    x[1] = make_input_with_size(ctx, 8);
    x[2] = make_input_with_size(ctx, 8);
    x[3] = ggml_mul(ctx, x[0], x[1]);
    x[4] = ggml_add(ctx, x[1], x[2]);
    x[5] = ggml_add(ctx, x[3], x[0]);
    x[6] = ggml_add(ctx, x[4], x[5]);
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, x[6], &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    check_max_size(ctx);
    GGML_ASSERT(backend.context->allocated_total() <= 16 + 16);
}

// Scenario where there is some space left in the first buffer, but not enough to accommodate
// a larger tensor, so a second buffer is required
static void test_max_size_tensor_too_large() {
    dummy_backend backend      = dummy_backend_init(32);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[3];
    x[0] = make_input_with_size(ctx, 16);    // chunk 0, [0 , 16)
    x[1] = make_input_with_size(ctx, 8);     // chunk 0, [16, 24)
    x[2] = ggml_concat(ctx, x[0], x[1], 0);  // chunk 1, [0 , 24)
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, x[2], &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    check_max_size(ctx);
    GGML_ASSERT(backend.context->allocated_total() <= 32 + 24);
}

// Scenario where a single tensor exceeds the max buffer size - in this case the allocator
// should try to create a bigger buffer anyway, and wait for the backend to throw an error.
// Backends may report an artificially lower max size in some cases for compatibility reasons.
static void test_tensor_larger_than_max_size() {
    dummy_backend backend      = dummy_backend_init(16);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[2];
    x[0] = make_input_with_size(ctx, 24);
    x[1] = ggml_scale(ctx, x[0], 2.0f);
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, x[1], &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    GGML_ASSERT(backend.context->allocated_total() == 24);
}

// This test assumes a max of 16 buffer chunks, and tries to allocate tensors that would
// require more. Expectation is that the last buffer should grow to fit everything,
// leaving it to the backend to error out if it can't allocate that much.
static void test_not_enough_chunks() {
    const int max_chunks = 16;
    const int max_size   = 8;

    dummy_backend backend      = dummy_backend_init(max_size);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[max_chunks + 1];
    for (int i = 0; i < max_chunks + 1; ++i) {
        x[i] = make_input_with_size(ctx, max_size);
    }
    ggml_tensor * acc = x[0];
    for (int i = 0; i < max_chunks; ++i) {
        acc = ggml_add(ctx, acc, x[i + 1]);
    }
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, acc, &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    GGML_ASSERT(backend.context->allocated_total() > max_chunks * max_size);
}

// Fill up leftover unallocated space of a chunk after allocating a large tensor that
// requires a new chunk.
static void test_fill_leftover_space() {
    dummy_backend backend      = dummy_backend_init(16);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[4];
    x[0] = make_input_with_size(ctx, 8);
    x[1] = ggml_pad(ctx, x[0], 2, 0, 0, 0);
    x[3] = ggml_mean(ctx, x[1]);
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, x[3], &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    check_max_size(ctx);
    GGML_ASSERT(backend.context->allocated_total() <= 12 + 16);
}

// Check that views don't require any extra memory
static void test_view_inplace() {
    dummy_backend backend      = dummy_backend_init(32);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[6];
    x[0] = make_input_1d(ctx, 4);                // chunk 0, [0, 16)
    x[1] = ggml_reshape_2d(ctx, x[0], 2, 2);     // view of x0
    x[2] = ggml_permute(ctx, x[1], 1, 0, 2, 3);  // view of x0
    x[3] = ggml_view_1d(ctx, x[2], 2, 4);        // view of x0
    x[4] = make_input_1d(ctx, 2);                // chunk 0, [16, 24)
    x[5] = ggml_add(ctx, x[3], x[4]);            // reuse (inplace add)
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, x[5], &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    check_max_size(ctx);
    GGML_ASSERT(backend.context->allocated_total() <= 24);
}

static void test_reuse_and_free() {
    dummy_backend backend      = dummy_backend_init(40);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[9];
    x[0] = make_input_with_size(ctx, 24);
    x[1] = make_input_with_size(ctx, 8);
    x[2] = make_input_with_size(ctx, 8);
    x[3] = ggml_add(ctx, x[1], x[2]);        // reuse, free x2
    x[4] = ggml_pad(ctx, x[0], 2, 0, 0, 0);  // alloc new buffer, free x0
    x[5] = ggml_scale(ctx, x[4], 2.0f);      // alloc from free block
    x[6] = ggml_add(ctx, x[4], x[5]);        // reuse, free x5
    x[7] = ggml_view_1d(ctx, x[6], 2, 8);    // view
    x[8] = ggml_add(ctx, x[3], x[7]);        // reuse
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, x[8], &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    check_max_size(ctx);
    GGML_ASSERT(backend.context->allocated_total() <= 40 + 32 + 32);
}

static void test_merge_free_block(size_t max_buffer_size) {
    dummy_backend backend      = dummy_backend_init(max_buffer_size);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[9];
    x[0] = make_input_with_size(ctx, 16);
    x[1] = make_input_with_size(ctx, 16);
    x[2] = make_input_with_size(ctx, 16);
    x[3] = ggml_mean(ctx, x[0]);
    x[4] = ggml_mean(ctx, x[1]);
    x[5] = ggml_pad(ctx, x[2], 2, 0, 0, 0);
    x[6] = ggml_add(ctx, x[3], x[4]);
    x[7] = ggml_pad(ctx, x[6], 5, 0, 0, 0);
    x[8] = ggml_add(ctx, x[5], x[7]);
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, x[8], &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    check_max_size(ctx);
    GGML_ASSERT(backend.context->allocated_total() <= 32 + 32 + 24);
}

// Check that previously allocated but freed memory is preferred over allocating
// additional memory, even if the remaining space in a chunk would match tensor size better
static void test_prefer_already_allocated_memory() {
    dummy_backend backend      = dummy_backend_init(32, /*align*/ 4);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[3];
    x[0] = make_input_with_size(ctx, 24);  // [24b][8b unused]
    x[1] = ggml_mean(ctx, x[0]);           // [24b free][4b][4b unused]
    x[2] = ggml_mean(ctx, x[1]);           // should be allocated in the 24b block
    assign_names(ctx);

    ggml_gallocr_ptr galloc = allocate_graph(graph, x[2], &backend.buffer_type);
    check_all_allocated(graph);
    check_no_overlap(graph);
    GGML_ASSERT(backend.context->allocated_total() <= 28);
}

// test for allocating on multiple devices with some tensors in the graph
// allocated externally (not by gallocr).
static void test_multiple_buffer_types() {
    dummy_backend backend_a = dummy_backend_init(32);
    dummy_backend backend_b = dummy_backend_init(SIZE_MAX);

    auto [ctx_a, _a, ctx_a_ptr] = make_context();
    auto [ctx_b, _b, ctx_b_ptr] = make_context();
    auto [ctx, graph, ctx_ptr]  = make_context();

    ggml_tensor * a[2];
    a[0] = make_input_with_size(ctx_a, 16);
    a[1] = make_input_with_size(ctx_a, 16);
    assign_names(ctx_a, "a");

    ggml_tensor * b[2];
    b[0] = make_input_with_size(ctx_b, 24);
    b[1] = make_input_with_size(ctx_b, 4);
    assign_names(ctx_b, "b");

    ggml_tensor * x[9];
    x[0] = make_input_with_size(ctx, 16);
    x[1] = ggml_mul(ctx, x[0], a[0]);
    x[2] = ggml_pad(ctx, x[1], 2, 0, 0, 0);
    x[3] = ggml_mul(ctx, x[2], b[0]);
    x[4] = ggml_mean(ctx, x[3]);
    x[5] = ggml_add(ctx, x[4], b[1]);
    x[6] = ggml_pad(ctx, x[5], 3, 0, 0, 0);
    x[7] = ggml_add(ctx, x[6], a[1]);
    x[8] = ggml_scale(ctx, x[7], 2.0f);
    assign_names(ctx, "x");

    ggml_backend_buffer_ptr    buf_a(ggml_backend_alloc_ctx_tensors_from_buft(ctx_a, &backend_a.buffer_type));
    ggml_backend_buffer_ptr    buf_b(ggml_backend_alloc_ctx_tensors_from_buft(ctx_b, &backend_b.buffer_type));
    ggml_backend_buffer_type_t bufts[2] = { &backend_a.buffer_type, &backend_b.buffer_type };

    // assign buffer types manually to avoid extra complexity from backend scheduler
    ggml_set_output(x[8]);
    ggml_build_forward_expand(graph, x[8]);

    GGML_ASSERT(graph->n_leafs == 5);
    int leaf_buffer_ids[5];
    leaf_buffer_ids[get_leaf_id(graph, "a0")] = 0;
    leaf_buffer_ids[get_leaf_id(graph, "a1")] = 0;
    leaf_buffer_ids[get_leaf_id(graph, "b0")] = 1;
    leaf_buffer_ids[get_leaf_id(graph, "b1")] = 1;
    leaf_buffer_ids[get_leaf_id(graph, "x0")] = 0;

    GGML_ASSERT(graph->n_nodes == 8);
    int node_buffer_ids[8];
    node_buffer_ids[get_node_id(graph, "x1")] = 0;
    node_buffer_ids[get_node_id(graph, "x2")] = 0;
    node_buffer_ids[get_node_id(graph, "x3")] = 1;
    node_buffer_ids[get_node_id(graph, "x4")] = 1;
    node_buffer_ids[get_node_id(graph, "x5")] = 1;
    node_buffer_ids[get_node_id(graph, "x6")] = 1;
    node_buffer_ids[get_node_id(graph, "x7")] = 0;
    node_buffer_ids[get_node_id(graph, "x8")] = 0;

    ggml_gallocr_ptr galloc(ggml_gallocr_new_n(bufts, 2));
    ggml_gallocr_reserve_n(galloc.get(), graph, node_buffer_ids, leaf_buffer_ids);
    ggml_gallocr_alloc_graph(galloc.get(), graph);

    check_all_allocated(graph);
    check_no_overlap(graph);
    check_max_size(ctx);
    GGML_ASSERT(backend_a.context->allocated_total() <= 32 + 32 + 24);
    GGML_ASSERT(backend_b.context->allocated_total() <= 32 + 24);
}

static void test_buffer_size_zero() {
    dummy_backend backend_a    = dummy_backend_init(SIZE_MAX);
    dummy_backend backend_b    = dummy_backend_init(SIZE_MAX);
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[2];
    x[0] = make_input_with_size(ctx, 16);
    x[1] = ggml_scale(ctx, x[0], 2.0f);

    ggml_set_output(x[1]);
    ggml_build_forward_expand(graph, x[1]);

    int leaf_buffer_ids[1] = { 0 };
    int node_buffer_ids[1] = { 0 };

    ggml_backend_buffer_type_t bufts[2] = { &backend_a.buffer_type, &backend_b.buffer_type };
    ggml_gallocr_ptr           galloc   = ggml_gallocr_ptr(ggml_gallocr_new_n(bufts, 2));
    bool                       res1     = ggml_gallocr_reserve_n(galloc.get(), graph, node_buffer_ids, leaf_buffer_ids);
    bool                       res2     = ggml_gallocr_alloc_graph(galloc.get(), graph);
    GGML_ASSERT(res1 && res2);

    check_all_allocated(graph);
    GGML_ASSERT(backend_a.context->allocated_total() == 16);
    GGML_ASSERT(backend_b.context->allocated_total() == 0);
}

// Test re-using gallocr for a different graph. The new graph has the same
// total size, but one of the chunks is larger, so reallocation is required.
static void test_reallocation() {
    dummy_backend    backend = dummy_backend_init(32, /*align*/ 4);
    ggml_gallocr_ptr galloc;
    {
        auto [ctx, graph, ctx_ptr] = make_context();
        ggml_tensor * x[4];
        x[0] = make_input_with_size(ctx, 24);
        x[1] = make_input_with_size(ctx, 16);
        x[2] = ggml_view_1d(ctx, x[0], 4, 0);
        x[3] = ggml_add(ctx, x[2], x[1]);
        assign_names(ctx);

        galloc = allocate_graph(graph, x[3], &backend.buffer_type);
        check_all_allocated(graph);
        GGML_ASSERT(backend.context->allocated_total() == 40);
    }
    {
        auto [ctx, graph, ctx_ptr] = make_context();
        ggml_tensor * x[3];
        x[0] = make_input_with_size(ctx, 20);
        x[1] = make_input_with_size(ctx, 20);
        x[2] = ggml_add(ctx, x[0], x[1]);
        assign_names(ctx);
        ggml_set_output(x[2]);
        ggml_build_forward_expand(graph, x[2]);

        bool result = ggml_gallocr_alloc_graph(galloc.get(), graph);
        GGML_ASSERT(result);
        check_all_allocated(graph);
        GGML_ASSERT(backend.context->allocated_total() == 40);
    }
}

//
// R46b B7b-R2 (check finding 5): the batch plan contract at the production caller,
// ggml_backend_alloc_ctx_tensors_from_buft(). One call = one (buffer type, context) batch.
//
// A MULTI-buffer set must never start allocating on an undecided answer, because that places part
// of a set whose completion is unknown. A SINGLE-buffer allocation has no partial state to leave
// behind and must keep its previous behaviour exactly, undecided answer or not.

static void plan_wire(dummy_backend & b) {
    b.buffer_type.iface.plan_begin        = dummy_backend_buffer_type_plan_begin;
    b.buffer_type.iface.plan_alloc_buffer = dummy_backend_buffer_type_plan_alloc_buffer;
    b.buffer_type.iface.plan_free         = dummy_backend_buffer_type_plan_free;
}

// `n` tensors of `size_bytes` with max_buffer_size == size_bytes, so the loader's own split
// arithmetic produces exactly `n` buffers.
static ggml_context_ptr plan_make_ctx(int n, size_t size_bytes) {
    ggml_init_params params{};
    params.mem_size = ggml_tensor_overhead() * (size_t) (n + 2);
    params.no_alloc = true;
    ggml_context_ptr ctx = ggml_context_ptr(ggml_init(params));
    for (int i = 0; i < n; i++) {
        make_input_with_size(ctx.get(), size_bytes);
    }
    return ctx;
}

static void test_plan_indeterminate_never_partially_allocates() {
    const size_t chunk = 64;

    // 1. multi-buffer + INDETERMINATE: refused, and NOT ONE buffer was allocated
    {
        dummy_backend backend = dummy_backend_init(chunk);
        plan_wire(backend);
        backend.context->plan_status = GGML_BACKEND_PLAN_INDETERMINATE;

        ggml_context_ptr ctx = plan_make_ctx(3, chunk);
        ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx.get(), &backend.buffer_type);
        GGML_ASSERT(buf == nullptr);
        GGML_ASSERT(backend.context->plan_begin_calls == 1);
        GGML_ASSERT(backend.context->plan_n == 3);
        GGML_ASSERT(backend.context->alloc_calls == 0);
        GGML_ASSERT(backend.context->buffers.empty());
    }

    // 2. single buffer + INDETERMINATE: unchanged behaviour, it allocates
    {
        dummy_backend backend = dummy_backend_init(chunk);
        plan_wire(backend);
        backend.context->plan_status = GGML_BACKEND_PLAN_INDETERMINATE;

        ggml_context_ptr ctx = plan_make_ctx(1, chunk);
        ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx.get(), &backend.buffer_type);
        GGML_ASSERT(buf != nullptr);
        GGML_ASSERT(backend.context->plan_n == 1);
        GGML_ASSERT(backend.context->alloc_calls == 1);
        ggml_backend_buffer_free(buf);
    }

    // 3. multi-buffer + INFEASIBLE: refused before the first allocation
    {
        dummy_backend backend = dummy_backend_init(chunk);
        plan_wire(backend);
        backend.context->plan_status = GGML_BACKEND_PLAN_INFEASIBLE;

        ggml_context_ptr ctx = plan_make_ctx(3, chunk);
        ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx.get(), &backend.buffer_type);
        GGML_ASSERT(buf == nullptr);
        GGML_ASSERT(backend.context->alloc_calls == 0);
    }

    // 4. multi-buffer + FEASIBLE: every buffer comes from the plan, and the plan is freed once
    {
        dummy_backend backend = dummy_backend_init(chunk);
        plan_wire(backend);
        backend.context->plan_status = GGML_BACKEND_PLAN_FEASIBLE;

        ggml_context_ptr ctx = plan_make_ctx(3, chunk);
        ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx.get(), &backend.buffer_type);
        GGML_ASSERT(buf != nullptr);
        GGML_ASSERT(backend.context->alloc_calls == 3);
        GGML_ASSERT(backend.context->plan_free_calls == 1);
    }

    // 5. a buffer type WITHOUT the hooks is UNSUPPORTED and allocates exactly as before
    {
        dummy_backend backend = dummy_backend_init(chunk);
        ggml_context_ptr ctx = plan_make_ctx(3, chunk);
        ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx.get(), &backend.buffer_type);
        GGML_ASSERT(buf != nullptr);
        GGML_ASSERT(backend.context->plan_begin_calls == 0);
        GGML_ASSERT(backend.context->alloc_calls == 3);
    }
}

//
// R46b B7b — the LOADER-WIDE transaction at its production boundary, the ggml-alloc API that
// llama_model::load_tensors() calls: init / add per group / commit / alloc per group / free.
//
// The unit under test is a whole model load's worth of groups, not one group. Every case below asks
// the same question in a different shape: can any driver memory be allocated for one group of a load
// whose LATER groups cannot be placed, and is every reservation discharged exactly once.

struct loader_plan_deleter {
    void operator()(ggml_backend_alloc_plan_t p) const { ggml_backend_alloc_plan_free(p); }
};
using loader_plan_ptr = std::unique_ptr<ggml_backend_alloc_plan, loader_plan_deleter>;

static void test_loader_transaction_refuses_before_any_allocation() {
    const size_t chunk = 64;

    // two buffer types in one load. A fits; B cannot place its three buffers. The whole load must be
    // refused, and A - censused and planned FIRST - must not have allocated one byte.
    dummy_backend a = dummy_backend_init(chunk);
    dummy_backend b = dummy_backend_init(chunk);
    plan_wire(a);
    plan_wire(b);
    a.context->budget = 10 * chunk;
    b.context->budget = 2 * chunk;   // three buffers will not fit

    ggml_context_ptr ctx_a = plan_make_ctx(3, chunk);
    ggml_context_ptr ctx_b = plan_make_ctx(3, chunk);

    loader_plan_ptr plan(ggml_backend_alloc_plan_init());
    GGML_ASSERT(plan);
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx_a.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx_b.get(), &b.buffer_type));

    GGML_ASSERT(!ggml_backend_alloc_plan_commit(plan.get()));

    GGML_ASSERT(a.context->alloc_calls == 0);          // the earlier group never touched the driver
    GGML_ASSERT(b.context->alloc_calls == 0);
    GGML_ASSERT(a.context->buffers.empty());
    plan.reset();
    GGML_ASSERT(a.context->reserved == 0);             // A's plan was opened and released
    GGML_ASSERT(a.context->plan_free_calls == 1);
    GGML_ASSERT(a.context->released == 3 * chunk);
}

static void test_loader_transaction_competing_and_isolated() {
    const size_t chunk = 64;

    // 1. two concurrent transactions against ONE buffer type must not over-admit
    {
        dummy_backend a = dummy_backend_init(chunk);
        plan_wire(a);
        a.context->budget = 5 * chunk;

        ggml_context_ptr ctx1 = plan_make_ctx(3, chunk);
        ggml_context_ptr ctx2 = plan_make_ctx(3, chunk);

        loader_plan_ptr first(ggml_backend_alloc_plan_init());
        GGML_ASSERT(ggml_backend_alloc_plan_add(first.get(), ctx1.get(), &a.buffer_type));
        GGML_ASSERT(ggml_backend_alloc_plan_commit(first.get()));
        GGML_ASSERT(a.context->reserved == 3 * chunk);

        // 3 + 3 buffers do not fit in 5: the second load is refused against the first's reservation
        loader_plan_ptr second(ggml_backend_alloc_plan_init());
        GGML_ASSERT(ggml_backend_alloc_plan_add(second.get(), ctx2.get(), &a.buffer_type));
        GGML_ASSERT(!ggml_backend_alloc_plan_commit(second.get()));
        second.reset();

        // the first transaction going away frees the capacity, and the same load now fits
        first.reset();
        GGML_ASSERT(a.context->reserved == 0);
        loader_plan_ptr third(ggml_backend_alloc_plan_init());
        GGML_ASSERT(ggml_backend_alloc_plan_add(third.get(), ctx2.get(), &a.buffer_type));
        GGML_ASSERT(ggml_backend_alloc_plan_commit(third.get()));
    }

    // 2. two devices: a transaction spanning both keeps their capacity separate
    {
        dummy_backend a = dummy_backend_init(chunk);
        dummy_backend b = dummy_backend_init(chunk);
        plan_wire(a);
        plan_wire(b);
        a.context->budget = 2 * chunk;   // exactly its own share, nothing spare
        b.context->budget = 2 * chunk;

        ggml_context_ptr ctx_a = plan_make_ctx(2, chunk);
        ggml_context_ptr ctx_b = plan_make_ctx(2, chunk);

        loader_plan_ptr plan(ggml_backend_alloc_plan_init());
        GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx_a.get(), &a.buffer_type));
        GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx_b.get(), &b.buffer_type));
        GGML_ASSERT(ggml_backend_alloc_plan_commit(plan.get()));

        // neither device was charged the other's bytes
        GGML_ASSERT(a.context->reserved == 2 * chunk);
        GGML_ASSERT(b.context->reserved == 2 * chunk);
        GGML_ASSERT(a.context->plan_begin_calls == 1 && a.context->plan_n == 2);
        GGML_ASSERT(b.context->plan_begin_calls == 1 && b.context->plan_n == 2);
    }
}

static void test_loader_transaction_two_contexts_one_buffer_type() {
    const size_t chunk = 64;

    // two contexts share one buffer type: ONE plan covers both, and the second context consumes its
    // OWN entries (3,4,5) rather than re-consuming the first context's (0,1,2)
    dummy_backend a = dummy_backend_init(chunk);
    plan_wire(a);
    a.context->budget = 6 * chunk;

    ggml_context_ptr ctx1 = plan_make_ctx(3, chunk);
    ggml_context_ptr ctx2 = plan_make_ctx(3, chunk);

    loader_plan_ptr plan(ggml_backend_alloc_plan_init());
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx1.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx2.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_commit(plan.get()));
    GGML_ASSERT(a.context->plan_begin_calls == 1);   // one plan, not one per context
    GGML_ASSERT(a.context->plan_n == 6);

    ggml_backend_buffer_t b1 = ggml_backend_alloc_plan_alloc(plan.get(), ctx1.get(), &a.buffer_type);
    ggml_backend_buffer_t b2 = ggml_backend_alloc_plan_alloc(plan.get(), ctx2.get(), &a.buffer_type);
    GGML_ASSERT(b1 != nullptr && b2 != nullptr);
    GGML_ASSERT(a.context->alloc_calls == 6);
    GGML_ASSERT(a.context->reserved == 0);           // every entry adopted, nothing left reserved

    const std::vector<size_t> expected = { 0, 1, 2, 3, 4, 5 };
    GGML_ASSERT(a.context->plan_alloc_order == expected);

    plan.reset();
    GGML_ASSERT(a.context->plan_free_calls == 1);    // one plan freed once, not once per group
    GGML_ASSERT(a.context->released == 0);           // nothing to release: all consumed

    ggml_backend_buffer_free(b1);
    ggml_backend_buffer_free(b2);
    GGML_ASSERT(a.context->allocated_total() == 0);
}

static void test_loader_transaction_mid_load_failure_rolls_back() {
    const size_t chunk = 64;

    dummy_backend a = dummy_backend_init(chunk);
    plan_wire(a);
    a.context->budget        = 6 * chunk;
    a.context->fail_alloc_at = 4;   // the 5th planned allocation fails: the second group, mid-group

    ggml_context_ptr ctx1 = plan_make_ctx(3, chunk);
    ggml_context_ptr ctx2 = plan_make_ctx(3, chunk);

    loader_plan_ptr plan(ggml_backend_alloc_plan_init());
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx1.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx2.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_commit(plan.get()));

    ggml_backend_buffer_t b1 = ggml_backend_alloc_plan_alloc(plan.get(), ctx1.get(), &a.buffer_type);
    GGML_ASSERT(b1 != nullptr);
    ggml_backend_buffer_t b2 = ggml_backend_alloc_plan_alloc(plan.get(), ctx2.get(), &a.buffer_type);
    GGML_ASSERT(b2 == nullptr);   // the failing group builds nothing

    plan.reset();
    // entry 3 was adopted then freed with its group's buffers; entries 4 and 5 were never consumed
    // and came back through plan_free. Either way each is discharged exactly once.
    GGML_ASSERT(a.context->reserved == 0);
    GGML_ASSERT(a.context->released == 2 * chunk);
    GGML_ASSERT(a.context->plan_free_calls == 1);

    ggml_backend_buffer_free(b1);
    GGML_ASSERT(a.context->allocated_total() == 0);
}

static void test_loader_transaction_mixed_backends() {
    const size_t chunk = 64;

    // one planning buffer type and one that does not implement planning, in the same load
    dummy_backend planned   = dummy_backend_init(chunk);
    dummy_backend unplanned = dummy_backend_init(chunk);
    plan_wire(planned);
    planned.context->budget = 3 * chunk;

    ggml_context_ptr ctx_p = plan_make_ctx(3, chunk);
    ggml_context_ptr ctx_u = plan_make_ctx(3, chunk);

    loader_plan_ptr plan(ggml_backend_alloc_plan_init());
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx_p.get(), &planned.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx_u.get(), &unplanned.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_commit(plan.get()));

    GGML_ASSERT(unplanned.context->plan_begin_calls == 0);   // never asked to plan
    GGML_ASSERT(unplanned.context->alloc_calls == 0);        // and nothing allocated by commit

    ggml_backend_buffer_t bp = ggml_backend_alloc_plan_alloc(plan.get(), ctx_p.get(), &planned.buffer_type);
    ggml_backend_buffer_t bu = ggml_backend_alloc_plan_alloc(plan.get(), ctx_u.get(), &unplanned.buffer_type);
    GGML_ASSERT(bp != nullptr && bu != nullptr);
    GGML_ASSERT(planned.context->alloc_calls == 3);
    GGML_ASSERT(unplanned.context->alloc_calls == 3);        // allocates exactly as it always did
    GGML_ASSERT(unplanned.context->plan_free_calls == 0);

    // a group that never joined the transaction still allocates through the ordinary path
    dummy_backend outside = dummy_backend_init(chunk);
    ggml_context_ptr ctx_o = plan_make_ctx(2, chunk);
    ggml_backend_buffer_t bo = ggml_backend_alloc_plan_alloc(plan.get(), ctx_o.get(), &outside.buffer_type);
    GGML_ASSERT(bo != nullptr);
    GGML_ASSERT(outside.context->alloc_calls == 2);

    plan.reset();
    ggml_backend_buffer_free(bp);
    ggml_backend_buffer_free(bu);
    ggml_backend_buffer_free(bo);
}

static void test_loader_transaction_overflow_is_refused() {
    // three groups whose byte totals sum past SIZE_MAX. The transaction must refuse while counting,
    // before any backend is asked to plan - a wrapped total would be a small number a backend would
    // cheerfully admit.
    const size_t huge = (size_t) 0x6000000000000000ULL;

    dummy_backend a = dummy_backend_init(SIZE_MAX);
    plan_wire(a);

    ggml_context_ptr ctx1 = plan_make_ctx(1, huge);
    ggml_context_ptr ctx2 = plan_make_ctx(1, huge);
    ggml_context_ptr ctx3 = plan_make_ctx(1, huge);

    loader_plan_ptr plan(ggml_backend_alloc_plan_init());
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx1.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx2.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx3.get(), &a.buffer_type));

    GGML_ASSERT(!ggml_backend_alloc_plan_commit(plan.get()));
    GGML_ASSERT(a.context->plan_begin_calls == 0);
    GGML_ASSERT(a.context->alloc_calls == 0);
}

//
// R46b B7c — the bounded host split, at the same loader-wide transaction boundary. These cases are
// about OWNERSHIP: who holds the spilled bytes at each instant, and that a split in flight is
// discharged exactly once. Which memory a real Vulkan device picks is decided by the planner and is
// covered in the heapres suite, not here.

static void test_loader_split_refusal_allocates_nothing() {
    const size_t chunk = 64;

    // A fits. B needs three buffers, holds two, and could spill the third - except the bound says
    // no. The whole load must be refused with A untouched: a bounded split that cannot be made
    // large enough is still a refusal, not a partial load.
    dummy_backend a = dummy_backend_init(chunk);
    dummy_backend b = dummy_backend_init(chunk);
    plan_wire(a);
    plan_wire(b);
    a.context->budget = 10 * chunk;
    b.context->budget = 2 * chunk;
    b.context->host_budget    = 8 * chunk;   // room to spill...
    b.context->host_split_max = chunk - 1;   // ...but not this load's room

    ggml_context_ptr ctx_a = plan_make_ctx(3, chunk);
    ggml_context_ptr ctx_b = plan_make_ctx(3, chunk);

    loader_plan_ptr plan(ggml_backend_alloc_plan_init());
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx_a.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx_b.get(), &b.buffer_type));

    GGML_ASSERT(!ggml_backend_alloc_plan_commit(plan.get()));
    GGML_ASSERT(a.context->alloc_calls == 0 && b.context->alloc_calls == 0);
    GGML_ASSERT(b.context->host_reserved == 0);

    plan.reset();
    GGML_ASSERT(a.context->reserved == 0 && b.context->reserved == 0);
    GGML_ASSERT(b.context->host_reserved == 0);

    // the same load with the bound one byte wider is admitted, and reports the split it took
    b.context->host_split_max = chunk;
    loader_plan_ptr wider(ggml_backend_alloc_plan_init());
    GGML_ASSERT(ggml_backend_alloc_plan_add(wider.get(), ctx_a.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_add(wider.get(), ctx_b.get(), &b.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_commit(wider.get()));
    GGML_ASSERT(b.context->reserved == 2 * chunk && b.context->host_reserved == chunk);
}

static void test_loader_split_staging_reserve_precedes_admission() {
    const size_t chunk = 64;

    // The spill pool is big enough for the load - until the staging reserve is charged first. The
    // reserve is not capacity the weights may borrow, so the load is refused rather than admitted
    // into it.
    dummy_backend a = dummy_backend_init(chunk);
    plan_wire(a);
    a.context->budget       = chunk;       // one buffer on the device
    a.context->host_budget  = 4 * chunk;   // two more would fit the spill pool...

    ggml_context_ptr ctx = plan_make_ctx(3, chunk);

    {
        a.context->staging_reserve = 2 * chunk + 1;   // ...but this is not theirs to take
        loader_plan_ptr plan(ggml_backend_alloc_plan_init());
        GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx.get(), &a.buffer_type));
        GGML_ASSERT(!ggml_backend_alloc_plan_commit(plan.get()));
        GGML_ASSERT(a.context->alloc_calls == 0);
    }

    // exactly at the boundary the same load is admitted, and the reserve is still untouched
    a.context->staging_reserve = 2 * chunk;
    {
        loader_plan_ptr plan(ggml_backend_alloc_plan_init());
        GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx.get(), &a.buffer_type));
        GGML_ASSERT(ggml_backend_alloc_plan_commit(plan.get()));
        GGML_ASSERT(a.context->reserved == chunk && a.context->host_reserved == 2 * chunk);

        ggml_backend_buffer_t buf = ggml_backend_alloc_plan_alloc(plan.get(), ctx.get(), &a.buffer_type);
        GGML_ASSERT(buf != nullptr);
        GGML_ASSERT(a.context->host_consumed == 2 * chunk);
        GGML_ASSERT(a.context->host_reserved == 0);
        GGML_ASSERT(a.context->staging_reserve == 2 * chunk);   // never consumed by weights
        ggml_backend_buffer_free(buf);
    }
    GGML_ASSERT(a.context->host_consumed == 0);
    GGML_ASSERT(a.context->host_reserved == 0);
}

static void test_loader_split_mid_load_failure_rolls_back() {
    const size_t chunk = 64;

    // A split is in flight when an allocation fails part way through the group. Every byte - device
    // and spilled - must come back, each released exactly once.
    dummy_backend a = dummy_backend_init(chunk);
    plan_wire(a);
    a.context->budget          = 2 * chunk;
    a.context->host_budget     = 2 * chunk;
    a.context->fail_alloc_at   = 2;   // third buffer of the group, so a spilled entry is live

    ggml_context_ptr ctx = plan_make_ctx(4, chunk);

    loader_plan_ptr plan(ggml_backend_alloc_plan_init());
    GGML_ASSERT(ggml_backend_alloc_plan_add(plan.get(), ctx.get(), &a.buffer_type));
    GGML_ASSERT(ggml_backend_alloc_plan_commit(plan.get()));
    GGML_ASSERT(a.context->reserved == 2 * chunk && a.context->host_reserved == 2 * chunk);

    GGML_ASSERT(ggml_backend_alloc_plan_alloc(plan.get(), ctx.get(), &a.buffer_type) == nullptr);

    plan.reset();
    GGML_ASSERT(a.context->reserved == 0 && a.context->host_reserved == 0);
    GGML_ASSERT(a.context->host_consumed == 0);
    GGML_ASSERT(a.context->released + a.context->host_released == 2 * chunk);   // the two never built
    GGML_ASSERT(a.context->plan_free_calls == 1);
}

static void test_backend_graph_optimize(ggml_backend_t, ggml_cgraph * graph, ggml_backend_graph_optimize_params * params) {
    GGML_ASSERT(graph->n_nodes == 3);
    params->add_alloc_dep(params->user_data, graph->nodes[0], graph->nodes[2]);
}

static bool graph_reuses_allocation(bool add_alloc_dep) {
    auto [ctx, graph, ctx_ptr] = make_context();

    ggml_tensor * x[4];
    x[0] = make_input_with_size(ctx, 16);
    x[1] = ggml_scale(ctx, x[0], 2.0f);
    x[2] = ggml_scale(ctx, x[1], 2.0f);
    x[3] = ggml_scale(ctx, x[2], 2.0f);

    ggml_set_output(x[3]);
    ggml_build_forward_expand(graph, x[3]);

    dummy_backend backend = dummy_backend_init(SIZE_MAX);
    if (add_alloc_dep) {
        backend.context->backend.iface.graph_optimize = test_backend_graph_optimize;
    }

    ggml_backend_t             backend_ptr = &backend.context->backend;
    ggml_backend_buffer_type_t buft        = &backend.buffer_type;
    ggml_backend_sched_ptr     sched(ggml_backend_sched_new(&backend_ptr, &buft, 1, 8, false, true));
    GGML_ASSERT(ggml_backend_sched_alloc_graph(sched.get(), graph));

    return x[1]->data == x[2]->data;
}

static void test_graph_optimize_alloc_dep() {
    GGML_ASSERT(graph_reuses_allocation(false));
    GGML_ASSERT(!graph_reuses_allocation(true));
}

static void run(const char * name, void (*f)()) {
    printf("%s ", name);
    fflush(stdout);
    f();
    printf("PASSED\n");
}

int main() {
    run("test_max_size_too_many_tensors", test_max_size_too_many_tensors);
    run("test_max_size_tensor_too_large", test_max_size_tensor_too_large);
    run("test_tensor_larger_than_max_size", test_tensor_larger_than_max_size);
    run("test_not_enough_chunks", test_not_enough_chunks);
    run("test_fill_leftover_space", test_fill_leftover_space);
    run("test_view_inplace", test_view_inplace);
    run("test_reuse_and_free", test_reuse_and_free);
    run("test_merge_free_block(32)", []() { test_merge_free_block(32); });
    run("test_merge_free_block(SIZE_MAX)", []() { test_merge_free_block(SIZE_MAX); });
    run("test_prefer_already_allocated_memory", test_prefer_already_allocated_memory);
    run("test_multiple_buffer_types", test_multiple_buffer_types);
    run("test_buffer_size_zero", test_buffer_size_zero);
    run("test_reallocation", test_reallocation);
    run("test_graph_optimize_alloc_dep", test_graph_optimize_alloc_dep);
    run("test_plan_indeterminate_never_partially_allocates", test_plan_indeterminate_never_partially_allocates);
    run("test_loader_transaction_refuses_before_any_allocation", test_loader_transaction_refuses_before_any_allocation);
    run("test_loader_transaction_competing_and_isolated", test_loader_transaction_competing_and_isolated);
    run("test_loader_transaction_two_contexts_one_buffer_type", test_loader_transaction_two_contexts_one_buffer_type);
    run("test_loader_transaction_mid_load_failure_rolls_back", test_loader_transaction_mid_load_failure_rolls_back);
    run("test_loader_split_refusal_allocates_nothing", test_loader_split_refusal_allocates_nothing);
    run("test_loader_split_staging_reserve_precedes_admission", test_loader_split_staging_reserve_precedes_admission);
    run("test_loader_split_mid_load_failure_rolls_back", test_loader_split_mid_load_failure_rolls_back);
    run("test_loader_transaction_mixed_backends", test_loader_transaction_mixed_backends);
    run("test_loader_transaction_overflow_is_refused", test_loader_transaction_overflow_is_refused);
    return 0;
}
