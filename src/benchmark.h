#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <string.h> // For memset to simulate usage
#include <time.h>
// #include <jemalloc/jemalloc.h>


#define BLOCK_SIZE 64 // Cache line size
#define NUM_BLOCKS 10
#define NUM_ITERATIONS 100000

#define NUM_THREADS 4
#define NUM_ALLOCATIONS 1000000
#define ALLOC_SIZE 1024

void benchmark(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *));
void false_sharing_test(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *));
void fragmentation_test(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *));

#endif