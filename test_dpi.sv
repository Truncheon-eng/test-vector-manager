import uvm_pkg::*;
`include "uvm_macros.svh"

module test_dpi;
    // Импорт функций
    import "DPI-C" function int write_data(input bit [31:0] data);
    import "DPI-C" function int read_data(output int unsigned data);

    class my_test extends uvm_test;
        `uvm_component_utils(my_test)
        
        function new(string name, uvm_component parent); 
            super.new(name, parent); 
        endfunction

        task run_phase(uvm_phase phase);
            int result;
            int w_data;
            int r_data;

            phase.raise_objection(this);
            
            w_data = 32'hDEADBEEF;;
            `uvm_info("SV", "Writing data to C++ shared memory...", UVM_LOW)
            result = write_data(w_data);

            `uvm_info("SV", "Reading data from C++ shared memory...", UVM_LOW)
            result = read_data(r_data);
            `uvm_info("SV", $sformatf("Read data: 0x%X", r_data), UVM_LOW)

            phase.drop_objection(this);
        endtask
    endclass

    initial begin
        uvm_top.set_report_id_action_hier("UVM/RELNOTES", UVM_NO_ACTION);
        uvm_top.set_report_id_action_hier("NO_DPI_TSTNAME", UVM_NO_ACTION);
        run_test("my_test");
    end
endmodule
