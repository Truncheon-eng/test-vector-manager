#include <sys/stat.h>
#include "auxiliary.hpp"

  int shared_memory_open_name(const char* name) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);

    if (fd < 0)
        return fd;

    // Guarantee at least PAGE_SIZE bytes: a consumer may open the segment
    // before the producer truncates it (mapping a zero-size file => SIGBUS)
    struct stat st;
    if (fstat(fd, &st) == 0 && st.st_size < PAGE_SIZE) {
      if (ftruncate(fd, PAGE_SIZE) != 0) {
        perror("ftruncate");
        close(fd);
        return -1;
      }
    }

    return fd;
  }

  int shared_memory_open(void) {
    return shared_memory_open_name(FILENAME);
  }

  int shared_memory_unlink_name(const char* name) {
    // Best effort: ENOENT simply means there is no stale segment
    if (shm_unlink(name) != 0 && errno != ENOENT) {
      perror("shm_unlink");
      return -1;
    }
    return 0;
  }

  int shared_memory_unlink(void) {
    return shared_memory_unlink_name(FILENAME);
  }

  int shared_memory_truncate(int fd, size_t size) {
    return ftruncate(fd, size);
  }

  int close_shared_memory_fd(int fd) {
    return close(fd);
  }
