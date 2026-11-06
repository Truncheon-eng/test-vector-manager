`ifndef TVM_TEST_PKG_SV
`define TVM_TEST_PKG_SV

// ============================================================
// TVM test package: UVM environment components
// Structure follows the easyUVM example (next_task/easyUVM):
//   transaction -> config -> monitor -> refmodel -> scoreboard -> env -> test
// ============================================================
package tvm_test_pkg;

  import uvm_pkg::*;
  import tvm_types_pkg::*;

  `include "uvm_macros.svh"

  `include "tvm_packet.sv"
  `include "tvm_config.sv"
  `include "tvm_monitor.sv"
  `include "tvm_refmodel.sv"
  `include "tvm_scoreboard.sv"
  `include "tvm_env.sv"
  `include "stress_test.sv"

endpackage : tvm_test_pkg

`endif // TVM_TEST_PKG_SV
