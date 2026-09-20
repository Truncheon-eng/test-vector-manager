`include "uvm_macros.svh"

import uvm_pkg::*;

// ============================================================
// Type Definitions & DPI-C Configurations
// ============================================================
typedef enum int {
    DPI_SUCCESS      = 0,
    DPI_ERROR        = 1,
    DPI_BUFFER_EMPTY = 2,
    DPI_BUFFER_FULL  = 3
} dpi_result_e;

  // Custom types defined via typedef for scalability
  typedef bit [15:0]      lfsr_state_t;    // Type for LFSR states and payloads
  typedef longint unsigned stat_counter_t;  // Type for all simulation counters
  typedef string           report_queue_t[$]; // Type for report string queues

// DPI-C function imports using explicit context tags
import "DPI-C" context function int dpi_shared_memory_open();

import "DPI-C" context function int dpi_read_data(
    input  int            fd,
    output stat_counter_t seq,
    output int unsigned   data
);

// ============================================================
// Test Component Class
// ============================================================
  class stress_test extends uvm_test;
    `uvm_component_utils(stress_test)

    // Configuration constants using the typed definition without localparam
    const lfsr_state_t LFSR_SEED = 16'hACE1;
    stat_counter_t     num_transactions = 1000;

    // Internal state descriptors
    protected int fd = -1;

    // Simulation metrics and counters standardized via stat_counter_t
    stat_counter_t received_count   = 0;
    stat_counter_t empty_count      = 0;
    stat_counter_t sequence_errors  = 0;
    stat_counter_t data_errors      = 0;

    // Component constructor
    function new(string name = "stress_test", uvm_component parent = null);
        super.new(name, parent);
    endfunction

    // --------------------------------------------------------
    // 1. Build Phase: Component configuration
    // --------------------------------------------------------
    virtual function void build_phase(uvm_phase phase);
        super.build_phase(phase);
        
        // Retrieve transaction count parameter from the command line (+NUM_TRANSACTIONS=...)
        void'($value$plusargs("NUM_TRANSACTIONS=%d", num_transactions));
        `uvm_info(get_type_name(), $sformatf("Configured NUM_TRANSACTIONS = %0d", num_transactions), UVM_NONE)
    endfunction : build_phase

    // --------------------------------------------------------
    // 2. Helper Functions: Decomposed verification logic
    // --------------------------------------------------------

    // Mathematical LFSR generator using the typed reference
    function automatic lfsr_state_t lfsr_next(ref lfsr_state_t state);
        bit feedback = state[0] ^ state[2] ^ state[3] ^ state[5];
        state = {feedback, state[15:1]};
        return state;
    endfunction : lfsr_next

    // Verifies the incoming packet ordering (Sequence Check)
    function automatic void check_sequence (stat_counter_t actual,
                                            stat_counter_t expected);
        if (actual != expected) begin
            sequence_errors++;
            if (sequence_errors <= 10) begin
                `uvm_error({get_full_name(), "SEQ_ERROR"}, 
                            $sformatf("Expected seq=%0d, received seq=%0d", 
                                      expected, actual))
            end
        end
    endfunction : check_sequence

    // Verifies the packet contents validity (Payload Check)
    function automatic void check_payload(int unsigned actual_data,
                                          lfsr_state_t expected_data, 
                                          stat_counter_t seq);

        if (actual_data[15:0] != expected_data) begin
            data_errors++;
            if (data_errors <= 10) begin
                `uvm_error({get_full_name(), "DATA_ERROR"}, 
                            $sformatf("seq=%0d: expected=0x%04X, received=0x%04X", 
                                      seq, 
                                      expected_data, 
                                      actual_data[15:0]))
            end
        end
    endfunction : check_payload

    // Periodic simulation progress logger
    function automatic void log_progress (stat_counter_t seq, 
                                          int unsigned actual_data, 
                                          lfsr_state_t expected_data);

        if ((received_count % 1000) == 0) begin
            `uvm_info({get_full_name(), ".PACKET"}, 
                      $sformatf("seq=%0d data=0x%04X expected=0x%04X", 
                                seq, 
                                actual_data[15:0],
                                expected_data), 
                      UVM_LOW)
        end
    endfunction : log_progress

    // --------------------------------------------------------
    // 3. Run Phase: Execution and traffic processing
    // --------------------------------------------------------
    virtual task run_phase(uvm_phase phase);
        int            status;
        stat_counter_t received_sequence;
        stat_counter_t expected_sequence = 0;
        int unsigned   received_data;

        lfsr_state_t   lfsr_state = LFSR_SEED;
        lfsr_state_t   expected_data;

        phase.raise_objection(this);

        // Initialize IPC connection via DPI
        fd = dpi_shared_memory_open();
        if (fd < 0) begin
            `uvm_fatal({get_full_name(), "SHM_OPEN_FATAL"}, 
                        "dpi_shared_memory_open() failed. Server not running?")
        end
        `uvm_info(get_full_name(), 
                  $sformatf("SHM_OK: Shared memory opened successfully, fd=%0d", 
                            fd), 
                  UVM_LOW)

        // Reset tracking counters before processing loop
        received_count  = 0;
        empty_count     = 0;
        sequence_errors = 0;
        data_errors     = 0;

        // Main transaction processing loop
        while (received_count < num_transactions) begin
            status = dpi_read_data(fd, received_sequence, received_data);

            case (status)
                DPI_SUCCESS: begin
                    // Data successfully retrieved, proceed to evaluation
                end

                DPI_BUFFER_EMPTY: begin
                    empty_count++;
                    continue; // Skip verification step, wait for next token
                end

                DPI_BUFFER_FULL: begin
                    `uvm_error("READ_WARN", "dpi_read_data() returned DPI_BUFFER_FULL")
                    continue;
                end

                DPI_ERROR: begin
                    `uvm_fatal("READ_FATAL", "dpi_read_data() returned DPI_ERROR. Connection lost.")
                end

                default: begin
                    `uvm_fatal("READ_FATAL", $sformatf("Unknown DPI result code: %0d", status))
                end
            endcase

            // Computing golden values and evaluating transactions
            expected_data = lfsr_next(lfsr_state);
            
            check_sequence(received_sequence, expected_sequence);
            check_payload(received_data, expected_data, received_sequence);
            log_progress(received_sequence, received_data, expected_data);

            received_count++;
            expected_sequence++;
        end

        phase.drop_objection(this);
    endtask : run_phase

    // --------------------------------------------------------
    // 4. Extract Phase: Post-simulation resource cleanup
    // --------------------------------------------------------
    virtual function void extract_phase(uvm_phase phase);
        super.extract_phase(phase);
        
        if (fd >= 0) begin
            `uvm_info({get_full_name(), "SHM_CLEAN"},
            $sformatf("Closing shared memory descriptor fd=%0d", fd),
                      UVM_HIGH)
        end
    endfunction : extract_phase

    // --------------------------------------------------------
    // 5. Report Phase: Status evaluation and dashboard output
    // --------------------------------------------------------
    virtual function void report_phase(uvm_phase phase);
        bit            is_success;
        report_queue_t msg;     
        string         final_report;

        is_success = (received_count == num_transactions) && (sequence_errors == 0) && (data_errors == 0);
        
        // Populate the string queue safely
        msg.push_back("\n========================================");
        msg.push_back("Stress test result");
        msg.push_back("========================================");
        msg.push_back($sformatf("Requested:       %0d", num_transactions));
        msg.push_back($sformatf("Received:        %0d", received_count));
        msg.push_back($sformatf("Empty reads:     %0d", empty_count));
        msg.push_back($sformatf("Sequence errors: %0d", sequence_errors));
        msg.push_back($sformatf("Data errors:     %0d", data_errors));
        msg.push_back($sformatf("Test status:     %s", (is_success ? "SUCCESS" : "FAILURE")));
        msg.push_back("========================================");

        // Flatten the queue for clean console display
        final_report = "";
        foreach (msg[i]) begin
            final_report = {final_report, msg[i], "\n"};
        end

        `uvm_info({get_full_name(), ".STRESS_RESULT"}, final_report, UVM_NONE)

        super.report_phase(phase);
    endfunction : report_phase

endclass : stress_test

  // ============================================================
  // Top Execution Scope
  // ============================================================
  module stress_test_top;
      initial begin
          run_test("stress_test");
      end
  endmodule : stress_test_top
