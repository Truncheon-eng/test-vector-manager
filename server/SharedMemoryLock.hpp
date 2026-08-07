#ifndef SHARED_MEMORY_LOCK_HPP
#define SHARED_MEMORY_LOCK_HPP

#include <cerrno>
#include <sys/file.h>
/*
 * Аналог lock_guard-а для разделямой памяти
 * Конструктор принимает в качетсве параметра файловый дескриптор, ассоциированный с файлом.
 * 
 * В рамках конструктора используется цикл с постусловием, чтобы получить "уникальную" блокировку (макрос LOCK_EX), 
 * то есть блокировку, которая будет доступна только одному процессу.
 *
 * Чтобы соотв. парадигме RAII был создан деструктор ~SharedMemoryLock, в рамках которого в цикле происходит снятие блокировки
 * посредством передачи макроса LOCK_UN в ту же функцию flock.
 *
 * Используется данный объект в таких функциях, как:
 * 	- write_data
 * 	- read_data
 * 	- clear_ring_buffer
 * что в файле "dpi_funcs.cpp", что в файле "auxiliary.cpp".
 */
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
