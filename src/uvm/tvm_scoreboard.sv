`ifndef TVM_SCOREBOARD_SV
`define TVM_SCOREBOARD_SV

// ============================================================
// TVM Scoreboard: compares observed packets (monitor) against
// expected ones (refmodel), counts mismatches, drives resync
// feedback and prints the final dashboard.
//
// Connection order in env matters: the monitor port is wired to
// the refmodel FIRST, so by the time an actual packet reaches
// write(), the matching expected packet is already in the fifo.
// ============================================================
class tvm_scoreboard extends uvm_scoreboard;
  `uvm_component_utils(tvm_scoreboard)

  uvm_tlm_analysis_fifo #(tvm_packet)             expected_fifo;
  uvm_analysis_imp      #(tvm_packet, tvm_scoreboard) actual_in;

  // Wired by the env: used to re-align the golden model on mismatch
  tvm_refmodel refmodel;

  tvm_config cfg;

  stat_counter_t received        = 0;
  stat_counter_t match_count     = 0;
  stat_counter_t sequence_errors = 0;
  stat_counter_t data_errors     = 0;
  stat_counter_t resync_count    = 0;

  function new(string name, uvm_component parent);
    super.new(name, parent);
    expected_fifo = new("expected_fifo", this);
    actual_in     = new("actual_in", this);
  endfunction

  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);

    if (!uvm_config_db#(tvm_config)::get(this, "", "cfg", cfg))
      `uvm_fatal({get_type_name(), ".CFG_FATAL\t"},
                  "tvm_config not found in config_db")
  endfunction : build_phase

  // Observed packet from the monitor
  virtual function void write(tvm_packet actual);
    tvm_packet exp;
    bit        seq_ok;
    bit        data_ok;

    received++;

    if (!expected_fifo.try_get(exp)) begin
      `uvm_error ({get_type_name(), ".SB_NO_EXPECT\t"},
                  $sformatf("No expected packet for %s",
                            actual.convert2string()))
      return;
    end

    seq_ok  = (actual.seq == exp.seq);
    data_ok = (actual.data == exp.data);

    if (!seq_ok) begin
      sequence_errors++;
      if (sequence_errors <= 10) begin
        `uvm_error ({get_type_name(), ".SEQ_ERROR\t"},
                    $sformatf("Expected seq=%0d, received seq=%0d",
                              exp.seq,
                              actual.seq))
      end
    end

    if (!data_ok) begin
      data_errors++;
      if (data_errors <= 10) begin
        `uvm_error ({get_type_name(), ".DATA_ERROR\t"},
                    $sformatf("seq=%0d: expected=0x%04X, received=0x%04X",
                              actual.seq,
                              exp.data,
                              actual.data))
      end
    end

    if (seq_ok && data_ok) begin
      match_count++;
    end else if (refmodel != null) begin
      refmodel.resync(actual);
      resync_count++;
    end

    if ((received % 1000) == 0) begin
      `uvm_info({get_type_name(), ".PACKET\t"},
                $sformatf("received=%s expected=seq:%0d data:0x%04X",
                          actual.convert2string(), exp.seq, exp.data),
                UVM_MEDIUM)
    end
  endfunction : write

  virtual function void report_phase(uvm_phase phase);
    bit            is_success;
    report_queue_t msg;
    string         final_report;

    super.report_phase(phase);

    is_success =  (received == cfg.num_transactions) &&
                  (sequence_errors == 0) &&
                  (data_errors == 0);

    msg.push_back("========================================");
    msg.push_back("Stress test result");
    msg.push_back("========================================");
    msg.push_back($sformatf("Requested:       %0d", cfg.num_transactions));
    msg.push_back($sformatf("Received:        %0d", received));
    msg.push_back($sformatf("Matches:         %0d", match_count));
    msg.push_back($sformatf("Sequence errors: %0d", sequence_errors));
    msg.push_back($sformatf("Data errors:     %0d", data_errors));
    msg.push_back($sformatf("Resyncs:         %0d", resync_count));
    msg.push_back($sformatf("Test status:     %s", (is_success ? "SUCCESS" : "FAILURE")));
    msg.push_back("========================================");

    final_report = "\n";
    foreach (msg[i]) begin
      final_report = {final_report, msg[i], "\n"};
    end

    if (is_success)
      `uvm_info({get_type_name(), ".STRESS_RESULT\t"},
                final_report,
                UVM_MEDIUM)
    else
      `uvm_error({get_type_name(), ".STRESS_RESULT\t"},
                final_report)
  endfunction : report_phase
endclass : tvm_scoreboard

`endif // TVM_SCOREBOARD_SV
