#ifndef SHM_CHANNEL_HPP
#define SHM_CHANNEL_HPP

#include <sys/mman.h>

#include "RingBuffer.hpp"
#include "SharedMemoryLock.hpp"

  // Persistent mapping of a ring buffer living in a POSIX shm segment.
  // Maps the segment once per connection instead of mmap/munmap per operation;
  // the fd is kept for flock-based mutual exclusion with the peer process.
  // Single-threaded use per channel.
  template <int SIZE, typename T>
  class ShmChannel {
    public:
      explicit ShmChannel(int fd) : fd_(fd) {
        constexpr size_t mapping_size = sizeof(RingBuffer<SIZE, T>);

        void* ptr = mmap(nullptr,
                          mapping_size,
                         PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd_,
                        0
        );

        if (ptr == MAP_FAILED) {
          perror("mmap");
          return;
        }

        ptr_ = ptr;

        // Initialize the ring under the lock if the peer has not done it yet
        SharedMemoryLock lock(fd_);
        if (!lock.owns_lock()) {
          perror("flock");
          munmap(ptr_, mapping_size);
          ptr_ = nullptr;
          return;
        }

        RingBuffer<SIZE, T>* ring = static_cast<RingBuffer<SIZE, T>*>(ptr_);
        if (ring->magic != RingBuffer<SIZE, T>::magic_value)
          ring = new (ptr_) RingBuffer<SIZE, T>();

        ring_ = ring;
      }

      ~ShmChannel() {
        if (ptr_ != nullptr)
          munmap(ptr_, sizeof(RingBuffer<SIZE, T>));
      }

      ShmChannel(const ShmChannel&) = delete;
      ShmChannel& operator=(const ShmChannel&) = delete;

      bool valid() const { return ring_ != nullptr; }
      int fd() const { return fd_; }
      RingBuffer<SIZE, T>* ring() { return ring_; }

    private:
      int fd_;
      void* ptr_ = nullptr;
      RingBuffer<SIZE, T>* ring_ = nullptr;
  };

#endif
