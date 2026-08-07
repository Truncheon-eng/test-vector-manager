#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <svdpi.h>
#include <stdio.h>
#include <new>

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

#include "server/RingBuffer.hpp"
#include "server/SharedMemoryLock.hpp"

#define FILENAME "a.c"
#define SIZE 0x1000
#define BUF_SIZE 128
#define SLEEP_SEC 3

using RingBufferUint64 = RingBuffer<64, uint32_t>;

enum DpiResult {
    DPI_SUCCESS      = 0,
    DPI_ERROR        = 1,
    DPI_BUFFER_EMPTY = 2,
    DPI_BUFFER_FULL  = 3
};

extern "C" {
    // обертка над системным вызовом shm_open
    int shared_memory_open(void) {
        return shm_open(FILENAME, O_CREAT | O_RDWR, 0666);
    }
    // обертка над системным вызовом ftruncate 
    int shared_memory_truncate(int fd, size_t size) {
        return ftruncate(fd, size);
    }
    // обертка над системным вызовом close
    int close_shared_memory_fd(int fd) {
        return close(fd);
    }
    // получение размера объекта RinBufferUint64 
    size_t get_ring_buffer_size(void) {
        return sizeof(RingBufferUint64);
    }

    // функция получает на вход указатель разделямой памяти и интерпретирует данные,
    // находящиеся по денному указателю, как RingBufferUint64
    //
    //  data_ptr - указатель на разделямую память
    //
    //  Возвращаемое знчкение: указатель на разделяемую память, как указатель на кольцевой буфер
    RingBufferUint64* get_ring_buffer(void* data_ptr) {
        RingBufferUint64* result = static_cast<RingBufferUint64*>(data_ptr);

        if (result->magic != RingBufferUint64::magic_value) {
            result = new (data_ptr) RingBufferUint64();
        }

        return result;
    }

    // функция, которая записывает переданное значение в кольцевой буфер
    //
    // fd - файловый дескриптор, ассоциированный с разделямой памятью
    // value - значение которое необходимо записать в кольцевой буфер
    //
    // Возвращаемое значение: статус записи.
    int write_data(int fd, uint32_t value) {
        if (fd < 0) {
            return DPI_ERROR;
        }
	// RAII обертка для сериализованного доступа к разделямой памяти
        SharedMemoryLock lock(fd);
        if (!lock.owns_lock()) {
            perror("flock");
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
            printf("[C++]: buffer is full\n");
            result = DPI_BUFFER_FULL;
        } else {
            printf("[C++]: successfully wrote 0x%08x\n", value);
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
    
    // функция, которая считывает пзначение из кольцевого буфера по указателю value, переданному в качестве параметра
    //
    // fd - файловый дескриптор, ассоцированный с разделямой памятью
    // value - указатель, по которому будет происходить запись значения из кольцевого буфера.
    //
    // Возвращаемое значение: статус чтения.
    int read_data(int fd, uint32_t* value) {
        if (fd < 0 || value == nullptr) {
            return DPI_ERROR;
        }

	// RAII обертка для сериализованного доступа к разделямой памяти
        SharedMemoryLock lock(fd);
        if (!lock.owns_lock()) {
            perror("flock");
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
            printf("[C++]: buffer is empty\n");
            result = DPI_BUFFER_EMPTY;
        } else {
            printf("[C++]: successfully read 0x%08x\n", *value);
            result = DPI_SUCCESS;
        }

        if (munmap(data_ptr, mapping_size) != 0) {
            perror("munmap");
            result = DPI_ERROR;
        }

        return result;
    }

    // функция, которая оичщает кольцевой буфер
    //
    // fd - файловый дескриптор, ассоциированный с разделфмой памятью
    //
    // Возвращаемое значение: статус очистки кольцевого буфера
    int clear_ring_buffer(int fd) {
        if (fd < 0) {
            return DPI_ERROR;
        }

	// RAII обертка для сериализованного доступа к разделямой памяти
        SharedMemoryLock lock(fd);
        if (!lock.owns_lock()) {
            perror("flock");
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

	// вызов оператора placement new для повторной инициализации кольцевого буфера (т.е. для его очистки)
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
            printf("[C++]: ring buffer cleared\n");

        return result;
    }
}
