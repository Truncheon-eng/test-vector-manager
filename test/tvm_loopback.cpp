// Single-process loopback self-test for the TVM IPC layer.
// Covers: burst write/read through ring wrap, capacity cap, legacy
// single-packet path, timeout, stream EOF and dead-producer detection.
// No UVM/Verilator involved - plain C++17, runs in well under a second.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "auxiliary.hpp"
#include "ctrl_packet.hpp"
#include "lfsr.hpp"
#include "packet.hpp"

namespace {

  int failures = 0;

#define EXPECT(cond, name)                                     \
  do {                                                         \
    if (cond) {                                                \
      printf("[ OK ] %s\n", name);                             \
    } else {                                                   \
      printf("[FAIL] %s (%s:%d)\n", name, __FILE__, __LINE__); \
      failures++;                                              \
    }                                                          \
  } while (0)

  int fresh_fd() {
    if (shared_memory_unlink() != 0)
      return -1;

    int fd = shared_memory_open();
    if (fd < 0)
      return fd;

    if (shared_memory_truncate(fd, PAGE_SIZE) != 0) {
      close_shared_memory_fd(fd);
      return -1;
    }
    return fd;
  }

}  // namespace

int main() {
  // ----------------------------------------------------------
  // 1. Burst loopback with LFSR payload check (wraps the ring)
  // ----------------------------------------------------------
  {
    constexpr int N     = 5000;  // > SIZE to force wrap and full handling
    constexpr int BATCH = 256;

    int fd = fresh_fd();
    EXPECT(fd >= 0, "open fresh segment");

    ShmChannel<SIZE, test_packet_t> channel(fd);
    EXPECT(channel.valid(), "channel mapping valid");
    channel.ring()->register_producer(getpid());
    channel.ring()->register_consumer(getpid());

    std::vector<test_packet_t> stage(BATCH);
    std::vector<test_packet_t> out(BATCH);

    uint16_t wstate = LFSR_SEED;
    uint16_t rstate = LFSR_SEED;
    uint64_t wseq = 0;
    uint64_t rseq = 0;
    bool seq_ok = true;
    bool payload_ok = true;

    while (rseq < static_cast<uint64_t>(N)) {
      // Produce: fill whatever space is available
      if (wseq < static_cast<uint64_t>(N)) {
        const int n = static_cast<int>(
            std::min<uint64_t>(BATCH, static_cast<uint64_t>(N) - wseq));

        for (int j = 0; j < n; j++) {
          stage[j].seq  = wseq + static_cast<uint64_t>(j);
          stage[j].data = lfsr_next(wstate);
        }

        const int w = write_burst(channel, stage.data(), n);
        if (w < 0) {
          payload_ok = false;
          break;
        }
        wseq += static_cast<uint64_t>(w);
      }

      // Consume everything available
      int cnt = 0;
      const int rc = read_burst(channel, out.data(), BATCH, &cnt, 1000000);
      if (rc != DPI_SUCCESS) {
        payload_ok = false;
        break;
      }

      for (int j = 0; j < cnt; j++) {
        const uint16_t expect = lfsr_next(rstate);
        if (out[j].seq != rseq)
          seq_ok = false;
        if (out[j].data != expect)
          payload_ok = false;
        rseq++;
      }
    }

    EXPECT(wseq == static_cast<uint64_t>(N) && rseq == static_cast<uint64_t>(N),
           "5000 packets through 1024-slot ring");
    EXPECT(seq_ok, "sequence numbers intact");
    EXPECT(payload_ok, "LFSR payloads intact");

    close_shared_memory_fd(fd);
  }

  // ----------------------------------------------------------
  // 2. write_burst caps at ring capacity (partial write)
  // ----------------------------------------------------------
  {
    int fd = fresh_fd();
    ShmChannel<SIZE, test_packet_t> channel(fd);

    std::vector<test_packet_t> big(SIZE * 2);
    for (size_t i = 0; i < big.size(); i++) {
      big[i].seq  = i;
      big[i].data = 0xBEEF;
    }

    const int w = write_burst(channel, big.data(), SIZE * 2);
    EXPECT(w == SIZE, "write_burst caps at ring capacity");

    close_shared_memory_fd(fd);
  }

  // ----------------------------------------------------------
  // 3. Legacy single-packet path (flock-based)
  // ----------------------------------------------------------
  {
    int fd = fresh_fd();
    ShmChannel<SIZE, test_packet_t> channel(fd);  // initializes the ring

    test_packet_t p{};
    p.seq  = 42;
    p.data = 0xABCD;

    EXPECT(write_data(fd, p) == DPI_SUCCESS, "legacy write_data");

    test_packet_t q{};
    EXPECT(read_data(fd, &q) == DPI_SUCCESS && q.seq == 42 && q.data == 0xABCD,
           "legacy read_data roundtrip");

    EXPECT(read_data(fd, &q) == DPI_BUFFER_EMPTY,
           "legacy read_data on empty ring -> BUFFER_EMPTY");

    close_shared_memory_fd(fd);
  }

  // ----------------------------------------------------------
  // 4/5. Timeout on empty ring, then graceful EOF via stream_done
  // ----------------------------------------------------------
  {
    int fd = fresh_fd();
    ShmChannel<SIZE, test_packet_t> channel(fd);

    std::vector<test_packet_t> out(16);
    int cnt = -1;

    const uint64_t t0 = monotonic_us();
    int rc = read_burst(channel, out.data(), 16, &cnt, 100000);  // 0.1 s
    const uint64_t dt = monotonic_us() - t0;

    EXPECT(rc == DPI_TIMEOUT, "empty ring -> DPI_TIMEOUT");
    EXPECT(dt >= 90000 && dt < 1000000, "timeout honored (~0.1 s)");

    channel.ring()->mark_stream_done();
    rc = read_burst(channel, out.data(), 16, &cnt, 1000000);
    EXPECT(rc == DPI_STREAM_END, "stream_done -> DPI_STREAM_END");

    close_shared_memory_fd(fd);
  }

  // ----------------------------------------------------------
  // 6. Dead producer -> DPI_PEER_GONE (fast, via liveness tick)
  // ----------------------------------------------------------
  {
    pid_t child = fork();
    if (child == 0)
      _exit(0);

    int wstatus = 0;
    waitpid(child, &wstatus, 0);  // child is dead and reaped now

    int fd = fresh_fd();
    ShmChannel<SIZE, test_packet_t> channel(fd);
    channel.ring()->register_producer(child);  // dead pid

    std::vector<test_packet_t> out(16);
    int cnt = -1;

    const uint64_t t0 = monotonic_us();
    const int rc = read_burst(channel, out.data(), 16, &cnt, 5000000);
    const uint64_t dt = monotonic_us() - t0;

    EXPECT(rc == DPI_PEER_GONE, "dead producer -> DPI_PEER_GONE");
    EXPECT(dt < 1000000, "peer death detected within ~1 tick (<1 s)");

    close_shared_memory_fd(fd);
  }

  // ----------------------------------------------------------
  // 7. Control channel roundtrip (sim -> producer commands)
  // ----------------------------------------------------------
  {
    EXPECT(shared_memory_unlink_name(CTRL_FILENAME) == 0, "ctrl: unlink stale");

    int fd = shared_memory_open_name(CTRL_FILENAME);
    EXPECT(fd >= 0, "ctrl: open");
    EXPECT(shared_memory_truncate(fd, PAGE_SIZE) == 0, "ctrl: truncate");

    ShmChannel<SIZE, ctrl_packet_t> ctrl(fd);
    EXPECT(ctrl.valid(), "ctrl: channel valid");

    ctrl_packet_t cmds[3];
    cmds[0] = {CTRL_LOAD_VECTORS, 0, 2};
    cmds[1] = {CTRL_SET_RATE, 0, 50000};
    cmds[2] = {CTRL_START_STREAM, 0, 1000};

    EXPECT(write_burst(ctrl, cmds, 3) == 3, "ctrl: write 3 commands");

    ctrl_packet_t got[3];
    int cnt = 0;
    int rc = read_burst(ctrl, got, 3, &cnt, 1000000);

    EXPECT(rc == DPI_SUCCESS && cnt == 3, "ctrl: read back 3 commands");
    EXPECT(got[0].cmd == CTRL_LOAD_VECTORS && got[0].arg == 2,
           "ctrl: LOAD_VECTORS intact");
    EXPECT(got[1].cmd == CTRL_SET_RATE && got[1].arg == 50000,
           "ctrl: SET_RATE intact");
    EXPECT(got[2].cmd == CTRL_START_STREAM && got[2].arg == 1000,
           "ctrl: START_STREAM intact");

    // Non-blocking drain on empty control ring
    rc = read_burst(ctrl, got, 3, &cnt, 0);
    EXPECT(rc == DPI_TIMEOUT, "ctrl: empty non-blocking -> DPI_TIMEOUT");

    // Seed table consistency (mirror of tvm_refmodel.sv)
    EXPECT(vector_set_seed(0) == 0xACE1 && vector_set_seed(1) == 0xBEEF &&
           vector_set_seed(2) == 0xCAFE && vector_set_seed(3) == 0xF00D &&
           vector_set_seed(-1) == 0xACE1,
           "ctrl: vector set seed table");

    close_shared_memory_fd(fd);
    shared_memory_unlink_name(CTRL_FILENAME);
  }

  shared_memory_unlink();

  printf(failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n",
         failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
