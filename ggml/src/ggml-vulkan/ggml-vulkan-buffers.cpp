#include "ggml-vulkan-common.h"

ggml_backend_buffer_type_i ggml_backend_vk_buffer_type_interface = {
    /* .get_name         = */ ggml_backend_vk_buffer_type_name,
    /* .alloc_buffer     = */ ggml_backend_vk_buffer_type_alloc_buffer,
    /* .get_alignment    = */ ggml_backend_vk_buffer_type_get_alignment,
    /* .get_max_size     = */ ggml_backend_vk_buffer_type_get_max_size,
    /* .get_alloc_size   = */ ggml_backend_vk_buffer_type_get_alloc_size,
    /* .is_host          = */ NULL,
};

// lane-230 / R46b commit E: GGML_VK_PLACEMENT=bulk-large-heap, an OPT-IN explicit placement
// policy. Unset keeps every existing selection byte-for-byte.
//
// R47d traced both load failures to the same shape: the placement chain accepts a memory type
// whenever the WHOLE heap is at least as large as this one allocation (no cumulative accounting),
// so GGML_VK_PREFER_HOST_MEMORY=1 puts the bulk weights on the host-visible heap 1 and a 256 MiB
// grouping puts them in the small combined-property heap 2, and the FIRST pinned upload then dies
// at submit seq=0 with ErrorUnknown. The policy routes bulk weight buffers to a DEVICE_LOCAL type
// on the LARGEST heap and excludes every other heap for them; grouping is untouched, and staging
// and scratch keep the paths they already use. Receipt:
// research/local-inference/lane-evidence/2026-09-11-lane-232-r47a-alloc-instrument/
// R47D-TRACES-SWEEP.md, "Placement levers and loader commission".
//
// NOT implemented here and OWED: the thread-safe cumulative requirement reservation with rollback
// that the same commission asks for. This policy is heap SELECTION only.
static bool ggml_vk_placement_bulk_large_heap() {
    static const bool on = [] {
        const char * s = getenv("GGML_VK_PLACEMENT");
        return s != nullptr && strcmp(s, "bulk-large-heap") == 0;
    }();
    return on;
}

static uint32_t ggml_vk_largest_heap(const vk::PhysicalDeviceMemoryProperties & mem_props) {
    uint32_t best = 0;
    for (uint32_t h = 1; h < mem_props.memoryHeapCount; ++h) {
        if (mem_props.memoryHeaps[h].size > mem_props.memoryHeaps[best].size) {
            best = h;
        }
    }
    return best;
}

// only_heap != UINT32_MAX restricts the candidate list to that one heap (R46b commit E).
static std::vector<uint32_t> ggml_vk_find_memory_properties(const vk::PhysicalDeviceMemoryProperties* mem_props, vk::MemoryRequirements* mem_req, vk::MemoryPropertyFlags flags, uint32_t only_heap = UINT32_MAX) {
    std::vector<uint32_t> indices;

    for (uint32_t i = 0; i < mem_props->memoryTypeCount; ++i) {
        vk::MemoryType memory_type = mem_props->memoryTypes[i];
        if (only_heap != UINT32_MAX && memory_type.heapIndex != only_heap) {
            continue;
        }
        if ((mem_req->memoryTypeBits & ((uint64_t)1 << i)) &&
            (flags & memory_type.propertyFlags) == flags &&
            mem_props->memoryHeaps[memory_type.heapIndex].size >= mem_req->size) {
            indices.push_back(i);
        }
    }
    return indices;
}

// lane-232 / R47a: opt-in allocation + submit trace, GGML_VK_ALLOC_TRACE=1.
//
// R47c could not name why two placement configs die at the first queue submit after every
// allocation reported success, because GGML_VK_MEMORY_LOGGER records requested bytes and a
// device/host category and nothing else. This trace adds the missing fields: requirement bytes,
// memoryTypeBits, the chosen memory type and heap, per-heap live bytes, the driver's budget and
// usage where VK_EXT_memory_budget is supported, and one line per queue submit carrying its
// recorded copy batch and the VkResult. Off unless the variable is set; the enable is read once.
bool ggml_vk_alloc_trace_enabled() {
    static const bool enabled = [] {
        const char * s = getenv("GGML_VK_ALLOC_TRACE");
        return s != nullptr && s[0] == '1';
    }();
    return enabled;
}

void vk_alloc_trace_line(const std::string & body) {
    static std::mutex line_mutex;
    std::lock_guard<std::mutex> guard(line_mutex);
    // endl, not "\n": every line must survive an abort mid-submit.
    std::cerr << "ggml_vulkan alloc-trace: " << body << std::endl;
}

static uint64_t vk_alloc_trace_new_seq() {
    static std::atomic<uint64_t> counter{0};
    return counter.fetch_add(1);
}

// The batch detail (buffers, offsets, lengths) is only known at the recording site in
// ggml_vk_submit; the VkResult is only known inside the queue wrapper. One sequence number
// handed from the first to the second ties the two lines together.
static thread_local uint64_t vk_alloc_trace_pending_submit_seq = UINT64_MAX;

uint64_t vk_alloc_trace_begin_submit() {
    const uint64_t seq = vk_alloc_trace_new_seq();
    vk_alloc_trace_pending_submit_seq = seq;
    return seq;
}

uint64_t vk_alloc_trace_take_submit() {
    uint64_t seq = vk_alloc_trace_pending_submit_seq;
    if (seq == UINT64_MAX) {
        // a submit that never passed through ggml_vk_submit's recording path (e.g. fence-only)
        seq = vk_alloc_trace_new_seq();
    }
    vk_alloc_trace_pending_submit_seq = UINT64_MAX;
    return seq;
}

// lane-232 / R47a: live per-heap accounting for the allocation trace. The memory logger keeps
// requested bytes by device/host category; this keeps the allocated requirement bytes by HEAP,
// which is the number a placement failure is actually about.
struct vk_alloc_trace_state {
    std::mutex mutex;
    std::map<uint32_t, uint64_t> heap_live;                            // heapIndex -> live requirement bytes
    std::map<VkBuffer, std::pair<uint32_t, uint64_t>> by_buffer;       // buffer -> (heapIndex, requirement bytes)
    uint64_t next_alloc_id = 0;
};

static vk_alloc_trace_state & vk_alloc_trace() {
    static vk_alloc_trace_state state;
    return state;
}

static uint64_t vk_alloc_trace_next_id() {
    std::lock_guard<std::mutex> guard(vk_alloc_trace().mutex);
    return vk_alloc_trace().next_alloc_id++;
}

