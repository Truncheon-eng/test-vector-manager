#include <cstdio>
#include <cstdlib>
#include <svdpi.h>
#include <stdio.h>


#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>


#define FILENAME "a.c"
#define SIZE 4096
#define BUF_SIZE 128
#define SLEEP_SEC 3


// Описываем структуру так же, как в SV т.е. packed
typedef struct {
    int a;
    int b;
} my_struct_t;

extern "C" {
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

    int write_data(int fd, int write_data) {
        void * data_ptr = mmap(
            NULL,
            sizeof(int),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0
        );

        if (data_ptr == MAP_FAILED)
            return EXIT_FAILURE;

        *((int *) data_ptr) = write_data;
        
        printf("[C++]: Successfully wrote data \"0x%x\" to \"%p\"!\n",
            write_data, data_ptr);
        
        return EXIT_SUCCESS;
    }

    int read_data(int fd, int * read_data) {
        void * data_ptr = mmap(
            NULL,
            sizeof(int),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0
        );

        if (data_ptr == MAP_FAILED)
            return EXIT_FAILURE;

        *read_data = *((int *) data_ptr);

        printf("[C++]: Successfully read data \"0x%x\" from \"%p\"!\n",
            *read_data, data_ptr);

        return EXIT_SUCCESS;
    }
}
