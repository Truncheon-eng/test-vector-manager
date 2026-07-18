#ifndef SERVER_HPP
#define SERVER_HPP

#include <cstdint>
#include <ios>
#include <iostream>
#include <iomanip>
#include <limits>

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

#define FILENAME "a.c"
#define INFO "[i]:"
#define PAGE_SIZE 0x1000

void print_menu(void);
int shared_memory_open(void);
int shared_memory_truncate(int fd, size_t size);
int close_shared_memory_fd(int fd);
int write_data(int fd, uint32_t value);
int read_data(int fd, uint32_t * value);
int clear_ring_buffer(int fd);

using std::cin;
using std::cout;
using std::endl;
using RingBufferUint64 = RingBuffer<64, uint32_t>;

enum DpiResult {
    DPI_SUCCESS      = 0,
    DPI_ERROR        = 1,
    DPI_BUFFER_EMPTY = 2,
    DPI_BUFFER_FULL  = 3
};

#endif