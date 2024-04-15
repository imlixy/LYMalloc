#include <stdio.h>
#include <stdlib.h>
#include "LYMalloc.h"
#include <omp.h>

#define ALLOCATE_SIZE 16384 * 1024  // Adjust size to increase the workload
#define NUM_ALLOCATIONS 1000        // Increase the number of allocations

int num_threads;
void benchmark_standard_allocator() {
    double start_time = omp_get_wtime();
    #pragma omp parallel for num_threads(num_threads)
    for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
        char* block = malloc(ALLOCATE_SIZE);
        for (size_t j = 0; j < ALLOCATE_SIZE; j += 1024) {
            block[j] = 'a';
        }
        free(block);
    }
    double end_time = omp_get_wtime();
    printf("Standard Allocator Time: %f seconds\n", end_time - start_time);
}

void benchmark_custom_allocator() {
    initMemoryAllocator(num_threads);
    double start_time = omp_get_wtime();
    #pragma omp parallel for num_threads(num_threads)
    for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
        char* block = (char*)customMalloc(ALLOCATE_SIZE);
        for (size_t j = 0; j < ALLOCATE_SIZE; j += 1024) {
            block[j] = 'a';
        }
        customFree(block);
    }
    double end_time = omp_get_wtime();
    printf("Custom Allocator Time: %f seconds\n", end_time - start_time);
}

int main(int argc, char * argv[]) {
    num_threads = atoi(argv[1]);
    int allocator_type = atoi(argv[2]); // 0 for standard, 1 for custom

    if (allocator_type == 0) {
        printf("Benchmarking 'malloc'\n");
        benchmark_standard_allocator();
    }
    else {
        printf("Benchmarking 'LYallocator'\n");
        benchmark_custom_allocator();
    }

    return 0;
}
