#include "loader.h"
#include <stdio.h>    // Added for printf/fprintf/perror
#include <stdlib.h>   // Added for malloc/free/_exit
#include <string.h>   // Added for memset
#include <unistd.h>   // Added for open/close/read/lseek
#include <fcntl.h>    // Added for O_RDONLY
#include <sys/mman.h> // Added for mmap
#include <signal.h>   // Added for sigaction
#include <stdint.h>   // Added for uintptr_t

// Global variables to store ELF information
Elf32_Ehdr *ehdr = NULL;
Elf32_Phdr *phdr = NULL;
int fd = -1;

// Global counters for assignment requirements
int total_page_faults = 0;
int total_page_allocations = 0;
size_t total_internal_fragmentation = 0;

/*
 * release memory and other cleanups
 */
void loader_cleanup() {
  if (fd > 0) {
    close(fd);
  }
  
  if (ehdr) {
    free(ehdr);  
  }

  if (phdr) {
    // Note: We don't munmap here, as the kernel will clean up mmapped regions 
    // upon process exit, but we free the program header array memory.
    free(phdr);  
  }
}

/*
 * Lazy Page Fault Handler (SIGSEGV)
 * This function handles the segmentation fault by treating it as a page fault,
 * mapping the required page, and resuming execution[cite: 36, 37, 40].
 */
void segv_handler(int signum, siginfo_t *info, void *context) {
    // The virtual address that caused the fault
    void *fault_addr = info->si_addr;
    total_page_faults++; // Increment fault counter [cite: 50]
    
    // Align the fault address down to the nearest page boundary (4KB multiple) [cite: 38]
    // This is the actual start address of the page we need to map.
    uintptr_t fault_page_base = (uintptr_t)fault_addr & ~(PAGE_SIZE - 1);

    // 1. Iterate through Program Headers to find the relevant PT_LOAD segment
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;
        
        Elf32_Addr seg_vaddr = phdr[i].p_vaddr;
        size_t seg_memsz = phdr[i].p_memsz;
        
        // Check if the fault address is within this segment's virtual address range
        if (fault_page_base >= seg_vaddr && fault_page_base < (seg_vaddr + seg_memsz)) {

            // We found the segment. Now calculate the page's position within it.
            size_t page_offset_in_segment = fault_page_base - seg_vaddr;
            
            // Check if this page has content from the file (text/data segment) or is BSS.
            if (page_offset_in_segment < phdr[i].p_filesz) {
                // Case A: Page is file-backed (part of p_filesz)
                
                // mmap a single page (4KB) at the exact required virtual address (MAP_FIXED)
                void *page_addr = mmap(
                    (void*)fault_page_base,                  // Required virtual address
                    PAGE_SIZE,                               // Allocate 1 page [cite: 41]
                    PROT_READ | PROT_WRITE | PROT_EXEC,      // Permissions
                    MAP_PRIVATE | MAP_FIXED,                 // MAP_FIXED is essential for lazy loading
                    fd,                                      // File descriptor
                    phdr[i].p_offset + page_offset_in_segment  // Offset in the ELF file
                );

                if (page_addr == MAP_FAILED) {
                    perror("mmap failed for file-backed page");
                    loader_cleanup();
                    _exit(1); 
                }

                // If the page is partially BSS (p_memsz > p_filesz), zero out the remainder.
                if (page_offset_in_segment + PAGE_SIZE > phdr[i].p_filesz) {
                    size_t bss_start_offset = phdr[i].p_filesz - page_offset_in_segment;
                    if (bss_start_offset < PAGE_SIZE) {
                        memset((char*)page_addr + bss_start_offset, 0, PAGE_SIZE - bss_start_offset);
                    }
                }
                
            } else {
                // Case B: Page is pure BSS (part of p_memsz, but beyond p_filesz)
                
                void *page_addr = mmap(
                    (void*)fault_page_base,
                    PAGE_SIZE,
                    PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, // Anonymous: zero-filled for BSS
                    -1, 0
                );
                
                if (page_addr == MAP_FAILED) {
                    perror("mmap failed for BSS page");
                    loader_cleanup();
                    _exit(1);
                }
            }

            total_page_allocations++; // Increment allocation counter [cite: 50]
            
            // Internal Fragmentation Calculation (Only occurs on the last page of a segment) [cite: 50]
            size_t seg_pages_required = (seg_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
            size_t current_page_index = page_offset_in_segment / PAGE_SIZE;

            if (current_page_index == seg_pages_required - 1) {
                // This is the last page, check if it's partially unused (fragmentation)
                size_t last_page_used_size = seg_memsz % PAGE_SIZE;
                if (last_page_used_size != 0) {
                    total_internal_fragmentation += PAGE_SIZE - last_page_used_size;
                }
            }
            
            // Successfully handled: The function returns, and the program execution resumes [cite: 40]
            return;
        }
    }

    // If loop completes without finding a segment, it is a genuine, unhandled SIGSEGV
    fprintf(stderr, "Fatal error: Unhandled Segmentation Fault at address %p. Exiting.\n", fault_addr);
    loader_cleanup();
    _exit(1);
}

/*
 * Load and run the ELF executable file
 */
void load_and_run_elf(char** exe) {
    // 1. Eagerly read Headers (Standard setup)
    fd = open(exe[1], O_RDONLY);  
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }
    
    ehdr = malloc(sizeof(Elf32_Ehdr));
    if (!ehdr || read(fd, ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
        perror("Failed to read ELF header or allocate memory");
        loader_cleanup();
        return;
    }

    phdr = malloc(ehdr->e_phnum * sizeof(Elf32_Phdr));
    lseek(fd, ehdr->e_phoff, SEEK_SET);
    if (!phdr || read(fd, phdr, ehdr->e_phnum * sizeof(Elf32_Phdr)) != ehdr->e_phnum * sizeof(Elf32_Phdr)) {
        perror("Failed to read program header or allocate memory");
        loader_cleanup();
        return;
    }

    // 2. Set up the SIGSEGV handler for lazy loading
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO; // Required to get the faulting address (info->si_addr)
    
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("Failed to set up SIGSEGV handler");
        loader_cleanup();
        return;
    }
    
    // **3. Jump to the entry point (Triggers the first page fault)** [cite: 33, 34]
    // The entry point memory is not mapped, so the kernel will deliver a SIGSEGV, 
    // which our handler will catch and load the page.
    int (*_start)() = (int (*)())ehdr->e_entry;
    
    int result = _start();
    
    // 4. Report results and Cleanup
    printf("\n--- SimpleSmartLoader Report ---\n");
    printf("User _start return value = %d\n", result);
    printf("Total number of page faults = %d\n", total_page_faults);
    printf("Total number of page allocations = %d\n", total_page_allocations);
    
    // --- MODIFIED LINE ---
    // Changed format specifier to %f
    printf("Total internal fragmentation = %f KB\n", (double)total_internal_fragmentation / 1024.0);
    
    printf("------------------------------\n");
    
    loader_cleanup();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ELF file>\n", argv[0]);
        return 1;
    }

    load_and_run_elf(argv);
    return 0;
}