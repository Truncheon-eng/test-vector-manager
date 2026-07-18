package test_pkg;

    import uvm_pkg::*;
    `include "uvm_macros.svh"

    localparam int DPI_SUCCESS      = 0;
    localparam int DPI_ERROR        = 1;
    localparam int DPI_BUFFER_EMPTY = 2;
    localparam int DPI_BUFFER_FULL  = 3;

    localparam int TEST_VALUES_COUNT = 8;


    import "DPI-C" function int shared_memory_open();

    import "DPI-C" function int shared_memory_truncate(
        input int              fd,
        input longint unsigned size
    );

    import "DPI-C" function int close_shared_memory_fd(
        input int fd
    );

    import "DPI-C" function longint unsigned get_ring_buffer_size();

    import "DPI-C" function int write_data(
        input int          fd,
        input int unsigned value
    );

    import "DPI-C" function int read_data(
        input  int          fd,
        output int unsigned value
    );

    import "DPI-C" function int clear_ring_buffer(
        input int fd
    );


    class my_test extends uvm_test;

        `uvm_component_utils(my_test)

        function new(
            string        name = "my_test",
            uvm_component parent = null
        );
            super.new(name, parent);
        endfunction


        task check_result(
            input int    actual,
            input int    expected,
            input string operation
        );
            if (actual != expected) begin
                `uvm_fatal(
                    "RING_BUFFER",
                    $sformatf(
                        "%s failed: expected=%0d, actual=%0d",
                        operation,
                        expected,
                        actual
                    )
                )
            end
        endtask


        task run_phase(uvm_phase phase);
            int              fd;
            int              result;
            int unsigned     read_value;
            longint unsigned shared_memory_size;

            int unsigned values[TEST_VALUES_COUNT];

            phase.raise_objection(this);

            values = '{
                32'hCAFE_0001,
                32'hCAFE_0002,
                32'hDEAD_BEEF,
                32'h1234_5678,
                32'hAABB_CCDD,
                32'h0000_0000,
                32'hFFFF_FFFF,
                32'h1357_2468
            };

            shared_memory_size = get_ring_buffer_size();

            `uvm_info(
                "RING_BUFFER",
                $sformatf("Ring buffer size: %0d bytes", shared_memory_size),
                UVM_LOW
            )

            fd = shared_memory_open();

            if (fd < 0) begin
                `uvm_fatal("RING_BUFFER", "shared_memory_open() failed")
            end

            result = shared_memory_truncate(fd, shared_memory_size);

            check_result(result, DPI_SUCCESS, "shared_memory_truncate");

            result = clear_ring_buffer(fd);

            check_result(result, DPI_SUCCESS, "initial clear_ring_buffer");

            foreach (values[i]) begin
                `uvm_info(
                    "RING_BUFFER",
                    $sformatf("Writing values[%0d] = 0x%08X", i, values[i]),
                    UVM_LOW
                )

                result = write_data(fd, values[i]);

                check_result(
                    result,
                    DPI_SUCCESS,
                    $sformatf("write_data[%0d]", i)
                );
            end

            foreach (values[i]) begin
                read_value = 0;

                result = read_data(fd, read_value);

                check_result(
                    result,
                    DPI_SUCCESS,
                    $sformatf("read_data[%0d]", i)
                );

                if (read_value !== values[i]) begin
                    `uvm_fatal(
                        "RING_BUFFER",
                        $sformatf(
                            "Mismatch at index %0d: expected 0x%08X, got 0x%08X",
                            i,
                            values[i],
                            read_value
                        )
                    )
                end

                `uvm_info(
                    "RING_BUFFER",
                    $sformatf("Read values[%0d] = 0x%08X",i, read_value),
                    UVM_LOW
                )
            end

            read_value = 32'hDEAD_BEEF;
            result = read_data(fd, read_value);

            check_result(result, DPI_BUFFER_EMPTY, "read from drained buffer");

            foreach (values[i]) begin
                result = write_data(fd, values[i]);
                check_result(result, DPI_SUCCESS, $sformatf("second write_data[%0d]", i));
            end

            result = clear_ring_buffer(fd);

            check_result(result, DPI_SUCCESS, "clear_ring_buffer");

            read_value = 32'hDEAD_BEEF;
            result = read_data(fd, read_value);

            check_result(result, DPI_BUFFER_EMPTY, "read after clear_ring_buffer");

            result = close_shared_memory_fd(fd);

            check_result(result, DPI_SUCCESS, "close_shared_memory_fd");

            `uvm_info("RING_BUFFER", "All checks passed", UVM_LOW)

            phase.drop_objection(this);
        endtask

    endclass

endpackage


module test_dpi;

    import uvm_pkg::*;
    import test_pkg::*;

    initial begin
        run_test("my_test");
    end

endmodule