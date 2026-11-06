`ifndef TVM_CONFIG_SV
`define TVM_CONFIG_SV

// ============================================================
// Environment configuration (distributed via uvm_config_db)
// ============================================================
class tvm_config extends uvm_object;
  `uvm_object_utils(tvm_config)

  stat_counter_t num_transactions = 1000;
  int            burst_max        = 256;        // packets per DPI burst (ring holds SIZE=1024)
  int            read_timeout_us  = 10_000_000; // host-time wait per burst; server absent => fatal

  function new(string name = "tvm_config");
    super.new(name);
  endfunction : new

  // Override defaults from the command line
  virtual function void apply_plusargs();
    void'($value$plusargs("NUM_TRANSACTIONS=%d", num_transactions));
    void'($value$plusargs("BURST_MAX=%d", burst_max));
    void'($value$plusargs("READ_TIMEOUT_US=%d", read_timeout_us));
  endfunction : apply_plusargs

  virtual function string convert2string();
    return $sformatf("NUM_TRANSACTIONS=%0d BURST_MAX=%0d READ_TIMEOUT_US=%0d",
                     num_transactions, burst_max, read_timeout_us);
  endfunction : convert2string
endclass : tvm_config

`endif // TVM_CONFIG_SV
