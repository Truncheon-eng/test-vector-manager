`ifndef TVM_TYPES_PKG_SV
`define TVM_TYPES_PKG_SV

// ============================================================
// TVM shared types & DPI-C bridge declarations
// ============================================================
package tvm_types_pkg;

  // Status codes shared with the C++ side (ipc/include/auxiliary.hpp: DpiResult)
  typedef enum int {
    DPI_SUCCESS      = 0,
    DPI_ERROR        = 1,
    DPI_BUFFER_EMPTY = 2,
    DPI_BUFFER_FULL  = 3,
    DPI_TIMEOUT      = 4,
    DPI_PEER_GONE    = 5,
    DPI_STREAM_END   = 6
  } dpi_result_e;

  typedef bit [15:0]       lfsr_state_t;     // LFSR states and payloads
  typedef longint unsigned stat_counter_t;   // Simulation counters
  typedef string           report_queue_t[$]; // Report line queues

  // Service commands of the control channel (mirror of ipc/include/ctrl_packet.hpp)
  typedef enum int {
    CTRL_NOP          = 0,
    CTRL_START_STREAM = 1,  // arg: packets to stream (0 = producer default)
    CTRL_STOP_STREAM  = 2,  // arg: ignored - finish the stream, mark done
    CTRL_LOAD_VECTORS = 3,  // arg: vector set id (see tvm_refmodel::seed_for_set)
    CTRL_SET_RATE     = 4   // arg: packets per second (0 = max rate)
  } ctrl_cmd_e;

  // DPI-C imports (implemented in client/dpi_wrapper.cpp)
  import "DPI-C" context function int dpi_shared_memory_open();
  import "DPI-C" context function int dpi_shared_memory_close(input int fd);
  import "DPI-C" context function int dpi_write_data (int            fd,
                                                      stat_counter_t seq,
                                                      int unsigned   data
  );
  import "DPI-C" context function int dpi_read_data(input  int            fd,
                                                    output stat_counter_t seq,
                                                    output int unsigned   data
  );
  // Blocking burst read: waits up to timeout_us of host time for at least one
  // packet, stages up to max_count packets, actual count is returned in count
  import "DPI-C" context function int dpi_read_burst (int            fd,
                                                      int            timeout_us,
                                                      int            max_count,
                                                      output int     count
  );
  // Fetch packet idx from the staging buffer filled by dpi_read_burst
  import "DPI-C" context function int dpi_burst_packet (int                 idx,
                                                        output stat_counter_t seq,
                                                        output int unsigned   data
  );
  // Control channel (sim -> producer): service commands
  import "DPI-C" context function int dpi_ctrl_open();
  import "DPI-C" context function int dpi_ctrl_close(input int fd);
  import "DPI-C" context function int dpi_ctrl_send(int            fd,
                                                    int            cmd,
                                                    stat_counter_t arg
  );

endpackage : tvm_types_pkg

`endif // TVM_TYPES_PKG_SV
