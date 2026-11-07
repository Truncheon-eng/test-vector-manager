#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>

#ifndef NUM_TRANSACTIONS
#define NUM_TRANSACTIONS 1000000ULL
#endif

  struct test_packet_t {
    uint64_t seq;
    uint16_t data;
  };

#endif
