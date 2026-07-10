#ifndef SERVER_HPP
#define SERVER_HPP

#include <ios>
#include <iostream>
#include <iomanip>
#include <limits>

#include <assert.h>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include <time.h>

#define FILENAME "a.c"
#define INFO "[i]:"
#define SIZE 0x1000
#define ARR_SIZE 8

void print_menu(void);
int shared_memory_open(void);
int shared_memory_truncate(int fd, size_t size);
int close_shared_memory_fd(int fd);
int write_data(int fd, int size);
int read_data(int fd, int size);

using std::cin;
using std::cout;
using std::endl;

#endif