#include <cstdint>

#include "../include/auxiliary.hpp"
#include "../include/packet.hpp"

std::ostream& operator<<(std::ostream& out, const test_packet_t& packet) {
    out << packet.data;
    return out;
}

extern "C" {
    int dpi_shared_memory_open()
    {
        return shared_memory_open();
    }

    int dpi_shared_memory_truncate(int fd, uint64_t size) {
        return shared_memory_truncate(fd, size);
    }

    int dpi_read_data(
        int fd,
        uint64_t* seq,
        uint32_t* data
    )
    {
        test_packet_t packet{};

        int rc = read_data<test_packet_t>(fd, &packet);

        if (rc != DPI_SUCCESS)
            return rc;

        *seq  = packet.seq;
        *data = static_cast<uint32_t>(packet.data);

        return DPI_SUCCESS;
    }
}
