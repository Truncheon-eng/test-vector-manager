`ifndef STRESS_TEST_SV
`define STRESS_TEST_SV

// ============================================================
// Stress Test: builds the TVM environment and feeds it the
// configuration from plusargs. All streaming/checking logic
// lives in the env components (monitor/refmodel/scoreboard).
// ============================================================
class stress_test extends uvm_test;
  `uvm_component_utils(stress_test)

  tvm_env    env_h;
  tvm_config cfg;

  function new(string name = "stress_test", uvm_component parent = null);
    super.new(name, parent);
  endfunction

  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);

    // Inject the test-specific golden model into the reusable env
    // (must happen before the env builds its components)
    tvm_refmodel::type_id::set_type_override(lfsr_refmodel::get_type());

    cfg = tvm_config::type_id::create("cfg");
    cfg.apply_plusargs();
    uvm_config_db#(tvm_config)::set(this, "*", "cfg", cfg);

    env_h = tvm_env::type_id::create("env_h", this);
  endfunction : build_phase
endclass : stress_test

`endif // STRESS_TEST_SV
