`timescale 1ns/1ns

// ============================================================
// TVM Testchamber Top
// ------------------------------------------------------------
// Structure:
//   tvm_types_pkg.sv - shared typedefs + DPI-C imports
//   stress_test.sv   - stress_test class (included into tvm_test_pkg)
//   tvm_test_pkg.sv  - UVM test package
//   uvm_testchamber.sv - this top module
// ============================================================
module stress_test_top;

  import uvm_pkg::*;
  import tvm_test_pkg::*;

  initial begin
    run_test("stress_test");
  end

endmodule : stress_test_top
