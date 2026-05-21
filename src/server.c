#include "../hdr/shared_memory.h"
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    char * shm = attach_memory_block(FILENAME, SIZE);
    
    if (shm == NULL) {
        printf("[x]: attach_memory_block error!\n");
        return -1;
    }

    memset(shm, 0, SIZE); // setting all shared memory with zeros
    
    block * b = (block *) shm; // casting to block in order to interact with first bytes of shared memory

    int i = 0;
    
    b -> read = false;
    snprintf(b -> buf, BUF_SIZE - 1, "Hello world: %d!", i);

    while (true) {
        if (b -> read) {
            i++;
            b -> read = false;
            snprintf(b -> buf, BUF_SIZE - 1, "Hello world: %d!", i);
        } else
            sleep(SLEEP_SEC);
    }

    bool res = detach_memory_block(shm);
    if (!res) {
        fprintf(stderr, "[x]: detach_memory_block error!\n");
        return -1;
    }
    return 0;
}