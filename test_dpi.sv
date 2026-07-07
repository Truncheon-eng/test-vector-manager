import uvm_pkg::*;
`include "uvm_macros.svh"

module test_dpi;

    import "DPI-C" function int shared_memory_open();
    import "DPI-C" function int shared_memory_truncate(input int fd, 
        input longint unsigned size);
    import "DPI-C" function int close_shared_memory_fd(input int fd);
    import "DPI-C" function int write_data(input int fd, input int _write_data);
    import "DPI-C" function int read_data(input int fd, output int _read_data);

    class my_test extends uvm_test;
        `uvm_component_utils(my_test)

        function new(string name, uvm_component parent);
            super.new(name, parent);
        endfunction

        task run_phase(uvm_phase phase);
            int fd;
            int result;
            int w_data;
            int r_data;

            phase.raise_objection(this);

            w_data = 32'hCAFEBABE;

            `uvm_info("SV", "Opening shared memory", UVM_LOW)
            fd = shared_memory_open();

            if (fd < 0) begin
                `uvm_fatal("SV", "shared_memory_open() failed")
            end

            `uvm_info("SV", "Truncating shared memory", UVM_LOW)
            result = shared_memory_truncate(fd, 4);

            if (result != 0) begin
                `uvm_fatal("SV", "shared_memory_truncate() failed")
            end

            `uvm_info("SV", $sformatf("Writing data: 0x%08X", w_data), UVM_LOW)
            result = write_data(fd, w_data);

            if (result != 0) begin
                `uvm_fatal("SV", "write_data() failed")
            end

            `uvm_info("SV", "Reading data from C++ shared memory", UVM_LOW)
            result = read_data(fd, r_data);

            if (result != 0) begin
                `uvm_fatal("SV", "read_data() failed")
            end

            `uvm_info("SV", $sformatf("Read data: 0x%08X", r_data), UVM_LOW)

            if (r_data == w_data) begin
                `uvm_info("SV", "TEST PASSED: read data matches written data", UVM_LOW)
            end else begin
                `uvm_error("SV", $sformatf("TEST FAILED: expected 0x%08X, got 0x%08X", w_data, r_data))
            end

            result = close_shared_memory_fd(fd);

            if (result != 0) begin
                `uvm_error("SV", "close_shared_memory_fd() failed")
            end

            phase.drop_objection(this);
        endtask
    endclass

    initial begin
        uvm_top.set_report_id_action_hier("UVM/RELNOTES", UVM_NO_ACTION);
        uvm_top.set_report_id_action_hier("NO_DPI_TSTNAME", UVM_NO_ACTION);

        run_test("my_test");
    end



endmodule