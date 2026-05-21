#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define FILENAME "a.c"
#define SIZE 4096

bool get_shared_block(int * ptr_id, const char * filename, size_t size) {
    key_t key;

    assert(ptr_id != NULL);

    if ((key = ftok(filename, 0)) == -1)
        return false;

    if ((*ptr_id = shmget(key, size, 0644 | IPC_CREAT)) == -1)
        return false;

    return true;
}

char * attach_memory_block(const char *filename, size_t size) {
    int id;

    char * result;

    if (!get_shared_block(&id, filename, size)) {
        printf("[x]: get_shared_block error!\n");
        return NULL;
    }

    if ((result = (char *) shmat(id, NULL, 0)) == (char *)-1) {
        printf("[x]: shmat error!\n");
        return NULL;
    }

    return result;
}

bool detach_memory_block(char * ptr) {
    return (shmdt(ptr) != -1);
}

bool destroy_memory_block(const char * filename, size_t size) {
    int id;

    if (!get_shared_block(&id, filename, size)) {
        printf("[x]: get_shared_block error!\n");
        return false;
    }

    return (shmctl(id, IPC_RMID, NULL) != -1);
}

int main(int argc, char *argv[]) {
    char * shm = attach_memory_block(FILENAME, SIZE);
    if (shm == NULL) {
        printf("[x]: attach_memory_block error!\n");
        return -1;
    }

    strcpy(shm, "Hello world!");

    printf("[!]: Successfully wrote to shared memory: %s\n", shm);

    if (!detach_memory_block(shm)) {
        printf("[x]: detach_memory_block error!\n");
        return -1;
    }

    return 0;
}