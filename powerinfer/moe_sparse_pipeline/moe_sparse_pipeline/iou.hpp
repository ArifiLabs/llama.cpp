#pragma once

#include <string>
#include <vector>

#if !defined(__linux__)
#include <windows.h>

namespace moe_sparse_pipeline {

// lane-110 M2b runtime toggle. Default ON (IOCP async reads); POWERINFER_IOCP=0
// selects the pre-M2b synchronous ReadFile+OVERLAPPED path, so a field deadlock
// is recoverable without a recompile. Read once at first use.
bool iocp_enabled();

struct IOUring {
    using CallbackFn = void(void *user_data);

    int fd = -1; // unused on Windows; retained for ABI parity with Linux
    HANDLE handle = INVALID_HANDLE_VALUE;
    HANDLE completion_port = nullptr;
    size_t n_inflight = 0;

    explicit IOUring(const std::string &path, size_t queue_depth = 64);
    ~IOUring();

    void enqueue_read(void *buffer, size_t offset, size_t size, void *user_data, CallbackFn *callback);
    void submit_and_wait(size_t wait_nr);
    size_t reap();

private:
    struct RequestData {
        OVERLAPPED overlapped{};
        size_t read_size = 0;
        void *user_data = nullptr;
        CallbackFn *callback = nullptr;
        bool in_flight = false;
    };

    struct Completion {
        RequestData *request = nullptr;
        DWORD bytes_transferred = 0;
    };

    std::vector<RequestData> req_data_buf;
    std::vector<OVERLAPPED_ENTRY> completion_entries;
    std::vector<Completion> pending;
    size_t queue_depth = 0;
    size_t req_data_buf_pos = 0;
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
