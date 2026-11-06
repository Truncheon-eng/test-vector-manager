#ifndef FUTEX_HPP
#define FUTEX_HPP

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <linux/futex.h>
#include <signal.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

// Thin wrappers over futex(2) for words living in shared memory.
// NOTE: no FUTEX_PRIVATE_FLAG — the words are shared between processes.
// Linux-only by design (the flock-based legacy path stays portable).

inline int futex_wait(std::atomic<uint32_t>* word, uint32_t expected,
                      const struct timespec* relative_ts = nullptr) {
    return static_cast<int>(syscall(
        SYS_futex,
        reinterpret_cast<uint32_t*>(word),
        FUTEX_WAIT,
        expected,
        relative_ts,
        nullptr,
        0
    ));
}

inline int futex_wake(std::atomic<uint32_t>* word, int max_wake = 1) {
    return static_cast<int>(syscall(
        SYS_futex,
        reinterpret_cast<uint32_t*>(word),
        FUTEX_WAKE,
        max_wake,
        nullptr,
        nullptr,
        0
    ));
}

// Liveness probe for pids registered in shared memory.
// pid <= 0 means "not registered" and is treated as alive (nothing to check).
// Note: a recycled pid would pass the probe - acceptable for this tool.
inline bool process_alive(int32_t pid) {
    if (pid <= 0)
        return true;

    if (kill(pid, 0) == 0)
        return true;

    return errno != ESRCH;  // EPERM: exists, owned by another user
}

#endif
