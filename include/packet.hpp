#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>

#define NUM_TRANSACTIONS 1'000'000

struct test_packet_t {
    uint64_t seq;
    uint16_t data;
};

#endif