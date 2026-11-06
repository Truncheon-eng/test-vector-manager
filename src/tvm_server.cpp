#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "auxiliary.hpp"
#include "lfsr.hpp"
#include "packet.hpp"

  std::ostream& operator<<(std::ostream& out, const test_packet_t& packet) {
    out << packet.data;
    return out;
  }

  int main(int argc, char* argv[]) {
    constexpr int BURST_MAX = 256;  // packets per write_burst call (ring holds SIZE=1024)

    uint64_t count = NUM_TRANSACTIONS;

    if (argc > 1) {
      char* end = nullptr;
      unsigned long long value = strtoull(argv[1], &end, 10);

      if (end == argv[1] || *end != '\0' || value == 0) {
        std::cerr << "usage: " << argv[0] << " [num_transactions]" << std::endl;
        return EXIT_FAILURE;
      }

      count = value;
    }

    uint16_t lfsr_state = LFSR_SEED;

    uint64_t generated {0};
    uint64_t written {0};
    uint64_t full_count {0};
    uint64_t burst_count {0};

    int fd = shared_memory_open();
    if (fd < 0) {
      perror("shm_open");
      return EXIT_FAILURE;
    }

    if (shared_memory_truncate(fd, PAGE_SIZE) != 0) {
      perror("ftruncate");
      close_shared_memory_fd(fd);
      return EXIT_FAILURE;
    }

    clear_ring_buffer<test_packet_t>(fd);

    // Persistent mapping: one mmap for the whole connection
    ShmChannel<SIZE, test_packet_t> channel(fd);
    if (!channel.valid()) {
      std::cerr << "[x]: ShmChannel mapping failed" << std::endl;
      close_shared_memory_fd(fd);
      return EXIT_FAILURE;
    }

    // Register for liveness detection: a blocked producer notices a dead
    // consumer instead of waiting forever
    channel.ring()->register_producer(getpid());

    std::vector<test_packet_t> stage(BURST_MAX);

    uint64_t i = 0;
    while (i < count) {
      // Generate a batch into the staging buffer
      const int n = static_cast<int>(std::min<uint64_t>(BURST_MAX, count - i));
      for (int j = 0; j < n; j++) {
        stage[j].seq  = i + static_cast<uint64_t>(j);
        stage[j].data = lfsr_next(lfsr_state);
      }
      generated += n;

      // Flush the batch under a single flock/mmap cycle
      int sent = 0;
      while (sent < n) {
        int w = write_burst(channel, stage.data() + sent, n - sent);

        if (w < 0) {
          std::cerr << "[x]: write_burst failed at packet " << (i + sent) << std::endl;
          close_shared_memory_fd(fd);
          return EXIT_FAILURE;
        }

        if (w == 0) {
          // Ring is full: block on the futex until the consumer frees slots
          // (event-driven; wakes periodically to check consumer liveness)
          full_count++;
          if (channel.ring()->wait_for_space() != 0) {
            std::cerr << "[x]: consumer process is gone - aborting at packet "
                      << (i + sent) << std::endl;
            close_shared_memory_fd(fd);
            return EXIT_FAILURE;
          }
          continue;
        }

        sent += w;
        burst_count++;
      }

      written += sent;
      i += n;
    }

    cout << "Generated:   " << generated << endl
        << "Written:     " << written << endl
        << "Bursts:      " << burst_count << endl
        << "Buffer full: " << full_count << endl;

    close_shared_memory_fd(fd);

    return EXIT_SUCCESS;
  }
