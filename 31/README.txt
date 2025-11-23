# Assignment 5 – SimpleMultithreader

This assignment implements a basic multithreading utility using POSIX Threads (pthreads) and C++11 lambda functions.  
It provides simplified interfaces for executing 1D and 2D loops in parallel.  
Two sample programs demonstrate usage of the utility.

## Files Included

simple-multithreader.h  
Header-only implementation of the parallel_for functions and demonstration main function.

vector.cpp  
Example program using the 1D parallel_for.

matrix.cpp  
Example program using both 1D and 2D parallel_for.

Makefile  
Compiles the programs into executables.

## Features Implemented

1. Support for 1D parallel loops using the interface:  
   parallel_for(int low, int high, std::function<void(int)>&& lambda, int numThreads)

2. Support for 2D parallel loops using the interface:  
   parallel_for(int low1, int high1, int low2, int high2, std::function<void(int,int)>&& lambda, int numThreads)


3. Uses C++11 lambda expressions for passing loop bodies  

4. The total number of running threads equals numThreads (main thread plus worker threads)

5. Threads are created inside each parallel_for call (no thread pool), as required

6. Execution time printing for each parallel_for call using clock_gettime(CLOCK_MONOTONIC)

7. Workload is divided among threads using a range-splitting helper

8. The demonstration main function shows welcome and exit messages automatically

## Requirements

Linux or WSL  
g++ compiler with C++11 support  

Expected output includes a welcome message, timing results for parallel execution, and an exit message.

Work is distributed evenly across threads using a range calculation helper function.  
Worker threads handle a portion of iterations and the main thread executes the final chunk.  
The 2D parallel_for splits only the outer loop among threads while the inner loop is executed sequentially.  
Timing allows performance comparison across different thread counts.

##Student Details
Name - Goyam Jain
Roll num - 2024224

Name - Shashank Verma
Roll num - 2024522
