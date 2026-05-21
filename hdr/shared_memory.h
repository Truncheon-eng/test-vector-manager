#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define FILENAME "a.c"
#define SIZE 4096
#define BUF_SIZE 128
#define SLEEP_SEC 3

bool get_shared_block(int * ptr_id, const char * filename, size_t size);
char * attach_memory_block(const char *filename, size_t size);
bool detach_memory_block(char * ptr);
bool destroy_memory_block(const char * filename, size_t size);


typedef struct {
    bool read;
    char buf[BUF_SIZE];
} block;

#endif