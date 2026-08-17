#ifndef AUXILIARY_HPP
#define AUXILIARY_HPP

#include <iostream>
#include <assert.h>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include <time.h>

#include "RingBuffer.hpp"
#include "SharedMemoryLock.hpp"

#define FILENAME "a.c"
#define INFO "[i]:"
#define PAGE_SIZE (8 * 0x1000)

#define SIZE 1024


using std::cin;
using std::cout;
using std::endl;

enum DpiResult {
    DPI_SUCCESS      = 0,
    DPI_ERROR        = 1,
    DPI_BUFFER_EMPTY = 2,
    DPI_BUFFER_FULL  = 3
};

int shared_memory_open(void);

int shared_memory_truncate(int fd, size_t size);

int close_shared_memory_fd(int fd);


template <typename T>
RingBuffer<SIZE, T>* get_ring_buffer(void* data_ptr) {
    RingBuffer<SIZE, T>* result = static_cast<RingBuffer<SIZE, T>*>(data_ptr);

    if (result->magic != RingBuffer<SIZE, T>::magic_value)
        result = new (data_ptr) RingBuffer<SIZE, T>();

    return result;
}

template <typename T>
int write_data(int fd, T value) {
    if (fd < 0)
        return DPI_ERROR;

    SharedMemoryLock lock(fd);
    if (!lock.owns_lock()) {
        perror("flock");
        return DPI_ERROR;
    }

    constexpr size_t mapping_size = sizeof(RingBuffer<SIZE, T>);
    void* data_ptr = mmap(
        nullptr,
        mapping_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data_ptr == MAP_FAILED) {
        perror("mmap");
        return DPI_ERROR;
    }

    RingBuffer<SIZE, T>* ring_buffer = get_ring_buffer<T>(data_ptr);
    bool buffer_full = ring_buffer->write_data(value);
    int result;

    if (buffer_full) {
        // cout << INFO << " buffer is full" << endl;
        result = DPI_BUFFER_FULL;
    } else {
        // cout << INFO << " successfully wrote \"" << value << "\"" << endl; 
        result = DPI_SUCCESS;
    }

    if (msync(data_ptr, mapping_size, MS_SYNC) != 0) {
        perror("msync");
        result = DPI_ERROR;
    }

    if (munmap(data_ptr, mapping_size) != 0) {
        perror("munmap");
        result = DPI_ERROR;
    }

    return result;
}


template <typename T>
int read_data(int fd, T* value) {
    if (fd < 0 || value == nullptr)
        return DPI_ERROR;

    SharedMemoryLock lock(fd);
    if (!lock.owns_lock()) {
        perror("flock");
        return DPI_ERROR;
    }

    constexpr size_t mapping_size = sizeof(RingBuffer<SIZE, T>);

    void* data_ptr = mmap(
        nullptr,
        mapping_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data_ptr == MAP_FAILED) {
        perror("mmap");
        return DPI_ERROR;
    }

    RingBuffer<SIZE, T>* ring_buffer = get_ring_buffer<T>(data_ptr);

    bool buffer_empty = ring_buffer->read_data(*value);
    int result;

    if (buffer_empty) {
        // cout << INFO << " buffer is empty" << endl;
        result = DPI_BUFFER_EMPTY;
    } else {
        // cout << INFO << " successfully read \"" << *value << "\"" << endl;
        result = DPI_SUCCESS;
    }

    if (munmap(data_ptr, mapping_size) != 0) {
        perror("munmap");
        result = DPI_ERROR;
    }

    return result;
}


template <typename T>
int clear_ring_buffer(int fd) {
    if (fd < 0)
        return DPI_ERROR;

    // исползование блокировки
    SharedMemoryLock lock(fd);
    if (!lock.owns_lock()) {
        perror("flock");
        return DPI_ERROR;
    }

    constexpr size_t mapping_size = sizeof(RingBuffer<SIZE, T>);

    void* data_ptr = mmap(
        nullptr,
        mapping_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data_ptr == MAP_FAILED) {
        perror("mmap");
        return DPI_ERROR;
    }

    new (data_ptr) RingBuffer<SIZE, T>();
    int result = DPI_SUCCESS;

    if (msync(data_ptr, mapping_size, MS_SYNC) != 0) {
        perror("msync");
        result = DPI_ERROR;
    }

    if (munmap(data_ptr, mapping_size) != 0) {
        perror("munmap");
        result = DPI_ERROR;
    }

    if (result == DPI_SUCCESS)
        cout << INFO << " ring buffer cleared" << endl;
    return result;
}


#endif