// "budget=unavailable" when VK_EXT_memory_budget is not supported on this device. Printing zeros
// there would be a fabricated measurement, which is exactly what R47c indicted the old logger for.
static std::string vk_alloc_trace_budget_str(vk_device& device) {
    if (device->idx >= vk_instance.device_supports_membudget.size() ||
        !vk_instance.device_supports_membudget[device->idx]) {
        return "budget=unavailable";
    }

    vk::PhysicalDeviceMemoryBudgetPropertiesEXT budgetprops;
    vk::PhysicalDeviceMemoryProperties2 memprops = {};
    memprops.pNext = &budgetprops;
    device->physical_device.getMemoryProperties2(&memprops);

    std::stringstream ss;
    ss << "budget=[";
    for (uint32_t i = 0; i < memprops.memoryProperties.memoryHeapCount; ++i) {
        if (i > 0) {
            ss << ",";
        }
        ss << i << ":" << budgetprops.heapBudget[i] << "/" << budgetprops.heapUsage[i];
    }
    ss << "]";
    return ss.str();
}

// Takes the lock: allocation can run from more than one thread, and an unguarded read of the map
// while another thread inserts would crash inside the instrument during the one run it exists for.
static std::string vk_alloc_trace_heap_live_str() {
    std::lock_guard<std::mutex> guard(vk_alloc_trace().mutex);
    std::stringstream ss;
    ss << "heap_live=[";
    bool first = true;
    for (const auto & kv : vk_alloc_trace().heap_live) {
        if (!first) {
            ss << ",";
        }
        first = false;
        ss << kv.first << ":" << kv.second;
    }
    ss << "]";
    return ss.str();
}

#ifndef _WIN32
extern char ** environ;
#endif

// Same-run device topology and the effective GGML_/LLAMA_/VK_ environment, printed once so a
// trace can be read without guessing which box and which flags produced it. The binary's identity
// is NOT hashed here: the build receipt carries its sha256.
static void vk_alloc_trace_preamble(vk_device& device) {
    static std::once_flag once;
    std::call_once(once, [&device] {
        const vk::PhysicalDeviceMemoryProperties mp = device->physical_device.getMemoryProperties();

        std::stringstream ss;
        ss << "device name=\"" << device->name << "\" idx=" << device->idx
           << " uma=" << (device->uma ? 1 : 0)
           << " prefer_host=" << (device->prefer_host_memory ? 1 : 0)
           << " max_buffer_size=" << device->max_buffer_size;
        vk_alloc_trace_line(ss.str());

        for (uint32_t i = 0; i < mp.memoryHeapCount; ++i) {
            vk_alloc_trace_line("heap " + std::to_string(i) +
                                " size=" + std::to_string(mp.memoryHeaps[i].size) +
                                " flags=" + to_string(mp.memoryHeaps[i].flags));
        }
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
            vk_alloc_trace_line("memtype " + std::to_string(i) +
                                " heap=" + std::to_string(mp.memoryTypes[i].heapIndex) +
                                " flags=" + to_string(mp.memoryTypes[i].propertyFlags));
        }
        vk_alloc_trace_line(vk_alloc_trace_budget_str(device));

#ifdef _WIN32
        char ** envp = _environ;
#else
        char ** envp = environ;
#endif
        for (char ** e = envp; e != nullptr && *e != nullptr; ++e) {
            const std::string entry(*e);
            if (entry.rfind("GGML_", 0) == 0 || entry.rfind("LLAMA_", 0) == 0 || entry.rfind("VK_", 0) == 0) {
                vk_alloc_trace_line("env " + entry);
            }
        }
    });
}

static void vk_alloc_trace_record_alloc(vk_device& device, VkBuffer buffer, uint64_t alloc_id,
                                        size_t request_size, const vk::MemoryRequirements & mem_req,
                                        uint32_t mtype, uint32_t heap) {
    uint64_t live_after = 0;
    {
        std::lock_guard<std::mutex> guard(vk_alloc_trace().mutex);
        vk_alloc_trace().heap_live[heap] += mem_req.size;
        vk_alloc_trace().by_buffer[buffer] = { heap, (uint64_t) mem_req.size };
        live_after = vk_alloc_trace().heap_live[heap];
    }

    std::stringstream ss;
    ss << "alloc id=" << alloc_id << " state=ok"
       << " request=" << request_size
       << " requirement=" << mem_req.size
       << " alignment=" << mem_req.alignment
       << " memoryTypeBits=0x" << std::hex << mem_req.memoryTypeBits << std::dec
       << " type=" << mtype << " heap=" << heap
       << " heap_live_after=" << live_after
       << " buffer=0x" << std::hex << (uint64_t) buffer << std::dec
       << " " << vk_alloc_trace_heap_live_str()
       << " " << vk_alloc_trace_budget_str(device);
    vk_alloc_trace_line(ss.str());
}

static void vk_alloc_trace_record_free(VkBuffer buffer) {
    if (!ggml_vk_alloc_trace_enabled()) {
        return;
    }

    uint32_t heap = 0;
    uint64_t bytes = 0;
    uint64_t live_after = 0;
    {
        std::lock_guard<std::mutex> guard(vk_alloc_trace().mutex);
        auto it = vk_alloc_trace().by_buffer.find(buffer);
        if (it == vk_alloc_trace().by_buffer.end()) {
            return;
        }
        heap  = it->second.first;
        bytes = it->second.second;
        uint64_t & live = vk_alloc_trace().heap_live[heap];
        live = (live >= bytes) ? (live - bytes) : 0;
        live_after = live;
        vk_alloc_trace().by_buffer.erase(it);
    }

    std::stringstream ss;
    ss << "free buffer=0x" << std::hex << (uint64_t) buffer << std::dec
       << " requirement=" << bytes << " heap=" << heap
       << " heap_live_after=" << live_after;
    vk_alloc_trace_line(ss.str());
}

// lane-232 / R47a: one entry per recorded transfer. A single upload can carry thousands of
// per-row regions, so the entry is a summary - count, total bytes, first and last region - not a
// line per region; that keeps the trace readable while still naming the buffers and extents that
// were in flight when a submit died.
void vk_alloc_trace_record_copy(vk_context & subctx, const char * tag, const ggml_tensor * tensor,
                                       VkBuffer src, VkBuffer dst, const vk::BufferCopy * slices, size_t n) {
    if (!ggml_vk_alloc_trace_enabled() || subctx == nullptr) {
        return;
    }

    uint64_t total = 0;
    for (size_t i = 0; i < n; i++) {
        total += slices[i].size;
    }

    std::stringstream ss;
    ss << tag
       << " tensor=" << (tensor && tensor->name[0] ? tensor->name : "-")
       << " src=0x" << std::hex << (uint64_t) src << " dst=0x" << (uint64_t) dst << std::dec
       << " regions=" << n << " bytes=" << total;
    if (n > 0) {
        ss << " first=[" << slices[0].srcOffset << "->" << slices[0].dstOffset << "," << slices[0].size << "]"
           << " last=["  << slices[n-1].srcOffset << "->" << slices[n-1].dstOffset << "," << slices[n-1].size << "]";
    }
    subctx->trace_ops.push_back(ss.str());
}

