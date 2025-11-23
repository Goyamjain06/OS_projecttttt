SimpleSmartLoader (CSE 231 Assignment 4)

This project is a C implementation of a "SimpleSmartLoader" for the CSE 231 Operating System course. Its primary function is to load and execute 32-bit ELF executables using a lazy loading mechanism.

Instead of loading the entire program into memory at once (eager loading), this loader maps individual pages (4KB) on demand, only when they are first accessed.

Core Concept: Lazy Loading via Page Faults

The "smart" part of this loader is how it handles memory. It treats SIGSEGV (Segmentation Fault) signals as Page Faults.

No Upfront Loading: The loader reads the ELF headers but does not map any of the program's segments (like .text or .data) into memory.

Signal Handler: It registers a custom signal handler (segv_handler) for SIGSEGV.

Intentional Fault: The loader then jumps directly to the ELF's entry point address (_start). Since this address is not mapped, the CPU immediately triggers a SIGSEGV.

Fault Handling: The OS kernel passes control to our segv_handler.

Map On-Demand: The handler inspects the faulting address, finds the corresponding PT_LOAD segment from the program headers, and maps only the single 4KB page that caused the fault.

File vs. BSS:

If the page is part of the file (data/text), it's mapped from the file descriptor (fd) using mmap.

If the page is in the BSS segment (uninitialized data), an anonymous, zero-filled page is mapped.

Resume Execution: The signal handler returns. The program now resumes execution from the instruction that failed, which succeeds this time. This process repeats for every new page the program accesses.

Features

Lazy Page Loading: Loads 4KB pages on demand instead of entire segments.

SIGSEGV Handling: Intelligently treats segmentation faults as page faults to trigger loading.

ELF 32-bit Support: Parses 32-bit ELF headers to manage PT_LOAD segments.

BSS Support: Correctly maps and zeroes anonymous memory for .bss segments.

Execution Reporting: After the program finishes, it reports:

Total number of page faults.

Total number of page allocations.

Total internal fragmentation in KB.

How to Build

A Makefile is provided to compile both the loader and the test executables (like fib and sum).

Simply run the make command:

make all


This will compile loader.c into ./loader and also build the 32-bit test executables (e.g., ./fib, ./sum).

How to Run Tests

The Makefile also includes a test target to automatically run the test executables through the loader.

To run the tests, use:

make test


This will execute the loader with the test programs and show the output, including the final statistics report.

Example Output

When you run make test, you will see the output from the test program followed by the loader's report:

(Output from the 'fib' program would appear here)

--- SimpleSmartLoader Report ---
User _start return value = 55
Total number of page faults = 3
Total number of page allocations = 3
Total internal fragmentation = 2.450000 KB
------------------------------