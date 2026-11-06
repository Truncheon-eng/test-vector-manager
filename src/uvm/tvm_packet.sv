`ifndef TVM_PACKET_SV
`define TVM_PACKET_SV

// ============================================================
// TVM stream transaction (mirrors C++ test_packet_t)
// ============================================================
class tvm_packet extends uvm_sequence_item;
  stat_counter_t seq;   // sequence number assigned by the producer
  lfsr_state_t   data;  // LFSR payload

  `uvm_object_utils_begin(tvm_packet)
    `uvm_field_int(seq,  UVM_ALL_ON | UVM_DEC)
    `uvm_field_int(data, UVM_ALL_ON | UVM_HEX)
  `uvm_object_utils_end

  function new(string name = "tvm_packet");
    super.new(name);
  endfunction : new

  virtual function string convert2string();
    return $sformatf("seq=%0d data=0x%04X", seq, data);
  endfunction : convert2string
endclass : tvm_packet

`endif // TVM_PACKET_SV
