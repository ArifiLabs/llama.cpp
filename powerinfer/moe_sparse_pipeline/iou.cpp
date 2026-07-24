// lane-110 M2b Windows port: the disk-streamed expert cache uses IOCP-backed
// positioned reads. Linux retains the vendored io_uring implementation.

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#else
#include <windows.h>
#endif
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "powerinfer-log.hpp"
#include "moe_sparse_pipeline/config.hpp"
#include "moe_sparse_pipeline/iou.hpp"

namespace moe_sparse_pipeline {

#if !defined(__linux__)
// ---------------- Windows: IOCP (default) or synchronous positioned reads ----------------

// Runtime toggle, read once. Matches the lane110_prof_enabled() idiom in
// ggml/src/ggml-cpu/ops.cpp: exact single-character parse, no per-call getenv.
// Default ON; only an exact "0" selects the pre-M2b synchronous path.
bool iocp_enabled() {
    static const bool enabled = []() {
        const char * value = std::getenv("POWERINFER_IOCP");
        return !(value != nullptr && value[0] == '0' && value[1] == '\0');
    }();
    return enabled;
}

IOUring::IOUring(const std::string &path, size_t queue_depth) :
    queue_depth(queue_depth),
    req_data_buf(queue_depth),
    completion_entries(queue_depth) {
    POWERINFER_ASSERT(queue_depth > 0);

    // FILE_FLAG_OVERLAPPED only in IOCP mode: the synchronous path needs a
    // blocking handle so ReadFile completes in place, exactly as pre-M2b.
    const DWORD create_flags = iocp_enabled()
        ? (FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_OVERLAPPED)
        : FILE_FLAG_RANDOM_ACCESS;

    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, create_flags, nullptr);
    POWERINFER_ASSERT(h != INVALID_HANDLE_VALUE);
    handle = h;

    if (iocp_enabled()) {
        HANDLE port = CreateIoCompletionPort(h, nullptr, 0, 0);
        POWERINFER_ASSERT(port != nullptr);
        completion_port = port;
    }

    pending.reserve(queue_depth);
}

IOUring::~IOUring() {
    // ExpertCache joins the worker before this destructor runs.  Closing an
    // IOCP with live OVERLAPPED storage would violate request ownership.
    POWERINFER_ASSERT(n_inflight == 0);
    POWERINFER_ASSERT(pending.empty());

    if (completion_port) {
        CloseHandle(completion_port);
        completion_port = nullptr;
    }
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

void IOUring::enqueue_read(void *buffer, size_t offset, size_t size, void *user_data, CallbackFn *callback) {
    POWERINFER_ASSERT(n_inflight < queue_depth);
    POWERINFER_ASSERT(size <= MAXDWORD);

    auto &request = req_data_buf[(req_data_buf_pos++) % queue_depth];
    POWERINFER_ASSERT(!request.in_flight);

    request.overlapped = {};
    request.overlapped.Offset     = static_cast<DWORD>(offset & 0xFFFFFFFFull);
    request.overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
    request.read_size = size;
    request.user_data = user_data;
    request.callback = callback;
    request.in_flight = true;
    ++n_inflight;

    if (!iocp_enabled()) {
        // Pre-M2b synchronous positioned read: OVERLAPPED carries the offset, so
        // there is no shared file pointer. The read completes here and reap()
        // fires the callback; submit_and_wait is a no-op in this mode.
        DWORD got = 0;
        BOOL ok = ReadFile(handle, buffer, static_cast<DWORD>(size), &got, &request.overlapped);
        if (!ok || got != size) {
            fprintf(stderr, "IOUring(win) read error: expect %zu, got %lu (err %lu)\n",
                    size,
                    static_cast<unsigned long>(got),
                    static_cast<unsigned long>(GetLastError()));
            abort();
        }
        pending.push_back({&request, got});
        return;
    }

    // Do not synthesize a completion for synchronous success.  Since this
    // handle has not opted into FILE_SKIP_COMPLETION_PORT_ON_SUCCESS, both
    // synchronous and pending reads produce exactly one IOCP completion.
    if (!ReadFile(handle, buffer, static_cast<DWORD>(size), nullptr, &request.overlapped)) {
        const DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            return;
        }
        fprintf(stderr, "IOUring(win) enqueue error: size %zu (err %lu)\n",
                size, static_cast<unsigned long>(error));
        abort();
    }
}

