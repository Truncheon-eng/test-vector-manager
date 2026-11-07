`ifndef TVM_ENV_SV
`define TVM_ENV_SV

// ============================================================
// TVM Environment: monitor -> refmodel -> scoreboard
//
//   tvm_monitor ──item_collected_port──> rfm.actual_in
//          │                             (golden LFSR)
//          │                                │ expected_out
//          └──> sb.actual_in                v
//                        │        sb.expected_fifo
//                        └──── compare ────┘
//                                 │ resync feedback ──> rfm
// ============================================================
class tvm_env extends uvm_env;
  `uvm_component_utils(tvm_env)

  tvm_controller ctl;
  tvm_monitor    mon;
  tvm_refmodel   rfm;
  tvm_scoreboard sb;

  function new(string name, uvm_component parent = null);
    super.new(name, parent);
  endfunction

  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    ctl = tvm_controller::type_id::create("ctl", this);
    mon = tvm_monitor::type_id::create("mon", this);
    rfm = tvm_refmodel::type_id::create("rfm", this);
    sb  = tvm_scoreboard::type_id::create("sb", this);
  endfunction : build_phase

  virtual function void connect_phase(uvm_phase phase);
    super.connect_phase(phase);

    // Order matters: refmodel is connected BEFORE the scoreboard, so the
    // expected packet is already queued when the actual one is compared
    mon.item_collected_port.connect(rfm.actual_in);
    mon.item_collected_port.connect(sb.actual_in);
    rfm.expected_out.connect(sb.expected_fifo.analysis_export);

    // Service-command channel: monitor tells the producer to stop
    // once the requested amount of data has been collected
    mon.ctrl = ctl;

    // Resync feedback path
    sb.refmodel = rfm;
  endfunction : connect_phase

  virtual function void report_phase(uvm_phase phase);
    super.report_phase(phase);
    `uvm_info(get_type_name(),
              $sformatf("Reporting matched %0d", sb.match_count),
              UVM_NONE)
    if (sb.sequence_errors || sb.data_errors) begin
      `uvm_error (get_type_name(),
                  $sformatf("Saw %0d sequence and %0d data mismatches",
                            sb.sequence_errors,
                            sb.data_errors))
    end
  endfunction : report_phase
endclass : tvm_env

`endif // TVM_ENV_SV
