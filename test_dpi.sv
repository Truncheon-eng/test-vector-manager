import uvm_pkg::*;
`include "uvm_macros.svh"

`define ARRAY_SIZE 8
`define INT_SIZE_BYTES 4

module test_dpi;

    import "DPI-C" function int shared_memory_open();
    import "DPI-C" function int shared_memory_truncate(
        input int              fd,
        input longint unsigned size
    );
    import "DPI-C" function int close_shared_memory_fd(input int fd);
    import "DPI-C" function int write_array(
        input int fd,
        input int data[],
        input int size
    );
    import "DPI-C" function int read_array(
        input  int fd,
        output int data[],
        input  int size
    );


    class my_test extends uvm_test;
        `uvm_component_utils(my_test)

        function new(string name, uvm_component parent);
            super.new(name, parent);
        endfunction


        task run_phase(uvm_phase phase);
            int fd;
            int result;

            int read_data_array[`ARRAY_SIZE];
            int write_data_array[`ARRAY_SIZE];

            longint unsigned shared_memory_size;

            phase.raise_objection(this);

            shared_memory_size =
                `ARRAY_SIZE * `INT_SIZE_BYTES;

            foreach (write_data_array[i]) begin
                write_data_array[i] = 32'hCAFE_0000 + i;
            end

            foreach (read_data_array[i]) begin
                read_data_array[i] = 0;
            end


            `uvm_info("SV", "Opening shared memory", UVM_LOW)

            fd = shared_memory_open();

            if (fd < 0) begin
                `uvm_fatal("SV", "shared_memory_open() failed")
            end


            `uvm_info(
                "SV",
                $sformatf("Truncating shared memory to %0d bytes", shared_memory_size),
                UVM_LOW
            )

            result = shared_memory_truncate(fd, shared_memory_size);

            if (result != 0) begin
                `uvm_fatal("SV", "shared_memory_truncate() failed")
            end

            `uvm_info("SV", "Reading array from shared memory", UVM_LOW)
            result = read_array(fd, read_data_array, ARRAY_SIZE);

            if (result != 0) begin
                `uvm_fatal("SV", "read_array() failed")
            end


            foreach (read_data_array[i]) begin
                `uvm_info("SV",
                    $sformatf("read_data_array[%0d] = 0x%08X", i, read_data_array[i]),
                    UVM_LOW
                )
            end


            `uvm_info("SV", "Writing array to shared memory", UVM_LOW)

            foreach (write_data_array[i]) begin
                `uvm_info(
                    "SV",
                    $sformatf("write_data_array[%0d] = 0x%08X", i, write_data_array[i]),
                    UVM_LOW
                )
            end


            result = write_array(fd,write_data_array,`ARRAY_SIZE);
            if (result != 0) begin
                `uvm_fatal("SV","write_array() failed")
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