void IOUring::submit_and_wait(size_t wait_nr) {
    if (!iocp_enabled()) {
        // No-op: reads already completed synchronously in enqueue_read.
        return;
    }

    if (wait_nr == 0 || n_inflight == 0) {
        return;
    }

    wait_nr = std::min(wait_nr, n_inflight);

    const auto collect = [this](DWORD timeout) -> size_t {
        ULONG count = 0;
        if (!GetQueuedCompletionStatusEx(
                completion_port,
                completion_entries.data(),
                static_cast<ULONG>(completion_entries.size()),
                &count,
                timeout,
                FALSE)) {
            const DWORD error = GetLastError();
            if (error == WAIT_TIMEOUT) {
                return 0;
            }
            fprintf(stderr, "IOUring(win) completion-port error: %lu\n",
                    static_cast<unsigned long>(error));
            abort();
        }

        for (ULONG i = 0; i < count; ++i) {
            auto *request = reinterpret_cast<RequestData *>(completion_entries[i].lpOverlapped);
            POWERINFER_ASSERT(request != nullptr);
            POWERINFER_ASSERT(request->in_flight);

            DWORD got = 0;
            if (!GetOverlappedResult(handle, &request->overlapped, &got, FALSE) ||
                got != request->read_size) {
                fprintf(stderr, "IOUring(win) read error: expect %zu, got %lu (err %lu)\n",
                        request->read_size,
                        static_cast<unsigned long>(got),
                        static_cast<unsigned long>(GetLastError()));
                abort();
            }

            pending.push_back({request, got});
        }

        return static_cast<size_t>(count);
    };

    size_t completed = 0;
    while (completed < wait_nr) {
        completed += collect(INFINITE);
    }

    // Drain already-ready completions without blocking; this increases
    // overlap while retaining bounded request ownership.
    while (collect(0) > 0) {
    }
}

size_t IOUring::reap() {
    size_t count = pending.size();
    for (const auto &completion : pending) {
        RequestData *request = completion.request;
        POWERINFER_ASSERT(request != nullptr);
        POWERINFER_ASSERT(request->in_flight);
        POWERINFER_ASSERT(completion.bytes_transferred == request->read_size);
        POWERINFER_ASSERT(n_inflight > 0);

        CallbackFn *callback = request->callback;
        void *user_data = request->user_data;

        // Release the slot before the callback.  A request cannot be
        // completed twice because only this path clears in_flight.
        request->callback = nullptr;
        request->user_data = nullptr;
        request->in_flight = false;
        --n_inflight;

        if (callback) {
            callback(user_data);
        }
    }
    pending.clear();
    return count;
}

#else
// ---------------- Linux: io_uring (unchanged vendor implementation) ----------------

IOUring::IOUring(const std::string &path, size_t queue_depth) : queue_depth(queue_depth), req_data_buf(queue_depth) {
    fd = open(path.c_str(), O_RDONLY | O_DIRECT);
    POWERINFER_ASSERT(fd >= 0);

    io_uring_params params{};

    if (iou_enable_sq_poll) {
        params.flags |= IORING_SETUP_SQPOLL;

        if (iou_sq_poll_cpu_affinity != -1) {
            params.flags |= IORING_SETUP_SQ_AFF;
            params.sq_thread_idle = 1000;
            params.sq_thread_cpu = iou_sq_poll_cpu_affinity;
        }
    }

    int ret = io_uring_queue_init_params(queue_depth, &ring, &params);
    POWERINFER_ASSERT(ret >= 0);
}

IOUring::~IOUring() {
    if (fd != -1) {
        io_uring_queue_exit(&ring);
        close(fd);
        fd = -1;
    }
}

void IOUring::enqueue_read(void *buffer, size_t offset, size_t size, void *user_data, CallbackFn *callback) {
    POWERINFER_ASSERT(n_inflight < queue_depth);
    n_inflight++;

    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    POWERINFER_ASSERT(sqe != nullptr);

    auto &req_data = req_data_buf[(req_data_buf_pos++) % queue_depth];
    req_data.read_size = size;
    req_data.user_data = user_data;
    req_data.callback = callback;

    io_uring_prep_read(sqe, fd, buffer, size, offset);
    sqe->flags |= IOSQE_ASYNC;
    sqe->user_data = reinterpret_cast<uint64_t>(&req_data);
}

void IOUring::submit_and_wait(size_t wait_nr) {
    int ret = io_uring_submit_and_wait(&ring, wait_nr);
    POWERINFER_ASSERT(ret >= 0 || ret == -EINTR);
}

size_t IOUring::reap() {
    io_uring_cqe* cqe;
    unsigned head;
    unsigned count = 0;

    io_uring_for_each_cqe(&ring, head, cqe) {
        ++count;
        auto &req_data = *reinterpret_cast<RequestData *>(cqe->user_data);
        if (cqe->res != req_data.read_size) {
            fprintf(stderr, "io_uring read error: expect %zu, got %d\n", req_data.read_size, cqe->res);
            abort();
        }

        req_data.callback(req_data.user_data);
    }
    io_uring_cq_advance(&ring, count);
    n_inflight -= count;

    return count;
}

#endif  // __linux__

}  // namespace moe_sparse_pipeline
