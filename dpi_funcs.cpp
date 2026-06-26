// #include <cstdint>
#include <cstdio>
#include <svdpi.h>
#include <stdio.h>


#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

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
    static bool create_shm_file(int * fd) {
        if ((*fd = open(FILENAME, O_CREAT | O_WRONLY, 0644)) == -1) {
            fprintf(stderr, "[x]: %s\n", strerror(errno));
            return false;
        }
        return true;
    }

    static bool init_shm_file(void) {
        int fd {};
        bool result = create_shm_file(&fd);
        if (!result) {
            fprintf(stderr, "[x]: \"init_shm_file\" error at \"create_shm_file\"\n");
            return false;
        }
        close(fd);
        return true;
    }

    static bool get_shared_block(int * ptr_id, const char * filename, size_t size) {
        key_t key;

        assert(ptr_id != NULL);

        if ((key = ftok(filename, 0)) == -1) {
            fprintf(stderr, "[x]: %s\n", strerror(errno));
            return false;
        }

        if ((*ptr_id = shmget(key, size, 0644 | IPC_CREAT)) == -1) {
            fprintf(stderr, "[x]: %s\n", strerror(errno));
            return false;
        }

        return true;
    }

    static char * attach_memory_block(const char *filename, size_t size) {
        int id;

        char * result;

        if (!get_shared_block(&id, filename, size)) {
            fprintf(stderr, "[x]: get_shared_block error!\n");
            return NULL;
        }

        if ((result = (char *) shmat(id, NULL, 0)) == (char *)-1) {
            fprintf(stderr, "[x]: %s\n", strerror(errno));
            return NULL;
        }

        return result;
    }

    static bool detach_memory_block(char * ptr) {
        int result = shmdt(ptr);

        if (result == -1) {
            fprintf(stderr, "[x]: %s\n", strerror(errno));
            return false;
        }

        return true;
    }

    static bool destroy_memory_block(const char * filename, size_t size) {
        int id;

        if (!get_shared_block(&id, filename, size)) {
            fprintf(stderr, "[x]: get_shared_block error!\n");
            return false;
        }

        int result = shmctl(id, IPC_RMID, NULL);

        if (result == -1) {
            fprintf(stderr, "[x]: %s\n", strerror(errno));
            return false;
        }

        return true;
    }

    int read_data(int * data_read) {
        bool result = init_shm_file();
        if (!result) {
            fprintf(stderr,"[x]: \"read_data\" error at \"init_shm_file\"\n");
            return -1;
        }

        char * memory_ptr = attach_memory_block(FILENAME, SIZE);
        if (!memory_ptr) {
            fprintf(stderr,"[x]: \"read_data\" error at \"attach_memory_block\"\n");
            return -1;
        }

        fprintf(stdout, "[C++]: number at memory - %08x\n", *(int *)memory_ptr);

        *data_read = *((int *) memory_ptr); // writing to the memory
        
        result = detach_memory_block(memory_ptr);
        if(!result) {
            fprintf(stderr,"[x]: \"read_data\" error at \"detach_memory_block\"\n");
            return -1;
        }

        return 0;
    }

    int write_data(int * data_write) {
        bool result = init_shm_file();
        if (!result) {
            fprintf(stderr,"[x]: \"write_data\" error at \"init_shm_file\"\n");
            return -1;
        }

        char * memory_ptr = attach_memory_block(FILENAME, SIZE);
        if (!memory_ptr) {
            fprintf(stderr,"[x]: \"write_data\" error at \"attach_memory_block\"\n");
            return -1;
        }

        *((int *) memory_ptr) = *data_write;

        fprintf(stdout, "[C++]: number at memory - %08x\n", *(int *)memory_ptr);

        result = detach_memory_block(memory_ptr);
        if(!result) {
            fprintf(stderr,"[x]: \"write_data\" error at \"detach_memory_block\"\n");
            return -1;
        }

        return 0;
    }
}
