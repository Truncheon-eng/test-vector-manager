#include "server.hpp"
#include <cstdlib>
#include <sys/mman.h>

int shared_memory_open(void) {
    int fd = shm_open(FILENAME, O_CREAT | O_RDWR, 0666);
    return fd;
}

int shared_memory_truncate(int fd, size_t size) {
    return ftruncate(fd, size);
}

int close_shared_memory_fd(int fd) {
    return close(fd);
}

void print_menu(void) {
    cout << endl;
    cout << "====== MENU =======" << endl;
    cout << INFO << " " << "0 - write data" << endl;
    cout << INFO << " " << "1 - read data" << endl;
    cout << INFO << " " << "2 - clear buffer" << endl;
    cout << INFO << " " << "3 - exit" << endl;
    return;
}

RingBufferUint64* get_ring_buffer(void* data_ptr) {
    RingBufferUint64* result = static_cast<RingBufferUint64*>(data_ptr);

    if (result->magic != RingBufferUint64::magic_value) {
        result = new (data_ptr) RingBufferUint64();
    }

    return result;
}

int write_data(int fd, uint32_t value) {
    if (fd < 0) {
        return DPI_ERROR;
    }

    constexpr size_t mapping_size = sizeof(RingBufferUint64);

    void* data_ptr = mmap(
        nullptr,
        mapping_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data_ptr == MAP_FAILED) {
        perror("mmap");
        return DPI_ERROR;
    }

    RingBufferUint64* ring_buffer = get_ring_buffer(data_ptr);
    bool buffer_full = ring_buffer->write_data(value);
    int result;

    if (buffer_full) {
        cout << INFO << " buffer is full" << endl;
        result = DPI_BUFFER_FULL;
    } else {
        cout << INFO << " successfully wrote \"0x" << std::hex << value << "\"" << endl; 
        result = DPI_SUCCESS;
    }

    if (msync(data_ptr, mapping_size, MS_SYNC) != 0) {
        perror("msync");
        result = DPI_ERROR;
    }

    if (munmap(data_ptr, mapping_size) != 0) {
        perror("munmap");
        result = DPI_ERROR;
    }

    return result;
}

int read_data(int fd, uint32_t* value) {
    if (fd < 0 || value == nullptr) {
        return DPI_ERROR;
    }

    constexpr size_t mapping_size = sizeof(RingBufferUint64);

    void* data_ptr = mmap(
        nullptr,
        mapping_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data_ptr == MAP_FAILED) {
        perror("mmap");
        return DPI_ERROR;
    }

    RingBufferUint64* ring_buffer = get_ring_buffer(data_ptr);
    bool buffer_empty = ring_buffer->read_data(*value);
    int result;

    if (buffer_empty) {
        cout << INFO << " buffer is empty" << endl;
        result = DPI_BUFFER_EMPTY;
    } else {
        cout << INFO << " successfully read \"0x" << std::hex << *value << "\"" << endl;
        result = DPI_SUCCESS;
    }

    if (munmap(data_ptr, mapping_size) != 0) {
        perror("munmap");
        result = DPI_ERROR;
    }

    return result;
}

int clear_ring_buffer(int fd) {
    if (fd < 0) {
        return DPI_ERROR;
    }

    constexpr size_t mapping_size = sizeof(RingBufferUint64);

    void* data_ptr = mmap(
        nullptr,
        mapping_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data_ptr == MAP_FAILED) {
        perror("mmap");
        return DPI_ERROR;
    }

    new (data_ptr) RingBufferUint64();
    int result = DPI_SUCCESS;

    if (msync(data_ptr, mapping_size, MS_SYNC) != 0) {
        perror("msync");
        result = DPI_ERROR;
    }

    if (munmap(data_ptr, mapping_size) != 0) {
        perror("munmap");
        result = DPI_ERROR;
    }

    if (result == DPI_SUCCESS)
        cout << INFO << " ring buffer cleared" << endl;
    return result;
}