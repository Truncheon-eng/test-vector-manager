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
#include "ShmChannel.hpp"

#define FILENAME "/tvm_shm"
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
    DPI_BUFFER_FULL  = 3,
    DPI_TIMEOUT      = 4
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
      result = DPI_BUFFER_FULL;
    } else {
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


  // Burst write: writes up to max_count packets under a single flock/mmap
  // cycle. Returns the number of packets written (may be less than max_count
  // if the ring became full) or -1 on error.
  // No msync here: updates to a MAP_SHARED mapping of a shm object are
  // immediately visible to other processes mapping it (same as the read path).
  template <typename T>
  int write_burst(int fd, const T* values, int max_count) {
    if (fd < 0 || values == nullptr || max_count <= 0)
      return -1;

    SharedMemoryLock lock(fd);
    if (!lock.owns_lock()) {
      perror("flock");
      return -1;
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
      return -1;
    }

    RingBuffer<SIZE, T>* ring_buffer = get_ring_buffer<T>(data_ptr);

    int written = 0;
    while(written < max_count &&
          ring_buffer->write_data(values[written]) == EXIT_SUCCESS)
      written++;

    if (munmap(data_ptr, mapping_size) != 0) {
      perror("munmap");
      return -1;
    }

    return written;
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
      result = DPI_BUFFER_EMPTY;
    } else {
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


  // Monotonic clock in microseconds for host-side waits
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

  // Blocking burst read: waits on the host side (no simulation time involved)
  // until at least one packet is available or timeout_us expires, then drains
  // up to max_count packets with a single flock/mmap cycle.
  // Returns DPI_SUCCESS (*out_count >= 1), DPI_TIMEOUT or DPI_ERROR.
  template <typename T>
  int read_burst (int fd, T* out, int max_count, int* out_count,
                  uint64_t timeout_us, uint64_t poll_us = 50) {
    if (fd < 0 || out == nullptr || out_count == nullptr || max_count <= 0)
      return DPI_ERROR;

    *out_count = 0;

    const uint64_t deadline = monotonic_us() + timeout_us;

    while (true) {
      int drained = 0;

      {
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

        while (drained < max_count &&
               ring_buffer->read_data(out[drained]) == EXIT_SUCCESS)
          drained++;

        if (munmap(data_ptr, mapping_size) != 0) {
          perror("munmap");
          return DPI_ERROR;
        }
      }

      if (drained > 0) {
        *out_count = drained;
        return DPI_SUCCESS;
      }

      if (monotonic_us() >= deadline)
        return DPI_TIMEOUT;

      sleep_us(poll_us);
    }
  }


  // ============================================================
  // Persistent-channel variants: flock only, no per-call mmap/munmap
  // ============================================================

  // Burst write over a persistent channel.
  // Returns the number of packets written (may be less than max_count if the
  // ring became full) or -1 on error.
  template <typename T>
  int write_burst(ShmChannel<SIZE, T>& channel, const T* values, int max_count) {
    if (!channel.valid() || values == nullptr || max_count <= 0)
      return -1;

    SharedMemoryLock lock(channel.fd());
    if (!lock.owns_lock()) {
      perror("flock");
      return -1;
    }

    RingBuffer<SIZE, T>* ring_buffer = channel.ring();

    int written = 0;
    while (written < max_count &&
           ring_buffer->write_data(values[written]) == EXIT_SUCCESS)
      written++;

    return written;
  }

  // Blocking burst read over a persistent channel: waits host-side (no
  // simulation time involved) until at least one packet is available or
  // timeout_us expires, then drains up to max_count packets.
  // Returns DPI_SUCCESS (*out_count >= 1), DPI_TIMEOUT or DPI_ERROR.
  template <typename T>
  int read_burst(ShmChannel<SIZE, T>& channel, T* out, int max_count,
                 int* out_count, uint64_t timeout_us, uint64_t poll_us = 50) {
    if (!channel.valid() || out == nullptr || out_count == nullptr || max_count <= 0)
      return DPI_ERROR;

    *out_count = 0;

    const uint64_t deadline = monotonic_us() + timeout_us;

    while (true) {
      int drained = 0;

      {
        SharedMemoryLock lock(channel.fd());
        if (!lock.owns_lock()) {
          perror("flock");
          return DPI_ERROR;
        }

        RingBuffer<SIZE, T>* ring_buffer = channel.ring();

        while (drained < max_count &&
               ring_buffer->read_data(out[drained]) == EXIT_SUCCESS)
          drained++;
      }

      if (drained > 0) {
        *out_count = drained;
        return DPI_SUCCESS;
      }

      if (monotonic_us() >= deadline)
        return DPI_TIMEOUT;

      sleep_us(poll_us);
    }
  }


#endif
