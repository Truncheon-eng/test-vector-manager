#ifndef RING_BUFFER_HPP
#define RING_BUFFER_HPP

#include <atomic>
#include <cstdlib>
#include <stddef.h>
#include <type_traits>
#include <cstdint>

#include "Futex.hpp"

// Lock-free SPSC ring buffer shared between two processes via mmap.
// Safety contract: exactly ONE producer (write_data) and ONE consumer
// (read_data); each counter is written by its side only and grows
// monotonically. Slot payloads are published via release stores to the
// counters and observed via acquire loads on the peer side.
//
// Event-driven waits (step 4): each side has its own futex word; the peer
// wakes it only when the waiter flag is set, so the hot path stays
// syscall-free. Both wait/notify pairs use a seq_cst fence between the
// flag store and the predicate load (Dekker pattern) to rule out lost
// wakeups on store-buffer architectures.
template <int SIZE, typename DataType>
class RingBuffer {
private:
    DataType data[SIZE];

    // Producer cache line: published slot count + producer futex word
    alignas(64) std::atomic<size_t> write_count;
    std::atomic<uint32_t> space_wait;  // producer waits here when ring is full

    // Consumer cache line: consumed slot count + consumer futex word
    alignas(64) std::atomic<size_t> read_count;
    std::atomic<uint32_t> data_wait;   // consumer waits here when ring is empty

    // Control cache line: process registration + stream state
    alignas(64) std::atomic<int32_t> producer_pid;
    std::atomic<int32_t> consumer_pid;
    std::atomic<uint32_t> stream_done;  // producer EOF marker

    size_t mask = SIZE - 1;

  public:
    static_assert(SIZE > 0 && (SIZE & (SIZE - 1)) == 0, "SIZE has to be a power of 2");
    static_assert(std::is_trivially_copyable_v<DataType>);
    static_assert(std::is_standard_layout_v<DataType>);
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "size_t atomics must be lock-free to be shared across processes");
    static_assert(std::atomic<uint32_t>::is_always_lock_free,
                  "futex words must be lock-free to be shared across processes");

    // Bumped from 0xdeadbeef: layout v4 (futex words, pid control line,
    // stream_done EOF marker, cache-line alignment), stale segments from
    // older builds must fail the magic check
    static constexpr uint32_t magic_value = 0xdeadc0e0;
    uint32_t magic;

    inline RingBuffer():
        write_count{0},
        space_wait{0},
        read_count{0},
        data_wait{0},
        producer_pid{0},
        consumer_pid{0},
        stream_done{0},
        magic{magic_value} {}

    inline int is_empty() {
      return read_count.load(std::memory_order_acquire) ==
             write_count.load(std::memory_order_acquire);
    }

    inline int is_full() {
      return ((write_count.load(std::memory_order_acquire) -
               read_count.load(std::memory_order_acquire)) & (~mask)) != 0;
    }

    // Producer side only
    inline int write_data(const DataType &data) {
      const size_t w = write_count.load(std::memory_order_relaxed);  // we own it
      const size_t r = read_count.load(std::memory_order_acquire);   // consumer progress

      if (((w - r) & (~mask)) != 0)
        return EXIT_FAILURE;  // full

      this->data[w & mask] = data;
      write_count.store(w + 1, std::memory_order_release);  // publish the slot
      return EXIT_SUCCESS;
    }

    // Consumer side only
    inline int read_data(DataType& data) {
      const size_t r = read_count.load(std::memory_order_relaxed);   // we own it
      const size_t w = write_count.load(std::memory_order_acquire);  // producer progress

      if (r == w)
        return EXIT_FAILURE;  // empty

      data = this->data[r & mask];
      read_count.store(r + 1, std::memory_order_release);  // free the slot
      return EXIT_SUCCESS;
    }

    // --------------------------------------------------------
    // Event-driven wait/notify (futex words in this shared object)
    // --------------------------------------------------------

    // Producer: call after publishing slots (release stores to write_count)
    inline void notify_data() {
      std::atomic_thread_fence(std::memory_order_seq_cst);
      if (data_wait.load(std::memory_order_relaxed) != 0) {
        data_wait.store(0, std::memory_order_relaxed);
        futex_wake(&data_wait, 1);
      }
    }

    // Consumer: block while the ring is empty; relative_ts bounds the wait
    // (nullptr = forever). Spurious wakeups/timeouts are fine: the caller
    // re-checks the predicate in a loop.
    inline void wait_for_data(const struct timespec* relative_ts) {
      data_wait.store(1, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_seq_cst);
      if (is_empty())
        futex_wait(&data_wait, 1, relative_ts);
      data_wait.store(0, std::memory_order_relaxed);
    }

    // Consumer: call after freeing slots (release stores to read_count)
    inline void notify_space() {
      std::atomic_thread_fence(std::memory_order_seq_cst);
      if (space_wait.load(std::memory_order_relaxed) != 0) {
        space_wait.store(0, std::memory_order_relaxed);
        futex_wake(&space_wait, 1);
      }
    }

    // Producer: block while the ring is full, waking up every tick_us to
    // re-check the registered consumer's liveness.
    // Returns 0 when space may be available, -1 when the consumer process
    // is gone. A consumer that has not registered yet (pid 0) is assumed
    // alive, so a producer may safely wait for a late consumer.
    inline int wait_for_space(uint64_t tick_us = 250000) {
      while (true) {
        space_wait.store(1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);

        if (!is_full()) {
          space_wait.store(0, std::memory_order_relaxed);
          return 0;
        }

        if (!process_alive(consumer())) {
          space_wait.store(0, std::memory_order_relaxed);
          return -1;
        }

        struct timespec ts;
        ts.tv_sec  = static_cast<time_t>(tick_us / 1000000ULL);
        ts.tv_nsec = static_cast<long>((tick_us % 1000000ULL) * 1000ULL);
        futex_wait(&space_wait, 1, &ts);
        space_wait.store(0, std::memory_order_relaxed);
      }
    }

    // --------------------------------------------------------
    // Process registration (liveness detection)
    // --------------------------------------------------------
    inline void register_producer(int32_t pid) {
      producer_pid.store(pid, std::memory_order_release);
    }

    inline void register_consumer(int32_t pid) {
      consumer_pid.store(pid, std::memory_order_release);
    }

    inline int32_t producer() const {
      return producer_pid.load(std::memory_order_acquire);
    }

    inline int32_t consumer() const {
      return consumer_pid.load(std::memory_order_acquire);
    }

    // --------------------------------------------------------
    // Stream lifecycle (graceful EOF)
    // --------------------------------------------------------

    // Producer: mark end of stream - no more packets will be produced.
    // Wakes a consumer blocked in wait_for_data so it sees the EOF promptly.
    inline void mark_stream_done() {
      stream_done.store(1, std::memory_order_release);
      notify_data();
    }

    inline int stream_done_flag() const {
      return stream_done.load(std::memory_order_acquire);
    }
  };

#endif
