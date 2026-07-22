#pragma once

#include <string>
#include <vector>

// lane-110 M2 Windows port: io_uring is a Linux kernel interface. On Windows we provide an
// API-identical SYNCHRONOUS implementation over Win32 positioned reads (ReadFile+OVERLAPPED offset),
// so the disk-streamed expert cache (EXPERT_BUNDLE_PATH, the run-bigger-than-RAM capability) works.
// Semantics preserved: enqueue_read does a blocking positioned read immediately and queues its
// completion; submit_and_wait is a no-op (reads already done); reap fires the queued callbacks.
// This is correct but not overlapped — the IOCP async version is a later optimization (M2b).
#if !defined(__linux__)
namespace moe_sparse_pipeline {

struct IOUring {
    using CallbackFn = void(void *user_data);

    int  fd = -1;            // unused on Windows (kept for ABI parity with the Linux struct)
    void *handle = nullptr;  // Win32 HANDLE (opaque here to keep windows.h out of the header)
    size_t n_inflight = 0;

    explicit IOUring(const std::string &path, size_t queue_depth = 64);
    ~IOUring();

    void enqueue_read(void *buffer, size_t offset, size_t size, void *user_data, CallbackFn *callback);
    void submit_and_wait(size_t wait_nr);
    size_t reap();

private:
    struct Completion {
        void *user_data = nullptr;
        CallbackFn *callback = nullptr;
    };
    std::vector<Completion> pending;   // reads done, callbacks not yet fired
    size_t queue_depth = 0;
};

}
#else
#include <liburing.h>

namespace moe_sparse_pipeline {

struct IOUring {
    using CallbackFn = void(void *user_data);

    int fd = -1;
    size_t n_inflight = 0;

    explicit IOUring(const std::string &path, size_t queue_depth = 64);
    ~IOUring();

    // Enqueue one read request into io_uring
    void enqueue_read(void *buffer, size_t offset, size_t size, void *user_data, CallbackFn *callback);

    // Submit all IO requests with io_uring_submit, and wait for at least `wait_nr` IO requests to complete
    void submit_and_wait(size_t wait_nr);

    // Examine all completed IO requests and invoke corresponding callbacks
    size_t reap();

private:
    struct RequestData {
        size_t read_size = 0;
        void *user_data = nullptr;
        CallbackFn *callback = nullptr;
    };

    struct io_uring ring{};
    size_t queue_depth = 0;
    std::vector<RequestData> req_data_buf;
    size_t req_data_buf_pos = 0;
};

}
#endif  // __linux__ (lane-110 Windows port)
