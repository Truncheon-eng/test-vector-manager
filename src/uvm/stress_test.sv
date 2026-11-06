`ifndef STRESS_TEST_SV
`define STRESS_TEST_SV

// ============================================================
// Test Component Class
// (included into tvm_test_pkg; do not compile as a standalone unit)
// ============================================================
class stress_test extends uvm_test;
  `uvm_component_utils(stress_test)

  // Golden LFSR seed (must match the server: include/lfsr.hpp)
  const lfsr_state_t LFSR_SEED = 16'hACE1;

  // Runtime configuration (plusargs)
  stat_counter_t num_transactions = 1000;
  int            burst_max        = 256;        // packets per DPI burst (ring holds SIZE=1024)
  int            read_timeout_us  = 10_000_000; // host-time wait per burst; server absent => fatal

  // Internal state descriptors
  protected int fd = -1;

  // Simulation metrics and counters
  stat_counter_t received_count  = 0;
  stat_counter_t burst_count     = 0;
  stat_counter_t sequence_errors = 0;
  stat_counter_t data_errors     = 0;
  stat_counter_t resync_count    = 0;

  function new(string name = "stress_test", uvm_component parent = null);
    super.new(name, parent);
  endfunction

  // --------------------------------------------------------
  // 1. Build Phase: Component configuration
  // --------------------------------------------------------
  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);

    void'($value$plusargs("NUM_TRANSACTIONS=%d", num_transactions));
    void'($value$plusargs("BURST_MAX=%d", burst_max));
    void'($value$plusargs("READ_TIMEOUT_US=%d", read_timeout_us));

    `uvm_info(get_type_name(),
              $sformatf("Configured NUM_TRANSACTIONS=%0d BURST_MAX=%0d READ_TIMEOUT_US=%0d",
                        num_transactions, burst_max, read_timeout_us),
              UVM_MEDIUM)
  endfunction : build_phase

  // --------------------------------------------------------
  // 2. Helper Functions: Decomposed verification logic
  // --------------------------------------------------------

  // Golden LFSR model (mirrors server include/lfsr.hpp)
  function automatic lfsr_state_t lfsr_next(ref lfsr_state_t state);
    bit feedback = state[0] ^ state[2] ^ state[3] ^ state[5];
    state = {feedback, state[15:1]};
    return state;
  endfunction : lfsr_next

  // Verifies the incoming packet ordering; returns 1 on match
  function automatic bit check_sequence(stat_counter_t actual,
                                        stat_counter_t expected);
    if (actual == expected)
      return 1;

    sequence_errors++;
    if (sequence_errors <= 10) begin
      `uvm_error ("SEQ_ERROR",
                  $sformatf("Expected seq=%0d, received seq=%0d",
                            expected, actual))
    end
    return 0;
  endfunction : check_sequence

  // Verifies the packet contents validity; returns 1 on match
  function automatic bit check_payload (int unsigned   actual_data,
                                        lfsr_state_t   expected_data,
                                        stat_counter_t seq);
    if (actual_data[15:0] == expected_data)
      return 1;

    data_errors++;
    if (data_errors <= 10) begin
      `uvm_error("DATA_ERROR",
                  $sformatf("seq=%0d: expected=0x%04X, received=0x%04X",
                            seq,
                            expected_data,
                            actual_data[15:0]))
    end
    return 0;
  endfunction : check_payload

  // Periodic simulation progress logger
  function automatic void log_progress (stat_counter_t seq,
                                        int unsigned   actual_data,
                                        lfsr_state_t   expected_data);
    if ((received_count % 1000) == 0) begin
      `uvm_info("PACKET",
                $sformatf("seq=%0d data=0x%04X expected=0x%04X",
                          seq, actual_data[15:0], expected_data),
                UVM_LOW)
    end
  endfunction : log_progress

  // --------------------------------------------------------
  // 3. Run Phase: Execution and traffic processing
  // --------------------------------------------------------
  virtual task run_phase(uvm_phase phase);
    int            status;
    int            staged;
    bit            seq_ok;
    bit            data_ok;
    stat_counter_t received_sequence;
    stat_counter_t expected_sequence = 0;
    int unsigned   received_data;

    lfsr_state_t   lfsr_state = LFSR_SEED;
    lfsr_state_t   expected_data;

    phase.raise_objection(this);

    // Initialize IPC connection via DPI
    fd = dpi_shared_memory_open();
    if (fd < 0) begin
      `uvm_fatal ("SHM_OPEN_FATAL",
                  "dpi_shared_memory_open() failed. Server not running?")
      // NB: under Verilator $finish does not preempt a zero-delay loop - exit explicitly
      phase.drop_objection(this);
      return;
    end
    `uvm_info(get_full_name(),
              $sformatf("SHM_OK: shared memory opened, fd=%0d", fd),
              UVM_LOW)

    // Main transaction processing loop:
    // blocking burst reads — all waiting happens on the C++ side (host time),
    // so the testbench contains no # delays at all
    while (received_count < num_transactions) begin
      status = dpi_read_burst(fd, read_timeout_us, burst_max, staged);

      if (status != DPI_SUCCESS) begin
        case (status)
          DPI_TIMEOUT: begin
            `uvm_fatal ("READ_TIMEOUT",
                        $sformatf("No packets in %0dus: received %0d of %0d. Is the server running?",
                                  read_timeout_us, received_count, num_transactions))
          end

          DPI_ERROR: begin
            `uvm_fatal ("READ_FATAL",
                        "dpi_read_burst() returned DPI_ERROR. Connection lost.")
          end

          default: begin
            `uvm_fatal ("READ_FATAL",
                        $sformatf("Unknown DPI result code: %0d", status))
          end
        endcase

        // NB: under Verilator $finish does not preempt a zero-delay loop - exit explicitly
        phase.drop_objection(this);
        return;
      end

      burst_count++;

      for (int i = 0; (i < staged) && (received_count < num_transactions); i++) begin
        status = dpi_burst_packet(i, received_sequence, received_data);
        if (status != DPI_SUCCESS) begin
          `uvm_fatal ("READ_FATAL",
                      $sformatf("dpi_burst_packet(%0d) failed with code %0d", i, status))
          phase.drop_objection(this);
          return;
        end

        // Computing golden values and evaluating transactions
        expected_data = lfsr_next(lfsr_state);

        seq_ok  = check_sequence(received_sequence, expected_sequence);
        data_ok = check_payload(received_data, expected_data, received_sequence);
        log_progress(received_sequence, received_data, expected_data);

        // Resynchronize the golden model after a loss/corruption:
        // the received payload IS the server's current LFSR state
        if (!seq_ok || !data_ok) begin
          lfsr_state        = received_data[15:0];
          expected_sequence = received_sequence;
          resync_count++;
        end

        received_count++;
        expected_sequence++;
      end
    end

    `uvm_info(get_full_name(),
              $sformatf("DONE: received %0d packets in %0d bursts",
                        received_count,
                        burst_count),
              UVM_MEDIUM)

    phase.drop_objection(this);
  endtask : run_phase

  // --------------------------------------------------------
  // 4. Extract Phase: Post-simulation resource cleanup
  // --------------------------------------------------------
  virtual function void extract_phase(uvm_phase phase);
    int rc;

    super.extract_phase(phase);

    if (fd >= 0) begin
      rc = dpi_shared_memory_close(fd);
      if (rc != 0)
        `uvm_warning ("SHM_CLOSE",
                      $sformatf("close(fd=%0d) returned %0d", fd, rc))
      else
        `uvm_info("SHM_CLEAN",
                  $sformatf("Shared memory fd=%0d closed", fd),
                  UVM_HIGH)
      fd = -1;
    end
  endfunction : extract_phase

  // --------------------------------------------------------
  // 5. Report Phase: Status evaluation and dashboard output
  // --------------------------------------------------------
  virtual function void report_phase(uvm_phase phase);
    bit            is_success;
    report_queue_t msg;
    string         final_report;

    super.report_phase(phase);

    is_success =(received_count == num_transactions) &&
                (sequence_errors == 0) &&
                (data_errors == 0);

    // Populate the string queue
    msg.push_back("========================================");
    msg.push_back("Stress test result");
    msg.push_back("========================================");
    msg.push_back($sformatf("Requested:       %0d", num_transactions));
    msg.push_back($sformatf("Received:        %0d", received_count));
    msg.push_back($sformatf("Bursts:          %0d", burst_count));
    msg.push_back($sformatf("Sequence errors: %0d", sequence_errors));
    msg.push_back($sformatf("Data errors:     %0d", data_errors));
    msg.push_back($sformatf("Resyncs:         %0d", resync_count));
    msg.push_back($sformatf("Test status:     %s", (is_success ? "SUCCESS" : "FAILURE")));
    msg.push_back("========================================");

    // Flatten the queue for clean console display
    final_report = "\n";
    foreach (msg[i]) begin
      final_report = {final_report, msg[i], "\n"};
    end

    if (is_success)
      `uvm_info("STRESS_RESULT", final_report, UVM_MEDIUM)
    else
      `uvm_error("STRESS_RESULT", final_report)
  endfunction : report_phase

endclass : stress_test

`endif // STRESS_TEST_SV
