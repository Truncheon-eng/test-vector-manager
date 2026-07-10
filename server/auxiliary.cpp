#include "server.hpp"


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

int write_data(int fd, int size) {
    if (fd < 0 || size <= 0)
        return EXIT_FAILURE;

    size_t fullsize = static_cast<size_t>(size) * sizeof(int);

    void* data_ptr = mmap(
        nullptr,
        fullsize,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    cout << "\n";

    int * array = static_cast<int *>(data_ptr);
    srand(time(0));

    for(int i = 0; i < size; i++) {
        array[i] = rand();
        cout << INFO << " " << "\"write_data\" - " << "data[" << i << "] = ";        
        cout << "0x" << std::hex << std::uppercase << array[i] << "\n";
    }

    if(msync(data_ptr, fullsize, MS_SYNC) != 0) {
        munmap(data_ptr, fullsize);
        return EXIT_FAILURE;
    }

    if (munmap(data_ptr, fullsize) != 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int read_data(int fd, int size) {
    if (fd < 0 || size <= 0)
        return EXIT_FAILURE;

    size_t fullsize = static_cast<size_t>(size) * sizeof(int);

    void* data_ptr = mmap(
        nullptr,
        fullsize,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    cout << "\n";

    int * array = static_cast<int *>(data_ptr);

    for(int i = 0; i < size; i++) {
        cout << INFO << " " << "\"read_data\" - " << "data[" << i << "] = ";        
        cout << "0x" << std::hex << std::uppercase << array[i] << "\n";
    }

    if (munmap(data_ptr, fullsize) != 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}