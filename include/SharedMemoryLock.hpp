#ifndef SHARED_MEMORY_LOCK_HPP
#define SHARED_MEMORY_LOCK_HPP

#include <cerrno>
#include <sys/file.h>


class SharedMemoryLock {
public:
    explicit SharedMemoryLock(int fd) : fd_(fd), locked_(false) {
        int status;
        do {
            status = flock(fd_, LOCK_EX);
        } while (status == -1 && errno == EINTR);

        locked_ = (status == 0);
    }

    ~SharedMemoryLock() {
        if (!locked_)
            return;

        while (flock(fd_, LOCK_UN) == -1 && errno == EINTR) {
        }
    }

    SharedMemoryLock(const SharedMemoryLock&) = delete;
    SharedMemoryLock& operator=(const SharedMemoryLock&) = delete;

    bool owns_lock() const {
        return locked_;
    }

private:
    int fd_;
    bool locked_;
};

#endif
