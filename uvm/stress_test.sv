`include "uvm_macros.svh"

import uvm_pkg::*;




typedef enum int {
    DPI_SUCCESS      = 0,
    DPI_ERROR        = 1,
    DPI_BUFFER_EMPTY = 2,
    DPI_BUFFER_FULL  = 3
} dpi_result_e;

import "DPI-C"
function int dpi_shared_memory_open();


import "DPI-C"
function int dpi_read_data(
    input  int              fd,
    output longint unsigned seq,
    output int unsigned     data
);

class stress_test extends uvm_test;

    `uvm_component_utils(stress_test)

    localparam bit [15:0] LFSR_SEED = 16'hACE1;

    longint unsigned num_transactions;

    longint unsigned received_count;

    longint unsigned empty_count;
    longint unsigned sequence_errors;
    longint unsigned data_errors;

    function new(
        string name = "stress_test",
        uvm_component parent = null
    );

        super.new(name, parent);

    endfunction

    function automatic bit [15:0] lfsr_next(
        ref bit [15:0] state
    );

        bit feedback;

        feedback =
            state[0] ^
            state[2] ^
            state[3] ^
            state[5];

        state = {
            feedback,
            state[15:1]
        };

        return state;

    endfunction

    task run_phase(
        uvm_phase phase
    );

        int fd;
        int status;

        longint unsigned received_sequence;
        longint unsigned expected_sequence;

        int unsigned received_data;

        bit [15:0] lfsr_state;
        bit [15:0] expected_data;


        phase.raise_objection(this);

        num_transactions = 1000;

        void'(
            $value$plusargs(
                "NUM_TRANSACTIONS=%d",
                num_transactions
            )
        );


        `uvm_info(
            "CONFIG",
            $sformatf(
                "Transactions = %0d",
                num_transactions
            ),
            UVM_NONE
        )

        fd = dpi_shared_memory_open();


        if (fd < 0) begin

            `uvm_fatal(
                "SHM_OPEN",
                "dpi_shared_memory_open() failed"
            )

        end


        `uvm_info(
            "SHM",
            $sformatf(
                "Shared memory opened successfully, fd=%0d",
                fd
            ),
            UVM_LOW
        )

        lfsr_state = LFSR_SEED;

        expected_sequence = 0;

        received_count = 0;

        empty_count = 0;
        sequence_errors = 0;
        data_errors = 0;


        `uvm_info(
            "STRESS",
            "Starting receive loop",
            UVM_NONE
        )

        while (
            received_count < num_transactions
        ) begin

            status = dpi_read_data(
                fd,
                received_sequence,
                received_data
            );

            case (status)
                DPI_SUCCESS: begin
                end

                DPI_BUFFER_EMPTY: begin
                    empty_count++;
                    continue;
                end

                DPI_ERROR: begin
                    `uvm_fatal(
                        "READ_ERROR",
                        "dpi_read_data() returned DPI_ERROR"
                    )
                end

                DPI_BUFFER_FULL: begin
                    `uvm_error(
                        "READ_ERROR",
                        "dpi_read_data() returned DPI_BUFFER_FULL"
                    )
                    continue;
                end

                default: begin
                    `uvm_fatal(
                        "READ_ERROR",
                        $sformatf(
                            "Unknown DPI result code: %0d",
                            status
                        )
                    )
                end

            endcase

            expected_data = lfsr_next(lfsr_state);

            if (received_sequence != expected_sequence) begin
                sequence_errors++;
                if (sequence_errors <= 10) begin
                    `uvm_error(
                        "SEQ_ERROR",
                        $sformatf(
                            "Expected seq=%0d, received seq=%0d",
                            expected_sequence,
                            received_sequence
                        )
                    )
                end
            end

            if (received_data[15:0] != expected_data) begin
                data_errors++;
                if (data_errors <= 10) begin
                    `uvm_error(
                        "DATA_ERROR",
                        $sformatf(
                            "seq=%0d expected=0x%04X received=0x%04X",
                            received_sequence,
                            expected_data,
                            received_data[15:0]
                        )
                    )
                end
            end

            if (received_count < 10) begin
                `uvm_info(
                    "PACKET",
                    $sformatf(
                        "seq=%0d data=0x%04X expected=0x%04X",
                        received_sequence,
                        received_data[15:0],
                        expected_data
                    ),
                    UVM_LOW
                )
            end

            received_count++;
            expected_sequence++;
        end

        `uvm_info(
            "STRESS_RESULT",
            $sformatf(
                {
                    "\n",
                    "========================================\n",
                    "Stress test result\n",
                    "========================================\n",
                    "Requested:       %0d\n",
                    "Received:        %0d\n",
                    "Empty reads:     %0d\n",
                    "Sequence errors: %0d\n",
                    "Data errors:     %0d\n",
                    "========================================"
                },
                num_transactions,
                received_count,
                empty_count,
                sequence_errors,
                data_errors
            ),
            UVM_NONE
        )

        if (received_count == num_transactions &&
            sequence_errors == 0 &&
            data_errors == 0) begin

            `uvm_info(
                "STRESS_RESULT",
                "PASS",
                UVM_NONE
            )
        end else begin

            `uvm_error(
                "STRESS_RESULT",
                "FAIL"
            )
        end


        phase.drop_objection(this);

    endtask

endclass

module stress_test_top;
    initial begin
        run_test("stress_test");
    end
endmodule