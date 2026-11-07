#ifndef CTRL_PACKET_HPP
#define CTRL_PACKET_HPP

#include <cstdint>

// Service/control channel: a separate POSIX shm segment carrying commands
// from the consumer (UVM testbench) to the producer (tvm_server).
// Roles in this ring are reversed: the sim writes, the server reads.
#define CTRL_FILENAME "/tvm_ctrl"

  enum ctrl_cmd_t : uint32_t {
    CTRL_NOP          = 0,
    CTRL_START_STREAM = 1,  // arg: packets to stream (0 = producer default)
    CTRL_STOP_STREAM  = 2,  // arg: ignored - finish the stream, mark done
    CTRL_LOAD_VECTORS = 3,  // arg: vector set id (see vector_set_seed)
    CTRL_SET_RATE     = 4   // arg: packets per second (0 = max rate)
  };

  struct ctrl_packet_t {
    uint32_t cmd;
    uint32_t reserved;
    uint64_t arg;
  };  // 16 bytes, trivially copyable, standard layout

  // Vector sets shared by the producer and the UVM reference model.
  // Mirrored in uvm/tvm_refmodel.sv: seed_for_set - keep in sync!
  constexpr int NUM_VECTOR_SETS = 4;

  inline uint16_t vector_set_seed(int set_id) {
    static constexpr uint16_t seeds[NUM_VECTOR_SETS] = {
      0xACE1,  // set 0 - default (matches LFSR_SEED)
      0xBEEF,  // set 1
      0xCAFE,  // set 2
      0xF00D   // set 3
    };

    if (set_id < 0 || set_id >= NUM_VECTOR_SETS)
      return seeds[0];

    return seeds[set_id];
  }

#endif
