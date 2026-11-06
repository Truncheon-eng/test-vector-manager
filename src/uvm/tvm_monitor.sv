`ifndef TVM_MONITOR_SV
`define TVM_MONITOR_SV

// ============================================================
// TVM Monitor: observes the packet stream coming from the C++
// server through the DPI burst interface and publishes every
// packet on the analysis port. Owns the IPC connection.
//
// All waiting happens host-side inside dpi_read_burst (futex),
// so the monitor loop is zero-delay: run_phase stays at @ 0.
// ============================================================
class tvm_monitor extends uvm_monitor;
  `uvm_component_utils(tvm_monitor)

  uvm_analysis_port #(tvm_packet) item_collected_port;

  tvm_config cfg;

  // Single reusable transaction: every subscriber consumes it
  // synchronously inside write(), so no per-packet allocation
  // is needed on the observation path
  tvm_packet tr;

  protected int          fd        = -1;
  protected stat_counter_t collected = 0;

  function new(string name, uvm_component parent);
    super.new(name, parent);
    item_collected_port = new("item_collected_port", this);
  endfunction

  virtual function void build_phase(uvm_phase phase);
    super.build_phase(phase);

    if (!uvm_config_db#(tvm_config)::get(this, "", "cfg", cfg))
      `uvm_fatal("CFG_FATAL", "tvm_config not found in config_db")

    tr = tvm_packet::type_id::create("tr");

    `uvm_info(get_type_name(),
              $sformatf("Configured %s", cfg.convert2string()),
              UVM_MEDIUM)
  endfunction : build_phase

  virtual task run_phase(uvm_phase phase);
    int            status;
    int            staged;
    stat_counter_t pkt_seq;
    int unsigned   pkt_data;

    phase.raise_objection(this);

    // Initialize IPC connection via DPI
    fd = dpi_shared_memory_open();
    if (fd < 0) begin
      `uvm_fatal ("SHM_OPEN_FATAL",
                  "dpi_shared_memory_open() failed. Server not running?")
      // NB: under Verilator $finish does not preempt a zero-delay loop - exit explicitly
      phase.drop_objection(this);
      return;
    end
    `uvm_info(get_full_name(),
              $sformatf("SHM_OK: shared memory opened, fd=%0d", fd),
              UVM_LOW)

    // Blocking burst reads: the loop only advances when data arrives
    while (collected < cfg.num_transactions) begin
      status = dpi_read_burst(fd, cfg.read_timeout_us, cfg.burst_max, staged);

      // Producer finished and the ring is drained: clean end of stream.
      // The scoreboard verdict (collected vs requested) decides pass/fail.
      if (status == DPI_STREAM_END) begin
        `uvm_info(get_full_name(),
                  $sformatf("STREAM_END: producer finished, collected %0d of %0d packets",
                            collected, cfg.num_transactions),
                  UVM_LOW)
        break;
      end

      if (status != DPI_SUCCESS) begin
        case (status)
          DPI_TIMEOUT: begin
            `uvm_fatal ("READ_TIMEOUT",
                        $sformatf("No packets in %0dus: collected %0d of %0d. Is the server running?",
                                  cfg.read_timeout_us, collected, cfg.num_transactions))
          end

          DPI_PEER_GONE: begin
            `uvm_fatal ("PEER_GONE",
                        $sformatf("Producer process gone: collected %0d of %0d packets",
                                  collected, cfg.num_transactions))
          end

          DPI_ERROR: begin
            `uvm_fatal ("READ_FATAL",
                        "dpi_read_burst() returned DPI_ERROR. Connection lost.")
          end

          default: begin
            `uvm_fatal ("READ_FATAL",
                        $sformatf("Unknown DPI result code: %0d", status))
          end
        endcase

        phase.drop_objection(this);
        return;
      end

      // Publish every staged packet
      for (int i = 0; (i < staged) && (collected < cfg.num_transactions); i++) begin
        status = dpi_burst_packet(i, pkt_seq, pkt_data);
        if (status != DPI_SUCCESS) begin
          `uvm_fatal ("READ_FATAL",
                      $sformatf("dpi_burst_packet(%0d) failed with code %0d", i, status))
          phase.drop_objection(this);
          return;
        end

        tr.seq  = pkt_seq;
        tr.data = pkt_data[15:0];
        item_collected_port.write(tr);
        collected++;
      end
    end

    `uvm_info(get_full_name(),
              $sformatf("DONE: collected %0d packets", collected),
              UVM_MEDIUM)

    phase.drop_objection(this);
  endtask : run_phase

  virtual function void extract_phase(uvm_phase phase);
    int rc;

    super.extract_phase(phase);

    if (fd >= 0) begin
      rc = dpi_shared_memory_close(fd);
      if (rc != 0)
        `uvm_warning ("SHM_CLOSE",
                      $sformatf("close(fd=%0d) returned %0d", fd, rc))
      else
        `uvm_info("SHM_CLEAN",
                  $sformatf("Shared memory fd=%0d closed", fd),
                  UVM_HIGH)
      fd = -1;
    end
  endfunction : extract_phase

  virtual function stat_counter_t get_collected();
    return collected;
  endfunction : get_collected
endclass : tvm_monitor

`endif // TVM_MONITOR_SV
