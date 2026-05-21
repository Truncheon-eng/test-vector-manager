#include "../hdr/shared_memory.h"


int main(int argc, char *argv[]) {
    char * shm = attach_memory_block(FILENAME, SIZE);
    
    if (shm == NULL) {
        printf("[x]: attach_memory_block error!\n");
        return -1;
    }
    
    block * b = (block *) shm;
    if (b -> read == false) {
        printf("Read: \"%s\"\n", b -> buf);
    } else {
        printf("[!]: Nothing to read!\n");
    }
    
    b -> read = true;

    bool res = detach_memory_block(shm);
    if (!res) {
        fprintf(stderr, "[x]: detach_memory_block error!\n");
        return -1;
    }

    return 0;
}