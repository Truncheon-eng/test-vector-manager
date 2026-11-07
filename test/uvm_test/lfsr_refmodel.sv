`ifndef LFSR_REFMODEL_SV
`define LFSR_REFMODEL_SV

// ============================================================
// Test-specific reference model for the LFSR stress traffic.
// Golden model mirrors the C++ server side:
//   src/lib/ipc/include/lfsr.hpp        (LFSR step and seed)
//   src/lib/ipc/include/ctrl_packet.hpp (vector set seed table)
//
// Injected into the reusable env (tvm_env) via factory override
// in stress_test::build_phase.
// ============================================================
class lfsr_refmodel extends tvm_refmodel;
  `uvm_component_utils(lfsr_refmodel)

  // Golden LFSR seed for vector set 0 (must match the server)
  const tvm_payload_t LFSR_SEED = 16'hACE1;

  tvm_config cfg;

  protected tvm_payload_t lfsr_state;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);

    if (!uvm_config_db#(tvm_config)::get(this, "", "cfg", cfg))
      `uvm_fatal  ({get_type_name(), ".CFG_FATAL"},
                  "tvm_config not found in config_db !")

    reset_model();
  endfunction : build_phase

  // Vector set seeds - mirror of src/lib/ipc/include/ctrl_packet.hpp:
  // vector_set_seed. Keep in sync!
  function automatic tvm_payload_t seed_for_set(int set_id);
    case (set_id)
      1:       return 16'hBEEF;
      2:       return 16'hCAFE;
      3:       return 16'hF00D;
      default: return LFSR_SEED;  // set 0 and fallback
    endcase
  endfunction : seed_for_set

  virtual function void reset_model();
    super.reset_model();
    lfsr_state = seed_for_set((cfg != null) ? cfg.vector_set : 0);
  endfunction : reset_model

  // Golden LFSR step (identical to the server implementation)
  function automatic tvm_payload_t lfsr_next(ref tvm_payload_t state);
    bit feedback = state[0] ^ state[2] ^ state[3] ^ state[5];
    state = {feedback, state[15:1]};
    return state;
  endfunction : lfsr_next

  virtual function void compute_expected(tvm_packet actual, tvm_packet exp_out);
    exp_out.data = lfsr_next(lfsr_state);
  endfunction : compute_expected

  // Resync: the received payload IS the producer's current LFSR state
  virtual function void resync(tvm_packet actual);
    super.resync(actual);
    lfsr_state = actual.data;
  endfunction : resync
endclass : lfsr_refmodel

`endif // LFSR_REFMODEL_SV
