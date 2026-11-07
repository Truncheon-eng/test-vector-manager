`ifndef TVM_REFMODEL_SV
`define TVM_REFMODEL_SV

// ============================================================
// TVM Reference Model (abstract base, library part).
//
// Generic per-packet plumbing: subscribes to the observed stream,
// numbers the expected packets and publishes them for the
// scoreboard. The traffic-specific golden model is supplied by
// the TEST: derive from this class, implement compute_expected()
// (and resync() if the traffic allows re-alignment) and register
// a factory override before the env is built - see
// test/uvm_test/lfsr_refmodel.sv for an example.
// ============================================================
class tvm_refmodel extends uvm_component;
  `uvm_component_utils(tvm_refmodel)

  uvm_analysis_imp  #(tvm_packet, tvm_refmodel) actual_in;
  uvm_analysis_port #(tvm_packet)               expected_out;

  // Single reusable expected packet. Safe because the whole analysis
  // chain (monitor.write -> rfm.write -> fifo -> sb.write) is
  // synchronous: the scoreboard pops the packet before the next one
  // is produced, so the fifo never holds more than one reference.
  protected tvm_packet exp;

  protected stat_counter_t expected_seq;

  function new(string name, uvm_component parent);
    super.new(name, parent);
    actual_in    = new("actual_in", this);
    expected_out = new("expected_out", this);
  endfunction

  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    exp = tvm_packet::type_id::create("exp");
  endfunction : build_phase

  // Reset the model to its initial state (derived models extend this
  // and call it from their build_phase after configuration is ready)
  virtual function void reset_model();
    expected_seq = 0;
  endfunction : reset_model

  // Compute the expected payload for the next stream slot.
  // exp_out.seq is filled by the base class; the derived model
  // fills exp_out.data according to the traffic type.
  // NOTE: not declared `pure virtual` on purpose - Verilator-friendly;
  // a missing factory override fails loudly at runtime instead.
  virtual function void compute_expected(tvm_packet actual, tvm_packet exp_out);
    `uvm_fatal ("REFMODEL",
                "compute_expected() not implemented - register a factory override with a concrete reference model")
  endfunction : compute_expected

  // Re-align after a mismatch so one loss does not cascade into
  // whole-stream failures
  virtual function void resync(tvm_packet actual);
    expected_seq = actual.seq + 1;
  endfunction : resync

  // For every observed packet publish the expected one
  virtual function void write(tvm_packet actual);
    exp.seq = expected_seq;
    compute_expected(actual, exp);
    expected_seq++;

    expected_out.write(exp);
  endfunction : write
endclass : tvm_refmodel

`endif // TVM_REFMODEL_SV