static vk_buffer ggml_vk_create_buffer(vk_device& device, size_t size, const std::initializer_list<vk::MemoryPropertyFlags> & req_flags_list,
                                       void *import_ptr = nullptr, uint32_t only_heap = UINT32_MAX) {
    VK_LOG_DEBUG("ggml_vk_create_buffer(" << device->name << ", " << size << ", " << to_string(req_flags_list.begin()[0]) << ", " << to_string(req_flags_list.begin()[req_flags_list.size()-1]) << ")");
    if (size > device->max_buffer_size) {
        throw vk::OutOfDeviceMemoryError("Requested buffer size exceeds device buffer size limit");
    }

    vk_buffer buf = std::make_shared<vk_buffer_struct>();

    if (size == 0) {
        buf->size = 0;
        return buf;
    }

    vk::BufferUsageFlags usage_flags = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
    vk::MemoryAllocateFlags mem_flags {};
    if (device->buffer_device_address) {
        usage_flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
        mem_flags |= vk::MemoryAllocateFlagBits::eDeviceAddress;
    }

    vk::BufferCreateInfo buffer_create_info{
        vk::BufferCreateFlags(),
        size,
        usage_flags,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
    };

    vk::ExternalMemoryBufferCreateInfo external_memory_bci;
    if (import_ptr) {
        external_memory_bci.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT;
        buffer_create_info.setPNext(&external_memory_bci);
    }

    buf->buffer = device->device.createBuffer(buffer_create_info);

    vk::MemoryRequirements mem_req = device->device.getBufferMemoryRequirements(buf->buffer);

    vk::PhysicalDeviceMemoryProperties mem_props = device->physical_device.getMemoryProperties();

    const vk::MemoryPriorityAllocateInfoEXT mem_priority_info { 1.0f };

    vk::MemoryAllocateFlagsInfo mem_flags_info { mem_flags };

    // lane-232 / R47a
    const bool     alloc_trace = ggml_vk_alloc_trace_enabled();
    const uint64_t alloc_id    = alloc_trace ? vk_alloc_trace_next_id() : 0;
    if (alloc_trace) {
        vk_alloc_trace_preamble(device);
        std::stringstream ss;
        ss << "alloc id=" << alloc_id << " state=attempt"
           << " request=" << size
           << " requirement=" << mem_req.size
           << " alignment=" << mem_req.alignment
           << " memoryTypeBits=0x" << std::hex << mem_req.memoryTypeBits << std::dec
           << " import=" << (import_ptr ? 1 : 0)
           << " flags_first=" << to_string(*req_flags_list.begin())
           << " " << vk_alloc_trace_heap_live_str()
           << " " << vk_alloc_trace_budget_str(device);
        vk_alloc_trace_line(ss.str());
    }

    if (device->memory_priority) {
        mem_flags_info.setPNext(&mem_priority_info);
    }

    if (import_ptr) {
        vk::MemoryHostPointerPropertiesEXT host_pointer_props;
        try {
            host_pointer_props = device->device.getMemoryHostPointerPropertiesEXT(vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT, import_ptr);
        } catch (vk::SystemError& e) {
            GGML_LOG_WARN("ggml_vulkan: Failed getMemoryHostPointerPropertiesEXT (%s)\n", e.what());
            device->device.destroyBuffer(buf->buffer);
            return {};
        }
        vk::PhysicalDeviceMemoryProperties mem_props = device->physical_device.getMemoryProperties();

        uint32_t memory_type_idx;
        vk::MemoryPropertyFlags property_flags = *req_flags_list.begin();
        for (memory_type_idx = 0; memory_type_idx < 32; ++memory_type_idx) {
            if (!(host_pointer_props.memoryTypeBits & (1u << memory_type_idx))) {
                continue;
            }
            if (!(mem_req.memoryTypeBits & (1u << memory_type_idx))) {
                continue;
            }

            vk::MemoryType memory_type = mem_props.memoryTypes[memory_type_idx];
            // check for visible+coherent+cached. Other flags (e.g. devicelocal) are allowed
            if ((memory_type.propertyFlags & property_flags) == property_flags) {
                property_flags = memory_type.propertyFlags;
                break;
            }
        }
        if (memory_type_idx == 32) {
            GGML_LOG_WARN("ggml_vulkan: Memory type for host allocation not found\n");
            device->device.destroyBuffer(buf->buffer);
            return {};
        }

        buf->memory_property_flags = mem_props.memoryTypes[memory_type_idx].propertyFlags;
        try {
            vk::ImportMemoryHostPointerInfoEXT import_info;
            import_info.handleType = vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT;
            import_info.pHostPointer = import_ptr;
            import_info.setPNext(&mem_flags_info);
            buf->device_memory = device->device.allocateMemory({ size, memory_type_idx, &import_info });
            if (alloc_trace) {
                vk_alloc_trace_record_alloc(device, buf->buffer, alloc_id, size, mem_req, memory_type_idx,
                                            mem_props.memoryTypes[memory_type_idx].heapIndex);
            }
        } catch (const vk::SystemError& e) {
            if (alloc_trace) {
                vk_alloc_trace_line("alloc id=" + std::to_string(alloc_id) + " state=fail import=1 type=" +
                                    std::to_string(memory_type_idx) + " result=" + e.code().message() + " what=" + e.what());
            }
        }
    } else {
        for (auto it = req_flags_list.begin(); it != req_flags_list.end(); it++) {
            const auto & req_flags = *it;

            const std::vector<uint32_t> memory_type_indices = ggml_vk_find_memory_properties(&mem_props, &mem_req, req_flags, only_heap);

            if (memory_type_indices.empty()) {
                if (alloc_trace) {
                    vk_alloc_trace_line("alloc id=" + std::to_string(alloc_id) +
                                        " state=no_candidate_type req_flags=" + to_string(req_flags));
                }
                continue;
            }

            bool done = false;

            for (auto mtype_it = memory_type_indices.begin(); mtype_it != memory_type_indices.end(); mtype_it++) {
                const uint32_t cand_heap = mem_props.memoryTypes[*mtype_it].heapIndex;
                if (alloc_trace) {
                    std::stringstream ss;
                    ss << "alloc id=" << alloc_id << " state=try"
                       << " type=" << *mtype_it << " heap=" << cand_heap
                       << " heap_size=" << mem_props.memoryHeaps[cand_heap].size
                       << " type_flags=" << to_string(mem_props.memoryTypes[*mtype_it].propertyFlags)
                       << " req_flags=" << to_string(req_flags);
                    vk_alloc_trace_line(ss.str());
                }
                try {
                    buf->device_memory = device->device.allocateMemory({ mem_req.size, *mtype_it, &mem_flags_info });
                    buf->memory_property_flags = mem_props.memoryTypes[*mtype_it].propertyFlags;
                    if (alloc_trace) {
                        vk_alloc_trace_record_alloc(device, buf->buffer, alloc_id, size, mem_req, *mtype_it, cand_heap);
                    }
                    done = true;
                    break;
                } catch (const vk::SystemError& e) {
                    if (alloc_trace) {
                        std::stringstream ss;
                        ss << "alloc id=" << alloc_id << " state=fail"
                           << " type=" << *mtype_it << " heap=" << cand_heap
                           << " result=" << e.code().message() << " what=" << e.what();
                        vk_alloc_trace_line(ss.str());
                    }
                    // loop and retry
                    // during last attempt throw the exception
                    if (it + 1 == req_flags_list.end() && mtype_it + 1 == memory_type_indices.end()) {
                        device->device.destroyBuffer(buf->buffer);
                        throw e;
                    }
                }
            }

            if (done) {
                break;
            }
        }
    }

    if (!buf->device_memory) {
        device->device.destroyBuffer(buf->buffer);
        throw vk::OutOfDeviceMemoryError("No suitable memory type found");
    }

    buf->ptr = nullptr;

    if (import_ptr) {
        buf->ptr = import_ptr;
    } else {
        if (buf->memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible) {
            buf->ptr = device->device.mapMemory(buf->device_memory, 0, VK_WHOLE_SIZE);
        }
    }

    device->device.bindBufferMemory(buf->buffer, buf->device_memory, 0);

    buf->device = device;
    buf->size = size;

    if (device->buffer_device_address) {
        const vk::BufferDeviceAddressInfo addressInfo(buf->buffer);
        buf->bda_addr = device->device.getBufferAddress(addressInfo);
    }

    device->memory_logger->log_allocation(buf, size);

    return buf;
}

