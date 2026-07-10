#include <cstddef>
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
        if (fd < 0)
            return EXIT_FAILURE;

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

        *(static_cast<int *>(data_ptr)) = write_data;

        if(msync(data_ptr, sizeof(int), MS_SYNC) != 0) {
            munmap(data_ptr, sizeof(int));
            return EXIT_FAILURE;
        }
        
        if(munmap(data_ptr, sizeof(int)) != 0)
            return EXIT_FAILURE;
        
        printf("[C++]: Successfully wrote data \"0x%x\" to \"%p\"!\n",
            write_data, data_ptr);
        
        return EXIT_SUCCESS;
    }

    int read_data(int fd, int * read_data) {
        if (fd < 0)
            return EXIT_FAILURE;

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

        *read_data = *(static_cast<int *>(data_ptr));

        if(munmap(data_ptr, sizeof(int)) != 0)
            return EXIT_FAILURE;

        printf("[C++]: Successfully read data \"0x%x\" from \"%p\"!\n",
            *read_data, data_ptr);

        return EXIT_SUCCESS;
    }

    int write_array(int fd, const svOpenArrayHandle data, int size) {
        if (fd < 0 || data == nullptr || size <= 0)
            return EXIT_FAILURE;

        size_t full_size = static_cast<std::size_t>(size) * sizeof(int);

        void * data_ptr = mmap(
            nullptr,
            full_size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0
        );

        if (data_ptr == MAP_FAILED)
            return EXIT_FAILURE;

        int* array = static_cast<int *>(data_ptr);
        for (int i = 0; i < size; i++){
            int* source_element = static_cast<int *>(svGetArrElemPtr1(data, i));
            if (source_element == nullptr) {
                munmap(data_ptr, full_size);
                return EXIT_FAILURE;
            }

            array[i] = *source_element;
            printf("[C++]: array[%d] = 0x%x\n", i, *source_element);
        }

        if (msync(data_ptr, full_size, MS_SYNC) != 0) {
            munmap(data_ptr, full_size);
            return EXIT_FAILURE;
        }

        if (munmap(data_ptr, full_size) != 0) {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    int read_array(int fd, const svOpenArrayHandle data, int size) {
        if (fd < 0 || data == nullptr || size <= 0)
            return EXIT_FAILURE;

        size_t full_size = static_cast<std::size_t>(size) * sizeof(int);

        void * data_ptr = mmap(
            nullptr,
            full_size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0
        );

        if (data_ptr == MAP_FAILED)
            return EXIT_FAILURE;

        int* array = static_cast<int *>(data_ptr);
        for (int i = 0; i < size; i++){
            int* destination_element = static_cast<int *>(svGetArrElemPtr1(data, i));
            if (destination_element == nullptr) {
                munmap(data_ptr, full_size);
                return EXIT_FAILURE;
            }
            *destination_element = array[i];
            printf("[C++]: array[%d] = 0x%x\n", i, *destination_element);
        }

        if (msync(data_ptr, full_size, MS_SYNC) != 0) {
            munmap(data_ptr, full_size);
            return EXIT_FAILURE;
        }

        if (munmap(data_ptr, full_size) != 0) {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
}
