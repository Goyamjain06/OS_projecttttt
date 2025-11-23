#ifndef SIMPLE_MULTITHREADER_H
#define SIMPLE_MULTITHREADER_H
#include <iostream>
#include <list>
#include <functional>
#include <stdlib.h>
#include <cstring>
#include <pthread.h>
#include<vector>
#include<time.h>


// parallel_for implementations using Pthreads and C++11 lambdas.

namespace SimpleMultithreaderInternal {

// Time helper: current time in seconds
static inline double now_in_sec() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // fallback: return 0 on error
        perror("clock_gettime");
        return 0.0;
    }
    return ts.tv_sec  + ts.tv_nsec / 1e9;
}

// Helper to compute [start, end) chunk of a range [low, high)
// for thread with index threadIndex out of numThreads.
static inline void calculateThreadRange(int low, int high,int threadIndex, int numThreads,int &outStart, int &outEnd) {
    int total = high - low;
    if (total <= 0 || numThreads <= 0) {
        outStart = outEnd = low;
        return;
    }

    int base = total / numThreads;
    int rem  = total % numThreads;

    int offset = threadIndex * base + (threadIndex < rem ? threadIndex : rem);
    int count  = base + (threadIndex < rem ? 1 : 0);

    outStart = low + offset;
    outEnd   = outStart + count;
}

// 1D parallel_for internal structs / functions 

struct ThreadChunk1D {
    int start;
    int end;
    std::function<void(int)> func;
};

static void *thread_entry_1d(void *arg) {
    ThreadChunk1D *chunk = static_cast<ThreadChunk1D *>(arg);
    for (int i = chunk->start; i < chunk->end; ++i) {
        chunk->func(i);
    }
    return nullptr;
}

// 2D parallel_for internal structs / functions

struct ThreadWork2D {
    int start_i;
    int end_i;
    int low_j;
    int high_j;
    std::function<void(int,int)> func;
};

static void *thread_entry_2d(void *arg) {
    ThreadWork2D *chunk = static_cast<ThreadWork2D *>(arg);
    for (int i = chunk->start_i; i < chunk->end_i; ++i) {
        for (int j = chunk->low_j; j < chunk->high_j; ++j) {
            chunk->func(i, j);
        }
    }
    return nullptr;
}

} // namespace SimpleMultithreaderInternal

//  1D parallel_for
//
// Runs the loop body in parallel over i in [low, high)
// using exactly numThreads threads INCLUDING the caller thread.
//
inline void parallel_for(int low, int high,
                         std::function<void(int)> &&lambda,
                         int numThreads) {
    using namespace SimpleMultithreaderInternal;

    if (numThreads <= 0) {
        std::cerr << "parallel_for(1D): numThreads must be > 0\n";
        return;
    }
    if (high <= low) {
        std::cerr << "parallel_for(1D): empty iteration range\n";
        return;
    }

    double t_start = now_in_sec();


    std::function<void(int)> func = lambda;


    int T = numThreads;
    int workerCount;
    if (T > 1) {
        workerCount = T - 1;
    } else {
        workerCount = 0;
    }


    // One chunk per logical thread (T chunks total).
    std::vector<ThreadChunk1D> chunks(T);
    // pthreads only for worker threads (main thread uses the last chunk).
    std::vector<pthread_t> threads(workerCount);

    // thread range calculation
    for (int t = 0; t < T; ++t) {
        int s, e;
        calculateThreadRange(low, high, t, T, s, e);
        chunks[t].start = s;
        chunks[t].end   = e;
        chunks[t].func  = func;   // each chunk gets a copy of the lambda
    }

    // Create worker threads; main thread will handle the last chunk
    for (int t = 0; t < workerCount; ++t) {
        if (pthread_create(&threads[t], nullptr,thread_entry_1d, &chunks[t]) != 0) {
            perror("pthread_create");

            // If creation fails, do remaining work in this thread.
            for (int k = t; k < T; ++k) {
                for (int i = chunks[k].start; i < chunks[k].end; ++i) {
                    chunks[k].func(i);
                }
            }
            workerCount = t; // only join those that were created
            break;
        }
    }

    // Main thread executes the last chunk (T-1)
    int mainIndex = T - 1;
    for (int i = chunks[mainIndex].start; i < chunks[mainIndex].end; ++i) {
        chunks[mainIndex].func(i);
    }

    // Join worker threads
    for (int t = 0; t < workerCount; ++t) {
        pthread_join(threads[t], nullptr);
    }

    double t_end = now_in_sec();
    std::cout << "[SimpleMultithreader] parallel_for (1D) took "
              << (t_end - t_start) << " seconds using "
              << numThreads << " threads\n";
}

