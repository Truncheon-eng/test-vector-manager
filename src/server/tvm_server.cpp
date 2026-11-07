#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "auxiliary.hpp"
#include "ctrl_packet.hpp"
#include "lfsr.hpp"
#include "packet.hpp"

  std::ostream& operator<<(std::ostream& out, const test_packet_t& packet) {
    out << packet.data;
    return out;
  }

  int main(int argc, char* argv[]) {
    constexpr int BURST_MAX = 256;  // packets per write_burst call (ring holds SIZE=1024)

    const auto usage = [argv]() {
      std::cerr << "usage: " << argv[0]
                << " [num_transactions] [packets_per_second] [consumer_grace_s] [ctrl_wait_s]"
                << std::endl;
    };

    uint64_t count = NUM_TRANSACTIONS;
    uint64_t pps   = 0;  // packets per second; 0 = max rate

    if (argc > 1) {
      char* end = nullptr;
      unsigned long long value = strtoull(argv[1], &end, 10);

      if (end == argv[1] || *end != '\0' || value == 0) {
        usage();
        return EXIT_FAILURE;
      }

      count = value;
    }

    if (argc > 2) {
      char* end = nullptr;
      unsigned long long value = strtoull(argv[2], &end, 10);

      if (end == argv[2] || *end != '\0') {
        usage();
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
        usage();
        return EXIT_FAILURE;
      }

      grace_us = value * 1000000ULL;
    }

    // How long to wait for control-channel commands (START_STREAM etc.)
    // before falling back to argv defaults; 0 = start immediately (legacy)
    uint64_t ctrl_wait_us = 5ULL * 1000000ULL;

    if (argc > 4) {
      char* end = nullptr;
      unsigned long long value = strtoull(argv[4], &end, 10);

      if (end == argv[4] || *end != '\0') {
        usage();
        return EXIT_FAILURE;
      }

      ctrl_wait_us = value * 1000000ULL;
    }

    uint16_t lfsr_state = LFSR_SEED;

    uint64_t generated {0};
    uint64_t written {0};
    uint64_t full_count {0};
    uint64_t burst_count {0};
    uint64_t ctrl_commands {0};

    // --------------------------------------------------------
    // Data channel (producer role)
    // --------------------------------------------------------
    // Start from a fresh segment: drop stale data from previous runs so a
    // consumer can never mistake leftovers for a live stream.
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

    ShmChannel<SIZE, test_packet_t> channel(fd);
    if (!channel.valid()) {
      std::cerr << "[x]: ShmChannel mapping failed" << std::endl;
      close_shared_memory_fd(fd);
      return EXIT_FAILURE;
    }

    channel.ring()->register_producer(getpid());

    // --------------------------------------------------------
    // Control channel (consumer role): commands from the testbench
    // --------------------------------------------------------
    if (shared_memory_unlink_name(CTRL_FILENAME) != 0) {
      close_shared_memory_fd(fd);
      return EXIT_FAILURE;
    }

    int ctrl_fd = shared_memory_open_name(CTRL_FILENAME);
    if (ctrl_fd < 0) {
      perror("ctrl shm_open");
      close_shared_memory_fd(fd);
      return EXIT_FAILURE;
    }

    if (shared_memory_truncate(ctrl_fd, PAGE_SIZE) != 0) {
      perror("ctrl ftruncate");
      close_shared_memory_fd(ctrl_fd);
      close_shared_memory_fd(fd);
      return EXIT_FAILURE;
    }

    ShmChannel<SIZE, ctrl_packet_t> ctrl_channel(ctrl_fd);
    if (!ctrl_channel.valid()) {
      std::cerr << "[x]: ctrl ShmChannel mapping failed" << std::endl;
      close_shared_memory_fd(ctrl_fd);
      close_shared_memory_fd(fd);
      return EXIT_FAILURE;
    }

    ctrl_channel.ring()->register_consumer(getpid());

    // --------------------------------------------------------
    // Pre-stream command phase: wait up to ctrl_wait_us for START_STREAM.
    // LOAD_VECTORS / SET_RATE received before it configure the stream.
    // On timeout or an absent controller fall back to argv defaults.
    // --------------------------------------------------------
    uint64_t stream_count = count;
    bool stopped = false;

    if (ctrl_wait_us > 0) {
      const uint64_t ctrl_deadline = monotonic_us() + ctrl_wait_us;

      cout << INFO << " waiting up to " << (ctrl_wait_us / 1000000ULL)
           << "s for control commands on " << CTRL_FILENAME << endl;

      while (true) {
        const uint64_t now_us = monotonic_us();
        if (now_us >= ctrl_deadline) {
          cout << INFO << " no START_STREAM received - using argv defaults" << endl;
          break;
        }

        ctrl_packet_t cmd {};
        int cnt = 0;
        const int rc = read_burst(ctrl_channel, &cmd, 1, &cnt,
                                  ctrl_deadline - now_us);

        if (rc != DPI_SUCCESS) {
          // TIMEOUT or PEER_GONE: no controller is coming - legacy start
          cout << INFO << " control wait ended (rc=" << rc
               << ") - using argv defaults" << endl;
          break;
        }

        ctrl_commands++;

        switch (cmd.cmd) {
          case CTRL_START_STREAM:
            if (cmd.arg > 0)
              stream_count = cmd.arg;
            cout << INFO << " START_STREAM: " << stream_count << " packets" << endl;
            break;

          case CTRL_STOP_STREAM:
            cout << INFO << " STOP_STREAM before start - nothing to stream" << endl;
            stopped = true;
            break;

          case CTRL_LOAD_VECTORS:
            lfsr_state = vector_set_seed(static_cast<int>(cmd.arg));
            cout << INFO << " LOAD_VECTORS: set " << cmd.arg << " (seed 0x"
                 << std::hex << lfsr_state << std::dec << ")" << endl;
            continue;

          case CTRL_SET_RATE:
            pps = cmd.arg;
            cout << INFO << " SET_RATE: " << pps << " pps" << endl;
            continue;

          default:
            cout << INFO << " unknown command " << cmd.cmd << " ignored" << endl;
            continue;
        }

        break;  // START_STREAM or STOP_STREAM ends the wait phase
      }
    }

    clear_ring_buffer<test_packet_t>(fd);

    std::vector<test_packet_t> stage(BURST_MAX);

    const uint64_t start_us = monotonic_us();

    uint64_t i = 0;

    // Non-blocking drain of service commands; returns true on STOP_STREAM
    auto handle_ctrl = [&]() -> bool {
      while (true) {
        ctrl_packet_t cmd {};
        int cnt = 0;

        if (read_burst(ctrl_channel, &cmd, 1, &cnt, 0) != DPI_SUCCESS)
          break;

        ctrl_commands++;

        if (cmd.cmd == CTRL_STOP_STREAM) {
          cout << INFO << " STOP_STREAM at packet " << i << endl;
          return true;
        }

        if (cmd.cmd == CTRL_SET_RATE) {
          pps = cmd.arg;
          cout << INFO << " SET_RATE: " << pps << " pps" << endl;
        }

        if (cmd.cmd == CTRL_LOAD_VECTORS) {
          // Rejected: switching the seed mid-stream desynchronizes the
          // consumer's golden model (see README, "Канал управления").
          // LOAD_VECTORS is accepted only in the pre-stream command phase.
          cout << INFO << " LOAD_VECTORS rejected mid-stream (set " << cmd.arg
               << ") - restart the stream to change vector sets" << endl;
        }
      }
      return false;
    };

    while (!stopped && i < stream_count) {
      // Generate a batch into the staging buffer
      const int n = static_cast<int>(std::min<uint64_t>(BURST_MAX, stream_count - i));
      for (int j = 0; j < n; j++) {
        stage[j].seq  = i + static_cast<uint64_t>(j);
        stage[j].data = lfsr_next(lfsr_state);
      }
      generated += n;

      // Flush the batch
      int sent = 0;
      while (sent < n) {
        int w = write_burst(channel, stage.data() + sent, n - sent);

        if (w < 0) {
          std::cerr << "[x]: write_burst failed at packet " << (i + sent) << std::endl;
          close_shared_memory_fd(ctrl_fd);
          close_shared_memory_fd(fd);
          return EXIT_FAILURE;
        }

        if (w == 0) {
          // Ring is full: a STOP may be pending - check before blocking
          if (handle_ctrl()) {
            stopped = true;
            break;
          }

          full_count++;
          const int wait_rc = channel.ring()->wait_for_space(250000, grace_us);

          if (wait_rc != 0) {
            // The consumer may have exited cleanly right after sending
            // STOP - drain the control channel before declaring failure
            if (handle_ctrl()) {
              stopped = true;
              break;
            }

            std::cerr << "[x]: "
                      << (wait_rc == -2 ? "no consumer attached within grace period"
                                        : "consumer process is gone")
                      << " - aborting at packet " << (i + sent) << std::endl;
            close_shared_memory_fd(ctrl_fd);
            close_shared_memory_fd(fd);
            return EXIT_FAILURE;
          }
          continue;
        }

        sent += w;
        burst_count++;
      }

      if (stopped)
        break;

      written += sent;
      i += n;

      // Service commands between batches (non-blocking drain)
      if (handle_ctrl())
        stopped = true;

      if (stopped)
        break;

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
         << "Ctrl cmds:   " << ctrl_commands << endl
         << "Elapsed:     " << seconds << " s" << endl
         << "Throughput:  " << (seconds > 0.0 ? static_cast<double>(written) / seconds : 0.0)
         << " packets/s" << endl;

    close_shared_memory_fd(ctrl_fd);
    close_shared_memory_fd(fd);

    return EXIT_SUCCESS;
  }
