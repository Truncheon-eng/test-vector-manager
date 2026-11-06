#include <cstdlib>
#include <iostream>
#include <thread>

#include "auxiliary.hpp"
#include "lfsr.hpp"
#include "packet.hpp"

  std::ostream& operator<<(std::ostream& out, const test_packet_t& packet) {
    out << packet.data;
    return out;
  }

  int main(int argc, char* argv[]) {
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

    for (uint64_t i = 0; i < count; i++) {
      test_packet_t packet;
      packet.seq  = i;
      packet.data = lfsr_next(lfsr_state);
      generated++;

      while (true) {
        int state = write_data(fd, packet);

        if (state == DPI_SUCCESS) {
          written++;
          break;
        }

        if (state == DPI_ERROR) {
          std::cerr << "[x]: write_data failed at packet " << i << std::endl;
          close_shared_memory_fd(fd);
          return EXIT_FAILURE;
        }

        // DPI_BUFFER_FULL: ring is full, wait for the consumer to drain it
        full_count++;
        std::this_thread::yield();
      }
    }

    cout << "Generated:   " << generated << endl
        << "Written:     " << written << endl
        << "Buffer full: " << full_count << endl;

    close_shared_memory_fd(fd);

    return EXIT_SUCCESS;
  }
