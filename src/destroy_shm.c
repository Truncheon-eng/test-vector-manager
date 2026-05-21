#include "../hdr/shared_memory.h"

int main(int argc, char *argv[]) {
    bool res = destroy_memory_block(FILENAME, SIZE);

    if (!res) {
        printf("[x]: destroy_memory_block error!\n");
        return -1;
    }
    return 0;
}