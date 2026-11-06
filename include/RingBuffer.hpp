#ifndef RING_BUFFER_HPP
#define RING_BUFFER_HPP

#include <atomic>
#include <cstdlib>
#include <stddef.h>
#include <type_traits>
#include <cstdint>

// Lock-free SPSC ring buffer shared between two processes via mmap.
template <int SIZE, typename DataType>
class RingBuffer {
private:
    DataType data[SIZE];
    std::atomic<size_t> write_count;  // producer-owned
    std::atomic<size_t> read_count;   // consumer-owned
    size_t mask = SIZE - 1;

  public:
    static_assert(SIZE > 0 && (SIZE & (SIZE - 1)) == 0, "SIZE has to be a power of 2");
    static_assert(std::is_trivially_copyable_v<DataType>);
    static_assert(std::is_standard_layout_v<DataType>);
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "size_t atomics must be lock-free to be shared across processes");

    static constexpr uint32_t magic_value = 0xdeadbeef;
    uint32_t magic;

    inline RingBuffer(): write_count{0}, read_count{0}, magic{magic_value} {}

    inline int is_empty() {
      return  read_count.load(std::memory_order_acquire) ==
              write_count.load(std::memory_order_acquire);
    }

    inline int is_full() {
      return  ((write_count.load(std::memory_order_acquire) -
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
  };

#endif
