`ifndef TVM_REFMODEL_SV
`define TVM_REFMODEL_SV

// ============================================================
// TVM Reference Model: golden LFSR generator (mirrors the C++
// server include/lfsr.hpp). Subscribes to the observed stream,
// produces the expected packet for each observed one and
// publishes it for the scoreboard.
//
// resync() re-aligns the golden state after a loss/corruption:
// the received payload IS the producer's current LFSR state.
// ============================================================
class tvm_refmodel extends uvm_component;
  `uvm_component_utils(tvm_refmodel)

  // Golden LFSR seed (must match the server: include/lfsr.hpp)
  const lfsr_state_t LFSR_SEED = 16'hACE1;

  uvm_analysis_imp  #(tvm_packet, tvm_refmodel) actual_in;
  uvm_analysis_port #(tvm_packet)               expected_out;

  // Single reusable expected packet. Safe because the whole analysis chain
  // (monitor.write -> rfm.write -> fifo -> sb.write) is synchronous: the
  // scoreboard pops the packet before the next one is produced, so the
  // fifo never holds more than one reference to this object.
  protected tvm_packet exp;

  protected lfsr_state_t   lfsr_state;
  protected stat_counter_t expected_seq;

  function new(string name, uvm_component parent);
    super.new(name, parent);
    actual_in    = new("actual_in", this);
    expected_out = new("expected_out", this);
  endfunction

  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    exp = tvm_packet::type_id::create("exp");
    reset_model();
  endfunction : build_phase

  virtual function void reset_model();
    lfsr_state   = LFSR_SEED;
    expected_seq = 0;
  endfunction : reset_model

  // Golden LFSR step (identical to the server implementation)
  function automatic lfsr_state_t lfsr_next(ref lfsr_state_t state);
    bit feedback = state[0] ^ state[2] ^ state[3] ^ state[5];
    state = {feedback, state[15:1]};
    return state;
  endfunction : lfsr_next

  // For every observed packet publish the expected one
  virtual function void write(tvm_packet actual);
    exp.seq  = expected_seq;
    exp.data = lfsr_next(lfsr_state);
    expected_seq++;

    expected_out.write(exp);
  endfunction : write

  // Scoreboard feedback after a mismatch: re-align the golden model
  // so a single loss does not cascade into whole-stream failures
  virtual function void resync(tvm_packet actual);
    lfsr_state   = actual.data;
    expected_seq = actual.seq + 1;
  endfunction : resync
endclass : tvm_refmodel

`endif // TVM_REFMODEL_SV
