#include "../include/lfsr.hpp"
#include "../include/auxiliary.hpp"
#include "../include/packet.hpp"
#include <chrono>
#include <cstdlib>
#include <thread>

std::ostream& operator<<(std::ostream& out, const test_packet_t& packet) {
    out << packet.data;
    return out;
}

int main() {
    uint16_t lfsr_state = LFSR_SEED;

    uint64_t generated {0};
    uint64_t written {0};
    uint64_t full_count {0};

    int fd = shared_memory_open();
    if (fd < 0)
        return EXIT_FAILURE;

    shared_memory_truncate(fd, PAGE_SIZE);

    clear_ring_buffer<test_packet_t>(fd); // специальная очистка буфера

    for(uint64_t i = 0; i < NUM_TRANSACTIONS; i++) {
        test_packet_t packet;
        packet.seq = i;
        packet.data = lfsr_next(lfsr_state);
        generated++;


        while (true) {
            int state = write_data(fd, packet);

            if (state == DpiResult::DPI_SUCCESS) {
                written++;
                break;
            }

            // std::this_thread::sleep_for(std::chrono::milliseconds(100));
            full_count++;
        }
    }
    
    cout
        << "Generated:   " << generated << endl
        << "Written:     " << written << endl
        << "Buffer full: " << full_count << endl;

    close_shared_memory_fd(fd);

    return EXIT_SUCCESS;
}