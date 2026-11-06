#ifndef MONOTONIC_CLOCK_HPP
#define MONOTONIC_CLOCK_HPP

#include <cerrno>
#include <cstdint>
#include <time.h>

// Monotonic clock in microseconds for host-side waits and deadlines
inline uint64_t monotonic_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL +
           static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

inline void sleep_us(uint64_t us) {
    struct timespec ts;
    ts.tv_sec  = static_cast<time_t>(us / 1000000ULL);
    ts.tv_nsec = static_cast<long>((us % 1000000ULL) * 1000ULL);
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

#endif
