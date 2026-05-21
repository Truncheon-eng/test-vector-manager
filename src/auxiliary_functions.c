#include "../hdr/shared_memory.h"

bool get_shared_block(int * ptr_id, const char * filename, size_t size) {
    key_t key;

    assert(ptr_id != NULL);

    if ((key = ftok(filename, 0)) == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        return false;
    }

    if ((*ptr_id = shmget(key, size, 0644 | IPC_CREAT)) == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        return false;
    }

    return true;
}

char * attach_memory_block(const char *filename, size_t size) {
    int id;

    char * result;

    if (!get_shared_block(&id, filename, size)) {
        fprintf(stderr, "[x]: get_shared_block error!\n");
        return NULL;
    }

    if ((result = (char *) shmat(id, NULL, 0)) == (char *)-1) {
        fprintf(stderr, "%s\n", strerror(errno));
        return NULL;
    }

    return result;
}

bool detach_memory_block(char * ptr) {
    int result = shmdt(ptr);

    if (result == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        return false;
    }

    return true;
}

bool destroy_memory_block(const char * filename, size_t size) {
    int id;

    if (!get_shared_block(&id, filename, size)) {
        fprintf(stderr, "[x]: get_shared_block error!\n");
        return false;
    }

    int result = shmctl(id, IPC_RMID, NULL);

    if (result == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        return false;
    }

    return true;
}