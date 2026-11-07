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
  int            vector_set       = -1;         // -1 = producer default; >=0 = LOAD_VECTORS id
  int            stream_rate_pps  = -1;         // -1 = do not send SET_RATE; >=0 = pps (0 = max)

  function new(string name = "tvm_config");
    super.new(name);
  endfunction : new

  // Override defaults from the command line
  virtual function void apply_plusargs();
    void'($value$plusargs("NUM_TRANSACTIONS=%d", num_transactions));
    void'($value$plusargs("BURST_MAX=%d", burst_max));
    void'($value$plusargs("READ_TIMEOUT_US=%d", read_timeout_us));
    void'($value$plusargs("VECTOR_SET=%d", vector_set));
    void'($value$plusargs("RATE_PPS=%d", stream_rate_pps));
  endfunction : apply_plusargs

  virtual function string convert2string();
    return $sformatf ("NUM_TRANSACTIONS=%0d BURST_MAX=%0d READ_TIMEOUT_US=%0d VECTOR_SET=%0d RATE_PPS=%0d",
                      num_transactions,
                      burst_max,
                      read_timeout_us,
                      vector_set,
                      stream_rate_pps);
  endfunction : convert2string
endclass : tvm_config

`endif // TVM_CONFIG_SV
