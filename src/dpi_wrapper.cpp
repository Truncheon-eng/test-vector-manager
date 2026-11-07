#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "auxiliary.hpp"
#include "ctrl_packet.hpp"
#include "packet.hpp"

  std::ostream& operator<<(std::ostream& out, const test_packet_t& packet) {
    out << packet.data;
    return out;
  }

  namespace {
    // Staging buffer for dpi_read_burst / dpi_burst_packet (single-threaded sim)
    std::vector<test_packet_t> burst_buffer;

    // Persistent channels: one mmap per fd, destroyed on dpi_shared_memory_close
    std::unordered_map<int, std::unique_ptr<ShmChannel<SIZE, test_packet_t>>> channels;

    ShmChannel<SIZE, test_packet_t>* get_channel(int fd) {
      if (fd < 0)
        return nullptr;

      auto it = channels.find(fd);
      if (it != channels.end())
        return it->second.get();

      auto channel = std::make_unique<ShmChannel<SIZE, test_packet_t>>(fd);
      if (!channel->valid())
        return nullptr;

      // Register for liveness detection: a blocked producer notices this
      // process' death instead of waiting forever
      channel->ring()->register_consumer(getpid());

      ShmChannel<SIZE, test_packet_t>* raw = channel.get();
      channels.emplace(fd, std::move(channel));
      return raw;
    }

    // Control channel (sim -> producer commands); single connection
    int ctrl_fd = -1;
    std::unique_ptr<ShmChannel<SIZE, ctrl_packet_t>> ctrl_channel;
  }

  extern "C" {
    int dpi_shared_memory_open()
    {
      return shared_memory_open();
    }

    int dpi_shared_memory_truncate(int fd, uint64_t size)
    {
      return shared_memory_truncate(fd, size);
    }

    int dpi_shared_memory_close(int fd)
    {
      channels.erase(fd);  // unmaps the persistent channel
      return close_shared_memory_fd(fd);
    }

    int dpi_read_data(
      int fd,
      uint64_t* seq,
      uint32_t* data
    )
    {
      test_packet_t packet{};

      int rc = read_data<test_packet_t>(fd, &packet);

      if (rc != DPI_SUCCESS)
        return rc;

      *seq  = packet.seq;
      *data = static_cast<uint32_t>(packet.data);

      return DPI_SUCCESS;
    }

    int dpi_write_data(
      int fd,
      uint64_t seq,
      uint32_t data
    )
    {
      test_packet_t packet{};

      packet.seq  = seq;
      packet.data = static_cast<uint16_t>(data);

      return write_data<test_packet_t>(fd, packet);
    }

    // Blocking burst read: waits up to timeout_us of HOST time for at least
    // one packet, then stages up to max_count packets. Returns a DpiResult
    // code; the staged count is written to *count.
    int dpi_read_burst (int fd,
                        int timeout_us,
                        int max_count,
                        int* count)
    {
      if (count == nullptr)
        return DPI_ERROR;

      *count = 0;

      ShmChannel<SIZE, test_packet_t>* channel = get_channel(fd);
      if (channel == nullptr)
        return DPI_ERROR;

      if (max_count <= 0 || max_count > SIZE)
        max_count = SIZE;

      burst_buffer.resize(max_count);

      int staged = 0;
      int rc = read_burst(
        *channel,
        burst_buffer.data(),
        max_count,
        &staged,
        (timeout_us > 0) ? static_cast<uint64_t>(timeout_us) : 0ULL
      );

      if (rc != DPI_SUCCESS) {
        burst_buffer.clear();
        return rc;
      }

      burst_buffer.resize(staged);
      *count = staged;

      return DPI_SUCCESS;
    }

    // Fetch packet idx from the staging buffer filled by dpi_read_burst
    int dpi_burst_packet(
      int idx,
      uint64_t* seq,
      uint32_t* data
    )
    {
      if (idx < 0 || static_cast<size_t>(idx) >= burst_buffer.size())
        return DPI_ERROR;

      *seq  = burst_buffer[static_cast<size_t>(idx)].seq;
      *data = static_cast<uint32_t>(burst_buffer[static_cast<size_t>(idx)].data);

      return DPI_SUCCESS;
    }

    // --------------------------------------------------------
    // Control channel (sim -> producer): service commands
    // --------------------------------------------------------
    int dpi_ctrl_open()
    {
      if (ctrl_fd >= 0)
        return ctrl_fd;

      int fd = shared_memory_open_name(CTRL_FILENAME);
      if (fd < 0)
        return -1;

      auto channel = std::make_unique<ShmChannel<SIZE, ctrl_packet_t>>(fd);
      if (!channel->valid()) {
        close_shared_memory_fd(fd);
        return -1;
      }

      // In the control ring the sim is the producer
      channel->ring()->register_producer(getpid());

      ctrl_fd      = fd;
      ctrl_channel = std::move(channel);
      return ctrl_fd;
    }

    int dpi_ctrl_send(int fd, uint32_t cmd, uint64_t arg)
    {
      if (ctrl_channel == nullptr || fd != ctrl_fd)
        return DPI_ERROR;

      ctrl_packet_t pkt{};
      pkt.cmd = cmd;
      pkt.arg = arg;

      const int w = write_burst(*ctrl_channel, &pkt, 1);

      if (w < 0)
        return DPI_ERROR;
      if (w == 0)
        return DPI_BUFFER_FULL;

      return DPI_SUCCESS;
    }

    int dpi_ctrl_close(int fd)
    {
      if (fd != ctrl_fd)
        return DPI_ERROR;

      ctrl_channel.reset();  // unmaps the persistent channel
      int rc = close_shared_memory_fd(ctrl_fd);
      ctrl_fd = -1;
      return rc;
    }
}
