#ifndef LOADER_H
#define LOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <signal.h>
#include <elf.h>
#include <stdint.h> // For uintptr_t

// Define page size (4KB required) [cite: 38]
#define PAGE_SIZE 0x1000 // 4096 bytes

// Global counters for assignment requirements [cite: 50]
extern int total_page_faults;
extern int total_page_allocations;
extern size_t total_internal_fragmentation;

// Function declarations
void loader_cleanup();
void load_and_run_elf(char** exe);
void segv_handler(int signum, siginfo_t *info, void *context);

#endif // LOADER_H