vk_buffer ggml_vk_create_buffer_check(vk_device& device, size_t size, vk::MemoryPropertyFlags req_flags, vk::MemoryPropertyFlags fallback_flags) {
    try {
        return ggml_vk_create_buffer(device, size, {req_flags, fallback_flags});
    } catch (const vk::SystemError& e) {
        std::cerr << "ggml_vulkan: Memory allocation of size " << size << " failed." << std::endl;
        std::cerr << "ggml_vulkan: " << e.what() << std::endl;
        throw e;
    }
}

static vk_buffer ggml_vk_create_buffer_device(vk_device& device, size_t size, bool bulk = false) {
    vk_buffer buf;
    try {
        // lane-230 / R46b commit E: bulk weight buffers, policy ON. DEVICE_LOCAL on the largest
        // heap and nothing else. A failure here is NOT fatal: fall through to the stock chain
        // below, because a policy that turns a working load into a hard failure is worse than the
        // placement it is trying to fix.
        if (bulk && ggml_vk_placement_bulk_large_heap()) {
            const vk::PhysicalDeviceMemoryProperties mem_props = device->physical_device.getMemoryProperties();
            const uint32_t heap = ggml_vk_largest_heap(mem_props);
            // stderr, not GGML_LOG_INFO, and NOT gated on GGML_VK_ALLOC_TRACE: the paired
            // performance cells for this policy run with tracing off and still have to prove which
            // heap the run used.
            static std::mutex receipt_mutex;
            static std::set<uint32_t> receipted;
            bool first = false;
            {
                std::lock_guard<std::mutex> guard(receipt_mutex);
                first = receipted.insert(heap).second;
            }
            if (first) {
                fprintf(stderr, "ggml_vulkan: placement policy bulk-large-heap: bulk weights -> heap %u "
                                "(size=%llu B, DEVICE_LOCAL only, every other heap excluded)\n",
                        heap, (unsigned long long) mem_props.memoryHeaps[heap].size);
            }
            try {
                return ggml_vk_create_buffer(device, size, {vk::MemoryPropertyFlagBits::eDeviceLocal}, nullptr, heap);
            } catch (const vk::SystemError& e) {
                fprintf(stderr, "ggml_vulkan: placement policy bulk-large-heap: heap %u refused %llu B (%s); "
                                "falling back to the default placement chain for this buffer\n",
                        heap, (unsigned long long) size, e.what());
            }
        }

        if (device->prefer_host_memory) {
            buf = ggml_vk_create_buffer(device, size, {vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                                       vk::MemoryPropertyFlagBits::eDeviceLocal});
        } else if (device->uma) {
            // On UMA, prefer host-visible memory so direct tensor borrowing works.
            // If unavailable, fall back to device-local memory.
            buf = ggml_vk_create_buffer(device, size, {vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                                       vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent});
        } else if (device->disable_host_visible_vidmem) {
            if (device->allow_sysmem_fallback) {
                buf = ggml_vk_create_buffer(device, size, {vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent});
            } else {
                buf = ggml_vk_create_buffer(device, size, {vk::MemoryPropertyFlagBits::eDeviceLocal});
            }
        } else {
            // use rebar if available, otherwise fallback to device only visible memory
            if (device->allow_sysmem_fallback) {
                buf = ggml_vk_create_buffer(device, size, {vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                                           vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent});
            } else {
                buf = ggml_vk_create_buffer(device, size, {vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                                           vk::MemoryPropertyFlagBits::eDeviceLocal});
            }
        }
    } catch (const vk::SystemError& e) {
        std::cerr << "ggml_vulkan: Device memory allocation of size " << size << " failed." << std::endl;
        std::cerr << "ggml_vulkan: " << e.what() << std::endl;
        throw e;
    }

    return buf;
}

void ggml_vk_destroy_buffer(vk_buffer& buf) {
    if (buf == nullptr) {
        return;
    }

    if (buf->device != nullptr) {
        buf->device->memory_logger->log_deallocation(buf);
    }
    vk_alloc_trace_record_free(buf->buffer);  // lane-232 / R47a

    buf.reset();
}

void * ggml_vk_host_malloc(vk_device& device, size_t size) {
    VK_LOG_MEMORY("ggml_vk_host_malloc(" << size << ")");
    vk_buffer buf = ggml_vk_create_buffer(device, size,
        {vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached,
         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent});

    if(!(buf->memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible)) {
        fprintf(stderr, "WARNING: failed to allocate %.2f MB of pinned memory\n",
            size/1024.0/1024.0);
        device->device.freeMemory(buf->device_memory);
        device->device.destroyBuffer(buf->buffer);
        return nullptr;
    }

    std::lock_guard<std::shared_mutex> guard(device->pinned_memory_mutex);
    device->pinned_memory.push_back(std::make_tuple(buf->ptr, size, buf));

    return buf->ptr;
}

void ggml_vk_host_free(vk_device& device, void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    VK_LOG_MEMORY("ggml_vk_host_free(" << ptr << ")");
    std::lock_guard<std::shared_mutex> guard(device->pinned_memory_mutex);

    vk_buffer buf;
    size_t index;
    for (size_t i = 0; i < device->pinned_memory.size(); i++) {
        const uint8_t* addr = (const uint8_t*) std::get<0>(device->pinned_memory[i]);
        const uint8_t* endr = addr + std::get<1>(device->pinned_memory[i]);
        if (ptr >= addr && ptr < endr) {
            buf = std::get<2>(device->pinned_memory[i]);
            index = i;
            break;
        }
    }
    if (buf == nullptr) {
        fprintf(stderr, "WARNING: failed to free pinned memory: memory not in map\n");
        return;
    }

    ggml_vk_destroy_buffer(buf);

    device->pinned_memory.erase(device->pinned_memory.begin() + index);
}