//  2D parallel_for
inline void parallel_for(int low1, int high1,int low2, int high2,std::function<void(int,int)> &&lambda,int numThreads) {
    using namespace SimpleMultithreaderInternal;

    if (numThreads <= 0) {
        std::cerr << "parallel_for(2D): numThreads must be > 0\n";
        return;
    }
    if (high1 <= low1 || high2 <= low2) {
        std::cerr << "parallel_for(2D): empty iteration range\n";
        return;
    }

    double t_start = now_in_sec();

    std::function<void(int,int)> func = lambda;


    int T = numThreads;
    int workerCount;
    if (T > 1) {
        workerCount = T - 1;
    } else {
        workerCount = 0;
    }

    std::vector<ThreadWork2D> chunks(T);
    std::vector<pthread_t> threads(workerCount);

    // Distribute outer loop i across T chunks
    for (int t = 0; t < T; ++t) {
        int i_start, i_end;
        calculateThreadRange(low1, high1, t, T, i_start, i_end);
        chunks[t].start_i = i_start;
        chunks[t].end_i   = i_end;
        chunks[t].low_j   = low2;
        chunks[t].high_j  = high2;
        chunks[t].func    = func;
    }

// Create worker threads (main thread will run the last chunk)
    for (int t = 0; t < workerCount; ++t) {
        if (pthread_create(&threads[t], nullptr,thread_entry_2d, &chunks[t]) != 0) {
            perror("pthread_create");

            // If creation fails, do remaining work in this thread.
            for (int k = t; k < T; ++k) {
                for (int i = chunks[k].start_i; i < chunks[k].end_i; ++i) {
                    for (int j = chunks[k].low_j; j < chunks[k].high_j; ++j) {
                        chunks[k].func(i, j);
                    }
                }
            }
            workerCount = t;
            break;
        }
    }

    // Main thread executes last chunk
    int mainIndex = T - 1;
    for (int i = chunks[mainIndex].start_i; i < chunks[mainIndex].end_i; ++i) {
        for (int j = chunks[mainIndex].low_j; j < chunks[mainIndex].high_j; ++j) {
            chunks[mainIndex].func(i, j);
        }
    }

    // Join worker threads
    for (int t = 0; t < workerCount; ++t) {
        pthread_join(threads[t], nullptr);
    }

    double t_end = now_in_sec();
    std::cout << "[SimpleMultithreader] parallel_for (2D) took "<< (t_end - t_start) << " seconds using "<< numThreads << " threads\n";
}

//  End of SimpleMultithreader impl 





int user_main(int argc, char **argv);

/* Demonstration on how to pass lambda as parameter.
 * "&&" means r-value reference. You may read about it online.
 */
void demonstration(std::function<void()> && lambda) {
  lambda();
}

int main(int argc, char **argv) {
  /* 
   * Declaration of a sample C++ lambda function
   * that captures variable 'x' by value and 'y'
   * by reference. Global variables are by default
   * captured by reference and are not to be supplied
   * in the capture list. Only local variables must be 
   * explicity captured if they are used inside lambda.
   */
  int x=5,y=1;
  // Declaring a lambda expression that accepts void type parameter
  auto /*name*/ lambda1 = /*capture list*/[/*by value*/ x, /*by reference*/ &y](void) {
    /* Any changes to 'x' will throw compilation error as x is captured by value */
    y = 5;
    std::cout<<"====== Welcome to Assignment-"<<y<<" of the CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  // Executing the lambda function
  demonstration(lambda1); // the value of x is still 5, but the value of y is now 5

  int rc = user_main(argc, argv);
 
  auto /*name*/ lambda2 = [/*nothing captured*/]() {
    std::cout<<"====== Hope you enjoyed CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  demonstration(lambda2);
  return rc;
}

#define main user_main

#endif // SIMPLE_MULTITHREADER_H
