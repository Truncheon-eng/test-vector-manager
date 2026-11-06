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
    uint64_t pps   = 0;  // packets per second; 0 = max rate

    if (argc > 1) {
      char* end = nullptr;
      unsigned long long value = strtoull(argv[1], &end, 10);

      if (end == argv[1] || *end != '\0' || value == 0) {
        std::cerr << "usage: " << argv[0]
                  << " [num_transactions] [packets_per_second] [consumer_grace_s]"
                  << std::endl;
        return EXIT_FAILURE;
      }

        count = value;
    }

    if (argc > 2) {
      char* end = nullptr;
      unsigned long long value = strtoull(argv[2], &end, 10);

      if (end == argv[2] || *end != '\0') {
        std::cerr << "usage: " << argv[0]
                  << " [num_transactions] [packets_per_second] [consumer_grace_s]"
                  << std::endl;
        return EXIT_FAILURE;
      }

      pps = value;  // 0 = max rate
    }

    // How long to wait for the FIRST consumer to attach when the ring is
    // full; 0 = wait forever. A registered consumer is never timed out -
    // only its death (liveness check) aborts the producer.
    uint64_t grace_us = 30ULL * 1000000ULL;

    if (argc > 3) {
      char* end = nullptr;
      unsigned long long value = strtoull(argv[3], &end, 10);

      if (end == argv[3] || *end != '\0') {
        std::cerr << "usage: " << argv[0]
                  << " [num_transactions] [packets_per_second] [consumer_grace_s]"
                  << std::endl;
        return EXIT_FAILURE;
      }

      grace_us = value * 1000000ULL;
    }

    uint16_t lfsr_state = LFSR_SEED;

    uint64_t generated {0};
    uint64_t written {0};
    uint64_t full_count {0};
    uint64_t burst_count {0};

    // Start from a fresh segment: drop stale data from previous runs so a
    // consumer can never mistake leftovers for a live stream.
    // Contract: the server is started BEFORE the consumer (an early consumer
    // would be stranded on the orphaned inode and fail loudly with a timeout).
    if (shared_memory_unlink() != 0)
      return EXIT_FAILURE;

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

    const uint64_t start_us = monotonic_us();

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
          const int wait_rc = channel.ring()->wait_for_space(250000, grace_us);

          if (wait_rc != 0) {
            std::cerr << "[x]: "
                      << (wait_rc == -2 ? "no consumer attached within grace period"
                                        : "consumer process is gone")
                      << " - aborting at packet " << (i + sent) << std::endl;
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

      // Open-loop pacing against a deadline schedule (no cumulative drift)
      if (pps > 0) {
        const uint64_t target_us = start_us + (i * 1000000ULL) / pps;
        const uint64_t now_us    = monotonic_us();

        if (target_us > now_us)
          sleep_us(target_us - now_us);
      }
    }

    // Signal EOF to the consumer: no more packets will be produced
    channel.ring()->mark_stream_done();

    const double seconds = static_cast<double>(monotonic_us() - start_us) / 1e6;

    cout << "Generated:   " << generated << endl
        << "Written:     " << written << endl
        << "Bursts:      " << burst_count << endl
        << "Buffer full: " << full_count << endl
        << "Elapsed:     " << seconds << " s" << endl
        << "Throughput:  " << (seconds > 0.0 ? static_cast<double>(written) / seconds : 0.0)
        << " packets/s" << endl;

    close_shared_memory_fd(fd);

    return EXIT_SUCCESS;
  }
