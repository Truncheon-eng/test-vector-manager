`ifndef TVM_ENV_PKG_SV
`define TVM_ENV_PKG_SV

// ============================================================
// TVM environment package: reusable verification components
//   transaction -> config -> controller -> monitor -> refmodel
//   -> scoreboard -> env
// Concrete tests import this package and add uvm_test classes
// on top (see test/uvm_test/tvm_test_pkg.sv).
// ============================================================
package tvm_env_pkg;

  import uvm_pkg::*;
  import tvm_types_pkg::*;

  `include "uvm_macros.svh"

  `include "tvm_packet.sv"
  `include "tvm_config.sv"
  `include "tvm_controller.sv"
  `include "tvm_monitor.sv"
  `include "tvm_refmodel.sv"
  `include "tvm_scoreboard.sv"
  `include "tvm_env.sv"

endpackage : tvm_env_pkg

`endif // TVM_ENV_PKG_SV
