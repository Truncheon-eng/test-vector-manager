#include <sys/stat.h>
#include "auxiliary.hpp"

  int shared_memory_open(void) {
    int fd = shm_open(FILENAME, O_CREAT | O_RDWR, 0666);

    if (fd < 0)
        return fd;

    // Guarantee at least PAGE_SIZE bytes: a consumer may open the segment
    // before the server truncates it (mapping a zero-size file => SIGBUS)
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

int shared_memory_unlink(void) {
    // Best effort: ENOENT simply means there is no stale segment
    if (shm_unlink(FILENAME) != 0 && errno != ENOENT) {
        perror("shm_unlink");
        return -1;
    }
    return 0;
}

int shared_memory_truncate(int fd, size_t size) {
    return ftruncate(fd, size);
}

  int close_shared_memory_fd(int fd) {
    return close(fd);
  }
