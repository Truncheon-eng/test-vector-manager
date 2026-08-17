
#include <chrono>
#include <cstdlib>
#include <thread>

#include "../include/packet.hpp"
#include "../include/auxiliary.hpp"
#include "../include/lfsr.hpp"


std::ostream& operator<<(std::ostream& out, const test_packet_t& packet) {
    out << packet.data;
    return out;
}

int main() {
    uint64_t received {0};
    uint64_t expected_sequence {0};

    uint64_t empty_count {0};
    uint64_t sequence_errors {0};
    uint64_t data_errors {0};

    uint16_t lfsr_state = LFSR_SEED;

    int fd = shared_memory_open();
    if (fd < 0)
        return EXIT_FAILURE;

    shared_memory_truncate(fd, PAGE_SIZE);

    while (received < NUM_TRANSACTIONS) {
        test_packet_t packet {};
        int status = read_data(fd, &packet);

        if (status != DpiResult::DPI_SUCCESS) {
            empty_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        uint16_t expected_data = lfsr_next(lfsr_state);
        if (packet.seq != expected_sequence) {
            sequence_errors++;
            if (sequence_errors <= 10) {
                std::cerr
                    << "[SEQUENCE ERROR] "
                    << "expected=" << expected_sequence
                    << " received=" << packet.seq
                    << endl;
            }
        }

        if (packet.data != expected_data) {
            data_errors++;
            if (data_errors <= 10) {
                std::cerr
                    << "[DATA ERROR] "
                    << "sequence=" << packet.seq
                    << " expected=0x" << std::hex
                    << expected_data
                    << " received=0x"
                    << packet.data
                    << std::dec
                    << '\n';
            }
        }

        expected_sequence++;
        received++;
    }

    std::cout
        << "\n==============================\n"
        << "Stress test result\n"
        << "==============================\n"
        << "Received:        " << received << '\n'
        << "Buffer empty:    " << empty_count << '\n'
        << "Sequence errors: " << sequence_errors << '\n'
        << "Data errors:     " << data_errors << '\n';


    if (sequence_errors == 0 && data_errors == 0) {
        std::cout << "RESULT: PASS" << endl;
        return EXIT_SUCCESS;
    }

    std::cout << "RESULT: FAIL" << endl;
    return EXIT_FAILURE;
}