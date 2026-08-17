#include "../include/auxiliary.hpp"

int shared_memory_open(void) {
    int fd = shm_open(FILENAME, O_CREAT | O_RDWR, 0666);
    return fd;
}

int shared_memory_truncate(int fd, size_t size) {
    return ftruncate(fd, size);
}

int close_shared_memory_fd(int fd) {
    return close(fd);
}