void ggml_vk_host_get(const vk_device& device, const void * ptr, vk_buffer& buf, size_t& buf_offset) {
    std::shared_lock<std::shared_mutex> guard(device->pinned_memory_mutex);
    buf = nullptr;
    buf_offset = 0;
    for (size_t i = 0; i < device->pinned_memory.size(); i++) {
        const uint8_t* addr = (const uint8_t*) std::get<0>(device->pinned_memory[i]);
        const uint8_t* endr = addr + std::get<1>(device->pinned_memory[i]);
        if (ptr >= addr && ptr < endr) {
            buf = std::get<2>(device->pinned_memory[i]);
            buf_offset = ((const uint8_t *)ptr) - addr;
            break;
        }
    }
}

void ggml_vk_ensure_sync_staging_buffer(vk_device& device, size_t size) {
    if (device->sync_staging == nullptr || device->sync_staging->size < size) {
        VK_LOG_MEMORY("ggml_vk_ensure_sync_staging_buffer(" << size << ")");
        ggml_vk_destroy_buffer(device->sync_staging);
        device->sync_staging = ggml_vk_create_buffer_check(device, size,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}

void ggml_vk_ensure_sync_staging_buffer(ggml_backend_vk_context * ctx, size_t size) {
    if (ctx->sync_staging == nullptr || ctx->sync_staging->size < size) {
        VK_LOG_MEMORY("ggml_vk_ensure_sync_staging_buffer(" << size << ")");
        ggml_vk_destroy_buffer(ctx->sync_staging);
        ctx->sync_staging = ggml_vk_create_buffer_check(ctx->device, size,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}

static void ggml_vk_buffer_write_nc_async(ggml_backend_vk_context * ctx, vk_context& subctx, vk_buffer& dst, size_t offset, const ggml_tensor * tensor, bool sync_staging = false) {
    VK_LOG_DEBUG("ggml_vk_buffer_write_nc_async(" << tensor << ")");
    GGML_ASSERT(!ggml_is_contiguous(tensor));
    // Buffer is already mapped
    if(dst->memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible) {
        std::cerr << "ggml_vulkan: buffer_write_nc_async dst buffer is host_visible. Use synchronous write." << std::endl;
        GGML_ABORT("fatal error");
    }
    // Check if src is pinned memory
    vk_buffer buf = nullptr;
    size_t buf_offset = 0;
    ggml_vk_host_get(ctx->device, tensor->data, buf, buf_offset);

    const uint64_t ne0 = tensor->ne[0];
    const uint64_t ne1 = tensor->ne[1];
    const uint64_t ne2 = tensor->ne[2];
    const uint64_t ne3 = tensor->ne[3];
    const uint64_t nb0 = tensor->nb[0];
    const uint64_t nb1 = tensor->nb[1];
    const uint64_t nb2 = tensor->nb[2];
    const uint64_t nb3 = tensor->nb[3];
    const ggml_type type = tensor->type;
    const uint64_t ts = ggml_type_size(type);
    const uint64_t bs = ggml_blck_size(type);

    const uint64_t dstnb0 = ts;
    const uint64_t dstnb1 = dstnb0*(ne0/bs);
    const uint64_t dstnb2 = dstnb1*ne1;
    const uint64_t dstnb3 = dstnb2*ne2;

    const uint64_t ne = ggml_nelements(tensor);

    if (buf != nullptr) {
        // Memory is pinned, use as staging buffer
        std::vector<vk::BufferCopy> slices;

        for (uint64_t i3 = 0; i3 < ne3; i3++) {
            for (uint64_t i2 = 0; i2 < ne2; i2++) {
                // Find longest contiguous slice
                if (ne1*nb1 == dstnb2) {
                    slices.push_back({ buf_offset + i3*nb3 + i2*nb2, offset + i3*dstnb3 + i2*dstnb2, dstnb2 });
                } else {
                    for (uint64_t i1 = 0; i1 < ne1; i1++) {
                        if (ne0*nb0/bs == dstnb1) {
                            slices.push_back({ buf_offset + i3*nb3 + i2*nb2 + i1*nb1, offset + i3*dstnb3 + i2*dstnb2 + i1*dstnb1, dstnb1 });
                        } else {
                            const uint64_t s_off = buf_offset + i3*nb3 + i2*nb2 + i1*nb1;
                            const uint64_t d_off = offset + i3*dstnb3 + i2*dstnb2 + i1*dstnb1;
                            for (uint64_t i0 = 0; i0 < ne0; i0++) {
                                slices.push_back({ s_off + i0*nb0, d_off + i0*dstnb0, dstnb0 });
                            }
                        }
                    }
                }
            }
        }

        ggml_vk_sync_buffers(ctx, subctx);
        vk_alloc_trace_record_copy(subctx, "write_nc_async/pinned", tensor, buf->buffer, dst->buffer, slices.data(), slices.size());
        subctx->s->buffer->buf.copyBuffer(buf->buffer, dst->buffer, slices);
        return;
    }

    if (!sync_staging) {
        GGML_ABORT("Asynchronous write to non-pinned memory not supported");
    }

    // Staging buffer required
    vk_buffer& staging = ctx->device->sync_staging;
    const uint64_t copy_size = ts*ne/bs;
    ggml_vk_ensure_sync_staging_buffer(ctx->device, copy_size);
    VkBufferCopy buf_copy{ 0, offset, copy_size };

    ggml_vk_sync_buffers(ctx, subctx);
    {
        const vk::BufferCopy trace_slice { buf_copy.srcOffset, buf_copy.dstOffset, buf_copy.size };
        vk_alloc_trace_record_copy(subctx, "write_nc_async/staging", tensor, (VkBuffer)staging->buffer, (VkBuffer)dst->buffer, &trace_slice, 1);
    }
    vkCmdCopyBuffer(subctx->s->buffer->buf, (VkBuffer)staging->buffer, (VkBuffer)dst->buffer, 1, &buf_copy);

    for (uint64_t i3 = 0; i3 < ne3; i3++) {
        for (uint64_t i2 = 0; i2 < ne2; i2++) {
            // Find longest contiguous slice
            if (ne1*nb1 == dstnb2) {
                deferred_memcpy((uint8_t *)staging->ptr + i3*dstnb3 + i2*dstnb2, (const uint8_t *) tensor->data + buf_offset + i3*nb3 + i2*nb2, dstnb2, &subctx->in_memcpys);
            } else {
                for (uint64_t i1 = 0; i1 < ne1; i1++) {
                    if (ne0*nb0/bs == dstnb1) {
                        deferred_memcpy((uint8_t *)staging->ptr + i3*dstnb3 + i2*dstnb2 + i1*dstnb1, (const uint8_t *) tensor->data + buf_offset + i3*nb3 + i2*nb2 + i1*nb1, dstnb1, &subctx->in_memcpys);
                    } else {
                        const uint64_t s_off = buf_offset + i3*nb3 + i2*nb2 + i1*nb1;
                        const uint64_t d_off = i3*dstnb3 + i2*dstnb2 + i1*dstnb1;
                        for (uint64_t i0 = 0; i0 < ne0; i0++) {
                            deferred_memcpy((uint8_t *)staging->ptr + d_off + i0*dstnb0, (const uint8_t *) tensor->data + s_off + i0*nb0, dstnb0, &subctx->in_memcpys);
                        }
                    }
                }
            }
        }
    }
}

bool ggml_vk_buffer_write_2d_async(vk_context subctx, vk_buffer& dst, size_t offset, const void * src, size_t spitch, size_t dpitch, size_t width, size_t height, bool sync_staging) {
    VK_LOG_DEBUG("ggml_vk_buffer_write_2d_async(" << width << ", " << height << ")");
    // Check if src is pinned memory
    vk_buffer buf = nullptr;
    size_t buf_offset = 0;
    ggml_vk_host_get(dst->device, src, buf, buf_offset);

    if (buf != nullptr) {
        // Memory is pinned, use as staging buffer
        std::vector<vk::BufferCopy> slices(1);
        if (width == spitch && width == dpitch) {
            // Only do single write if stride is equal
            slices[0].srcOffset = buf_offset;
            slices[0].dstOffset = offset;
            slices[0].size = width * height;
        } else {
            slices.resize(height);
            for (size_t i = 0; i < height; i++) {
                slices[i].srcOffset = buf_offset + i * spitch;
                slices[i].dstOffset = offset + i * dpitch;
                slices[i].size = width;
            }
        }

        ggml_vk_sync_buffers(nullptr, subctx);
        vk_alloc_trace_record_copy(subctx, "write_2d_async/pinned", nullptr, buf->buffer, dst->buffer, slices.data(), slices.size());
        subctx->s->buffer->buf.copyBuffer(buf->buffer, dst->buffer, slices);
        return true;
    }
    VK_LOG_DEBUG("STAGING");

    if (!sync_staging) {
        // copy was not handled caller needs to fall back
        return false;
    }

    // Staging buffer required
    const size_t staging_size = width * height;
    ggml_vk_ensure_sync_staging_buffer(dst->device, staging_size);

    vk_buffer& staging_buffer = dst->device->sync_staging;

    std::vector<vk::BufferCopy> slices(1);
    if (width == dpitch) {
        slices[0].srcOffset = 0;
        slices[0].dstOffset = offset;
        slices[0].size = staging_size;
    } else {
        slices.resize(height);
        for (size_t i = 0; i < height; i++) {
            slices[i].srcOffset = i * width;
            slices[i].dstOffset = offset + i * dpitch;
            slices[i].size = width;
        }
    }

    ggml_vk_sync_buffers(nullptr, subctx);
    vk_alloc_trace_record_copy(subctx, "write_2d_async/staging", nullptr, (VkBuffer)staging_buffer->buffer, (VkBuffer)dst->buffer, slices.data(), slices.size());
    subctx->s->buffer->buf.copyBuffer(staging_buffer->buffer, dst->buffer, slices);

    if (width == spitch) {
        deferred_memcpy((uint8_t *)staging_buffer->ptr, src, staging_size, &subctx->in_memcpys);
    } else {
        for (size_t i = 0; i < height; i++) {
            deferred_memcpy((uint8_t *)staging_buffer->ptr + i * width, (const uint8_t *) src + i * spitch, width, &subctx->in_memcpys);
        }
    }
    return true;
}

bool ggml_vk_buffer_write_async(vk_context subctx, vk_buffer& dst, size_t offset, const void * src, size_t size, bool sync_staging) {
    VK_LOG_DEBUG("ggml_vk_buffer_write_async(" << size << ")");
    return ggml_vk_buffer_write_2d_async(subctx, dst, offset, src, size, size, size, 1, sync_staging);
}

void ggml_vk_buffer_write_2d(vk_buffer& dst, size_t offset, const void * src, size_t spitch, size_t dpitch, size_t width, size_t height) {
    VK_LOG_DEBUG("ggml_vk_buffer_write_2d(" << width << ", " << height << ")");
    // Buffer is already mapped
    if(dst->memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible) {
        GGML_ASSERT(dst->memory_property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);

        if (width == spitch && width == dpitch) {
            memcpy((uint8_t *)dst->ptr + offset, src, width * height);
        } else {
            for (size_t i = 0; i < height; i++) {
                memcpy((uint8_t *)dst->ptr + offset + i * dpitch, (const uint8_t *) src + i * spitch, width);
            }
        }
    } else {
        std::lock_guard<std::recursive_mutex> guard(dst->device->mutex);

        vk_context subctx = ggml_vk_create_temporary_context(dst->device->transfer_queue->cmd_pool);
        ggml_vk_ctx_begin(dst->device, subctx);
        bool ret = ggml_vk_buffer_write_2d_async(subctx, dst, offset, src, spitch, dpitch, width, height, true);
        GGML_ASSERT(ret);
        ggml_vk_ctx_end(subctx);

        for (auto& cpy : subctx->in_memcpys) {
            memcpy(cpy.dst, cpy.src, cpy.n);
        }

        for (auto& mset : subctx->memsets) {
            memset(mset.dst, mset.val, mset.n);
        }

        ggml_vk_submit(subctx, dst->device->fence);
        VK_CHECK(dst->device->device.waitForFences({ dst->device->fence }, true, UINT64_MAX), "vk_buffer_write_2d waitForFences", dst->device);
        dst->device->device.resetFences({ dst->device->fence });
        ggml_vk_queue_command_pools_cleanup(dst->device);
    }
}

void ggml_vk_buffer_write(vk_buffer& dst, size_t offset, const void * src, size_t size) {
    VK_LOG_DEBUG("ggml_vk_buffer_write(" << size << ")");
    ggml_vk_buffer_write_2d(dst, offset, src, size, size, size, 1);
}

bool ggml_vk_buffer_read_2d_async(vk_context subctx, vk_buffer& src, size_t offset, void * dst, size_t spitch, size_t dpitch, size_t width, size_t height, bool sync_staging) {
    VK_LOG_DEBUG("ggml_vk_buffer_read_2d_async(offset=" << offset << ", width=" << width << ", height=" << height << ")");
    GGML_ASSERT(width > 0);
    GGML_ASSERT(height > 0);
    GGML_ASSERT(src != nullptr);

    // TODO: staging_offset is not used

    // Check if dst is pinned memory
    vk_buffer buf = nullptr;
    size_t buf_offset = 0;
    ggml_vk_host_get(src->device, dst, buf, buf_offset);

    std::vector<vk::BufferCopy> slices(1);
    if (width == spitch && width == dpitch) {
        // Only do single write if stride is equal
        slices[0].srcOffset = offset;
        slices[0].dstOffset = buf_offset;
        slices[0].size = width * height;
    } else {
        slices.resize(height);
        for (size_t i = 0; i < height; i++) {
            slices[i].srcOffset = offset + i * spitch;
            slices[i].dstOffset = buf_offset + i * dpitch;
            slices[i].size = width;
        }
    }

    if (buf != nullptr) {
        // Memory is pinned, use as staging buffer
        ggml_vk_sync_buffers(nullptr, subctx);
        vk_alloc_trace_record_copy(subctx, "read_2d_async/pinned", nullptr, src->buffer, buf->buffer, slices.data(), slices.size());
        subctx->s->buffer->buf.copyBuffer(src->buffer, buf->buffer, slices);

        return true;
    }
    VK_LOG_DEBUG("STAGING");

    if (!sync_staging) {
        // copy was not handled caller needs to fall back
        return false;
    }

    // Fall back to staging buffer
    const size_t staging_size = width * height;
    ggml_vk_ensure_sync_staging_buffer(src->device, staging_size);

    vk_buffer& staging_buffer = src->device->sync_staging;

    std::vector<vk::BufferCopy> staging_slices(1);
    if (width == spitch) {
        staging_slices[0].srcOffset = offset;
        staging_slices[0].dstOffset = 0;
        staging_slices[0].size = staging_size;
    } else {
        staging_slices.resize(height);
        for (size_t i = 0; i < height; i++) {
            staging_slices[i].srcOffset = offset + i * spitch;
            staging_slices[i].dstOffset = i * width;
            staging_slices[i].size = width;
        }
    }

    ggml_vk_sync_buffers(nullptr, subctx);
    vk_alloc_trace_record_copy(subctx, "read_2d_async/staging", nullptr, src->buffer, staging_buffer->buffer, staging_slices.data(), staging_slices.size());
    subctx->s->buffer->buf.copyBuffer(src->buffer, staging_buffer->buffer, staging_slices);

    if (width == dpitch) {
        deferred_memcpy(dst, staging_buffer->ptr, staging_size, &subctx->out_memcpys);
    } else {
        for (size_t i = 0; i < height; i++) {
            deferred_memcpy((uint8_t *) dst + i * dpitch, (const uint8_t *) staging_buffer->ptr + i * width, width, &subctx->out_memcpys);
        }
    }
    return true;
}

static bool ggml_vk_buffer_read_async(vk_context subctx, vk_buffer& src, size_t offset, void * dst, size_t size, bool sync_staging = false) {
    return ggml_vk_buffer_read_2d_async(subctx, src, offset, dst, size, size, size, 1, sync_staging);
}

// arifi lane-212 (WI-1693 fix A, flashnext-hybrid fix 3 ported to Vulkan UMA): the direct host memcpy
// below is only the FAST read path when the mapping is HOST_CACHED. ggml_vk_create_buffer_device's UMA
// branch asks for DeviceLocal|HostVisible|HostCoherent and never for HostCached, and on a Radeon 780M no
// memory type carries DEVICE_LOCAL together with HOST_CACHED at all (vulkaninfo, 16 types: cached only on
// heap 1), so every UMA tensor buffer is mapped write-combined and a CPU read out of it is the classic
// uncached-read penalty. The staging branch (device copy into sync_staging, which IS allocated HostCached
// first, then a cached memcpy) already pays the same one submit + one fence, so it costs no extra round trip.
//
// RUNTIME SWITCH, never compile-time: probe = the buffer's OWN matched memory-type flags (stored at
// allocation from the device's table), per call. A UMA device whose host-visible type is cached (Intel
// iGPUs commonly are) keeps the upstream direct memcpy untouched. Override for A/B and for anyone whose
// driver measures differently: GGML_ARIFI_UMA_READ_PATH=auto (default, probe) | direct (upstream
// behaviour) | staging (always staging). Writes are not touched: write-combined is the right mode for
// CPU writes, and ggml_vk_buffer_write_2d keeps its memcpy branch.
static bool ggml_vk_uma_direct_read_ok(const vk_buffer & src) {
    if (!(src->memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible) || !src->device->uma) {
        return false;
    }
    static const int mode = [] {
        const char * e = getenv("GGML_ARIFI_UMA_READ_PATH");
        if (e == nullptr || strcmp(e, "auto") == 0) return 0;
        if (strcmp(e, "direct") == 0)  return 1;
        if (strcmp(e, "staging") == 0) return 2;
        fprintf(stderr, "ggml_vulkan: GGML_ARIFI_UMA_READ_PATH=%s not understood (auto|direct|staging), using auto\n", e);
        return 0;
    }();
    const bool cached = bool(src->memory_property_flags & vk::MemoryPropertyFlagBits::eHostCached);
    const bool direct = mode == 1 || (mode == 0 && cached);
    static bool once = false;
    if (!once) {
        once = true;
        // stderr, not GGML_LOG_INFO: llama-server drops ggml INFO records (see the q6_k receipt above).
        // This receipt is how an A/B log proves which path the arm actually took.
        fprintf(stderr, "ggml_vulkan: ARIFI UMA read path = %s [GGML_ARIFI_UMA_READ_PATH=%s]\n",
                direct ? (mode == 1 ? "direct (forced)" : "direct (mapping is HOST_CACHED)")
                       : (mode == 2 ? "staging (forced)" : "staging (mapping lacks HOST_CACHED)"),
                mode == 1 ? "direct" : mode == 2 ? "staging" : "auto");
    }
    return direct;
}

void ggml_vk_buffer_read_2d(vk_buffer& src, size_t offset, void * dst, size_t spitch, size_t dpitch, size_t width, size_t height) {
    VK_LOG_DEBUG("ggml_vk_buffer_read_2d(" << src->buffer << ", " << offset << ", " << width << ", " << height << ")");

    // If the device is not an UMA device the memory is host-accessible through rebar. While writing
    // through PCIe is sufficient fast reading back data from PCIe is slower than going through
    // the HW device to host copy path.
    // arifi lane-212: on UMA the direct memcpy is taken only through ggml_vk_uma_direct_read_ok
    // (HOST_CACHED probe + GGML_ARIFI_UMA_READ_PATH override); an uncached mapping falls through to
    // the staging branch below, which every non-UMA host-visible (rebar) buffer already uses.
    if (ggml_vk_uma_direct_read_ok(src)) {
        GGML_ASSERT(src->memory_property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);

        std::lock_guard<std::recursive_mutex> guard(src->device->mutex);
        vk_context subctx = ggml_vk_create_temporary_context(src->device->compute_queue->cmd_pool);
        ggml_vk_ctx_begin(src->device, subctx);
        subctx->s->buffer->buf.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eHost,
            {},
            { { vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eTransferWrite,
                vk::AccessFlagBits::eHostRead } },
            {}, {});
        ggml_vk_ctx_end(subctx);
        ggml_vk_submit(subctx, src->device->fence);
        VK_CHECK(src->device->device.waitForFences({ src->device->fence }, true, UINT64_MAX),
                 "vk_buffer_read_2d uma waitForFences", src->device);
        src->device->device.resetFences({ src->device->fence });
        ggml_vk_queue_command_pools_cleanup(src->device);

        if (width == spitch && width == dpitch) {
            memcpy(dst, (const uint8_t *) src->ptr + offset, width * height);
        } else {
            for (size_t i = 0; i < height; i++) {
                memcpy((uint8_t *) dst + i * dpitch, (const uint8_t *) src->ptr + offset + i * spitch, width);
            }
        }
    } else {
        std::lock_guard<std::recursive_mutex> guard(src->device->mutex);

        vk_context subctx = ggml_vk_create_temporary_context(src->device->transfer_queue->cmd_pool);
        ggml_vk_ctx_begin(src->device, subctx);
        bool ret = ggml_vk_buffer_read_2d_async(subctx, src, offset, dst, spitch, dpitch, width, height, true);
        GGML_ASSERT(ret);
        ggml_vk_ctx_end(subctx);

        ggml_vk_submit(subctx, src->device->fence);
        VK_CHECK(src->device->device.waitForFences({ src->device->fence }, true, UINT64_MAX), "vk_buffer_read_2d waitForFences", src->device);
        src->device->device.resetFences({ src->device->fence });
        ggml_vk_queue_command_pools_cleanup(src->device);

        for (auto& cpy : subctx->out_memcpys) {
            memcpy(cpy.dst, cpy.src, cpy.n);
        }
    }
}

void ggml_vk_buffer_read(vk_buffer& src, size_t offset, void * dst, size_t size) {
    VK_LOG_DEBUG("ggml_vk_buffer_read(" << src->buffer << ", " << offset << ", " << size << ")");
    ggml_vk_buffer_read_2d(src, offset, dst, size, size, size, 1);
}

void ggml_vk_buffer_copy_async(vk_context& ctx, vk_buffer& dst, size_t dst_offset, vk_buffer& src, size_t src_offset, size_t size) {
    VK_LOG_DEBUG("ggml_vk_buffer_copy_async(" << size << ")");
    // Make sure both buffers are on same device
    GGML_ASSERT(src->device == dst->device);

    VkBufferCopy bc{ src_offset, dst_offset, size };

    vkCmdCopyBuffer(ctx->s->buffer->buf, (VkBuffer)src->buffer, (VkBuffer)dst->buffer, 1, &bc);
}

void ggml_vk_buffer_copy(vk_buffer& dst, size_t dst_offset, vk_buffer& src, size_t src_offset, size_t size) {
    if (src->device == dst->device) {
        std::lock_guard<std::recursive_mutex> guard(src->device->mutex);
        VK_LOG_DEBUG("ggml_vk_buffer_copy(SINGLE_DEVICE, " << size << ")");
        // Copy within the device
        vk_context subctx = ggml_vk_create_temporary_context(src->device->transfer_queue->cmd_pool);
        ggml_vk_ctx_begin(src->device, subctx);
        ggml_vk_buffer_copy_async(subctx, dst, dst_offset, src, src_offset, size);
        ggml_vk_ctx_end(subctx);
        ggml_vk_submit(subctx, src->device->fence);
        VK_CHECK(src->device->device.waitForFences({ src->device->fence }, true, UINT64_MAX), "vk_buffer_copy waitForFences", src->device);
        src->device->device.resetFences({ src->device->fence });
        ggml_vk_queue_command_pools_cleanup(src->device);
    } else {
        VK_LOG_DEBUG("ggml_vk_buffer_copy(MULTI_DEVICE, " << size << ")");
        // Copy device to device
        ggml_vk_ensure_sync_staging_buffer(src->device, size);

        // Copy to src staging buffer
        ggml_vk_buffer_copy(src->device->sync_staging, 0, src, src_offset, size);
        // Copy to dst buffer
        ggml_vk_buffer_write(dst, dst_offset, src->device->sync_staging->ptr, size);
    }
}

void ggml_vk_buffer_memset_async(vk_context& ctx, vk_buffer& dst, size_t offset, uint32_t c, size_t size) {
    VK_LOG_DEBUG("ggml_vk_buffer_memset_async(" << offset << ", " << c << ", " << size << ")");

    if (dst->memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible &&
        dst->device->uma) {
        deferred_memset((uint8_t*)dst->ptr + offset, c, size, &ctx->memsets);
        return;
    }

    // Fall back to GPU fillBuffer for non-UMA or non-host-visible buffers
    ctx->s->buffer->buf.fillBuffer(dst->buffer, offset, size, c);
}

void ggml_vk_buffer_memset(vk_buffer& dst, size_t offset, uint32_t c, size_t size) {
    VK_LOG_DEBUG("ggml_vk_buffer_memset(" << offset << ", " << c << ", " << size << ")");

    if (dst->memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible &&
        dst->device->uma) {
        memset((uint8_t*)dst->ptr + offset, c, size);
        return;
    }

    std::lock_guard<std::recursive_mutex> guard(dst->device->mutex);
    vk_context subctx = ggml_vk_create_temporary_context(dst->device->transfer_queue->cmd_pool);
    ggml_vk_ctx_begin(dst->device, subctx);
    subctx->s->buffer->buf.fillBuffer(dst->buffer, offset, size, c);
    ggml_vk_ctx_end(subctx);

    ggml_vk_submit(subctx, dst->device->fence);
    VK_CHECK(dst->device->device.waitForFences({ dst->device->fence }, true, UINT64_MAX), "vk_memset waitForFences", dst->device);
    dst->device->device.resetFences({ dst->device->fence });
    ggml_vk_queue_command_pools_cleanup(dst->device);
}

ggml_backend_buffer_i ggml_backend_vk_buffer_interface = {
    /* .free_buffer     = */ ggml_backend_vk_buffer_free_buffer,
    /* .get_base        = */ ggml_backend_vk_buffer_get_base,
    /* .init_tensor     = */ ggml_backend_vk_buffer_init_tensor,
    /* .memset_tensor   = */ ggml_backend_vk_buffer_memset_tensor,
    /* .set_tensor      = */ ggml_backend_vk_buffer_set_tensor,
    /* .get_tensor      = */ ggml_backend_vk_buffer_get_tensor,
    /* .set_tensor_2d   = */ ggml_backend_vk_buffer_set_tensor_2d,
    /* .get_tensor_2d   = */ ggml_backend_vk_buffer_get_tensor_2d,
    /* .cpy_tensor      = */ ggml_backend_vk_buffer_cpy_tensor,
    /* .clear           = */ ggml_backend_vk_buffer_clear,
    /* .reset           = */ NULL,
};

vk_buffer ggml_vk_buffer_from_host_ptr(vk_device & device, void * ptr, size_t size) {
    if (!device->external_memory_host) {
        return {};
    }

    uintptr_t uptr = reinterpret_cast<uintptr_t>(ptr);
    if (uptr & (device->min_imported_host_pointer_alignment - 1)) {
        return {};
    }
    if (size & (device->min_imported_host_pointer_alignment - 1)) {
        return {};
    }

    const vk::MemoryPropertyFlags property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;

    vk_buffer buf {};
    try {
        buf = ggml_vk_create_buffer(device, size, { property_flags }, ptr);
    } catch (vk::SystemError& e) {
        GGML_LOG_WARN("ggml_vulkan: Failed ggml_vk_create_buffer (%s)\n", e.what());
    }

    return buf;
}

