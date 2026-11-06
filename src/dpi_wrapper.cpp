#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "auxiliary.hpp"
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

      ShmChannel<SIZE, test_packet_t>* raw = channel.get();
      channels.emplace(fd, std::move(channel));
      return raw;
    }
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
  }
