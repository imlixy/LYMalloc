#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <string.h> // For memset to simulate usage
#include <time.h>
// #include <jemalloc/jemalloc.h>


#define CACHE_LINE_SIZE 64 // Cache line size
#define NUM_BLOCKS 10

#define ALLOC_SIZE 64

void benchmark(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *), int num_threads, int iteration);

#endif