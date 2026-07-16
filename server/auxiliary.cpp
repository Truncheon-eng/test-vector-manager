#include "server.hpp"


RingBufferUint64* get_ring_buffer(void * data_ptr) {
    RingBufferUint64* result = static_cast<RingBufferUint64*>(data_ptr);
    if (result -> magic != RingBufferUint64::magic_value) {
        result = new (data_ptr) RingBufferUint64();
    }
    return result;
}

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
    cout << INFO << " " << "2 - exit" << endl;
    return;
}

int write_data(int fd, uint32_t value) {
    int result = EXIT_SUCCESS;

    if (fd < 0)
        return EXIT_FAILURE;

    void* data_ptr = mmap(
        nullptr,
        PAGE_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    RingBufferUint64* ring_buf_p = get_ring_buffer(data_ptr);

    cout << "\n";
    if (!(ring_buf_p -> write_data(value))) {
        cout << INFO << " Sucessfully wrote value \"0x" << 
            std::hex << std::uppercase << value << "\"" << endl;
    } else {
        cout << INFO << " Ring buffer is filled" << endl;
        result = EXIT_FAILURE;
    }

    if(msync(data_ptr, PAGE_SIZE, MS_SYNC) != 0) {
        munmap(data_ptr, PAGE_SIZE);
        return EXIT_FAILURE;
    }

    if (munmap(data_ptr, PAGE_SIZE) != 0)
        return EXIT_FAILURE;

    return result;
}

int read_data(int fd) {
    int result = EXIT_SUCCESS;

    if (fd < 0)
        return EXIT_FAILURE;

    void* data_ptr = mmap(
        nullptr,
        PAGE_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    RingBufferUint64* ring_buf_p = get_ring_buffer(data_ptr);

    cout << "\n";
    uint32_t data;
    if (!(ring_buf_p -> read_data(data))) {
        cout << INFO << " Successfully read value \"0x"
            << std::hex << std::uppercase << data << "\"" << endl; 
    } else {
        cout << INFO << " Ring buffer is empty" << endl;
        result = EXIT_FAILURE;
    }

    if(msync(data_ptr, PAGE_SIZE, MS_SYNC) != 0) {
        munmap(data_ptr, PAGE_SIZE);
        return EXIT_FAILURE;
    }

    if (munmap(data_ptr, PAGE_SIZE) != 0)
        return EXIT_FAILURE;

    return result;
}