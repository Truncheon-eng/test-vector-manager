#ifndef LFSR_HPP
#define LFSR_HPP

#include <cstdint>

constexpr uint16_t LFSR_SEED = 0xACE1;

inline uint16_t lfsr_next(uint16_t& state)
{
    const uint16_t feedback =
        ((state >> 0) ^
         (state >> 2) ^
         (state >> 3) ^
         (state >> 5)) & 1u;

    state = static_cast<uint16_t>(
        (state >> 1) |
        (feedback << 15)
    );

    return state;
}

#endif