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
    DPI_TIMEOUT      = 4,
    DPI_PEER_GONE    = 5,
    DPI_STREAM_END   = 6
  };

int shared_memory_open(void);

int shared_memory_unlink(void);

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
      // Wake a futex-waiting consumer (legacy single-packet path; the SPSC
      // contract still forbids mixing more than one writer/reader overall)
      ring_buffer->notify_data();
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
      // Wake a futex-waiting producer (legacy single-packet path)
      ring_buffer->notify_space();
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
  // Persistent-channel variants: lock-free SPSC, no flock, no per-call mmap
  // Contract: one producer process (write_burst) + one consumer process
  // (read_burst) sharing the channel's ring.
  // ============================================================  // Burst write over a persistent channel.
  // Returns the number of packets written (may be less than max_count if the
  // ring became full) or -1 on error.
  template <typename T>
  int write_burst(ShmChannel<SIZE, T>& channel, const T* values, int max_count) {
    if (!channel.valid() || values == nullptr || max_count <= 0)
      return -1;

    RingBuffer<SIZE, T>* ring_buffer = channel.ring();

    int written = 0;
    while (written < max_count &&
           ring_buffer->write_data(values[written]) == EXIT_SUCCESS)
      written++;

    if (written > 0)
      ring_buffer->notify_data();  // wake the consumer waiting for data

    return written;
  }

  // Blocking burst read over a persistent channel: waits event-driven (futex,
  // host-side — no simulation time involved) until at least one packet is
  // available or timeout_us expires, then drains up to max_count packets.
  // Returns DPI_SUCCESS (*out_count >= 1), DPI_STREAM_END (producer marked
  // EOF and the ring is drained), DPI_TIMEOUT, DPI_PEER_GONE (registered
  // producer process died) or DPI_ERROR.
  template <typename T>
  int read_burst(ShmChannel<SIZE, T>& channel, T* out, int max_count,
                 int* out_count, uint64_t timeout_us) {
    if (!channel.valid() || out == nullptr || out_count == nullptr || max_count <= 0)
      return DPI_ERROR;

    *out_count = 0;

    RingBuffer<SIZE, T>* ring_buffer = channel.ring();

    const uint64_t deadline = monotonic_us() + timeout_us;

    while (true) {
      int drained = 0;

      while (drained < max_count &&
             ring_buffer->read_data(out[drained]) == EXIT_SUCCESS)
        drained++;

      if (drained > 0) {
        ring_buffer->notify_space();  // wake the producer waiting for space
        *out_count = drained;
        return DPI_SUCCESS;
      }

      // Ring is empty and the producer marked EOF - clean end of stream
      if (ring_buffer->stream_done_flag())
        return DPI_STREAM_END;

      const uint64_t now = monotonic_us();
      if (now >= deadline)
        return DPI_TIMEOUT;

      // Producer died? Do not wait for data that will never arrive
      if (!process_alive(ring_buffer->producer()))
        return DPI_PEER_GONE;

      // Event-driven wait bounded by the remaining timeout, capped by a
      // liveness tick so a producer death is noticed even without wakeups
      uint64_t remain_us = deadline - now;
      constexpr uint64_t liveness_tick_us = 250000;
      if (remain_us > liveness_tick_us)
        remain_us = liveness_tick_us;

      struct timespec ts;
      ts.tv_sec  = static_cast<time_t>(remain_us / 1000000ULL);
      ts.tv_nsec = static_cast<long>((remain_us % 1000000ULL) * 1000ULL);

      ring_buffer->wait_for_data(&ts);
    }
  }


#endif
