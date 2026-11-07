`ifndef TVM_CONTROLLER_SV
`define TVM_CONTROLLER_SV

// ============================================================
// TVM Controller: issues service commands to the producer via
// the control channel (LOAD_VECTORS / START_STREAM / STOP_STREAM).
//
// Commands are sent in start_of_simulation_phase - BEFORE any
// run_phase process can block inside a host-blocking DPI read.
// Under Verilator a blocking DPI call cannot be preempted by
// other SV processes, so sending START_STREAM from run_phase
// could deadlock against the monitor's first read_burst.
// ============================================================
class tvm_controller extends uvm_component;
  `uvm_component_utils(tvm_controller)

  tvm_config cfg;

  protected int ctrl_fd = -1;

  function new(string name, uvm_component parent);
    super.new(name, parent);
  endfunction

  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);

    if (!uvm_config_db#(tvm_config)::get(this, "", "cfg", cfg))
      `uvm_fatal("CFG_FATAL", "tvm_config not found in config_db")
  endfunction : build_phase

  virtual function void start_of_simulation_phase(uvm_phase phase);
    int status;

    super.start_of_simulation_phase(phase);

    ctrl_fd = dpi_ctrl_open();
    if (ctrl_fd < 0) begin
      `uvm_warning ("CTRL_OPEN",
                    "Control channel unavailable - producer will use its defaults")
      return;
    end

    if (cfg.vector_set >= 0) begin
      status = dpi_ctrl_send(ctrl_fd, CTRL_LOAD_VECTORS, cfg.vector_set);
      `uvm_info(get_type_name(),
                $sformatf("LOAD_VECTORS set=%0d status=%0d",
                          cfg.vector_set, status),
                UVM_LOW)
    end

    status = dpi_ctrl_send(ctrl_fd, CTRL_START_STREAM, cfg.num_transactions);
    `uvm_info(get_type_name(),
              $sformatf("START_STREAM n=%0d status=%0d",
                        cfg.num_transactions, status),
              UVM_LOW)
  endfunction : start_of_simulation_phase

  // Called by the monitor when the stream is fully consumed
  virtual function void stop_stream();
    if (ctrl_fd >= 0) begin
      int status = dpi_ctrl_send(ctrl_fd, CTRL_STOP_STREAM, 0);
      `uvm_info(get_type_name(),
                $sformatf("STOP_STREAM status=%0d", status),
                UVM_HIGH)
    end
  endfunction : stop_stream

  virtual function void extract_phase(uvm_phase phase);
    super.extract_phase(phase);

    if (ctrl_fd >= 0) begin
      void'(dpi_ctrl_close(ctrl_fd));
      ctrl_fd = -1;
    end
  endfunction : extract_phase
endclass : tvm_controller

`endif // TVM_CONTROLLER_SV
