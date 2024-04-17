#include "LYMalloc.h"
#include "benchmark.h"

void benchmark(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *), int num_threads, int iteration) {
    double start_time, end_time;
    if (strcmp(allocator_name, "LYMalloc") == 0) {
        initMemoryAllocator(num_threads);
    }
    start_time = omp_get_wtime();

    #pragma omp parallel num_threads(num_threads)
    {
        for (int i = 0; i < iteration; ++i) {
            char *memory = (char *)alloc_func(ALLOC_SIZE);
            memset(memory, 0, ALLOC_SIZE); // Simulate memory usage
            free_func(memory);
        }
    }

    end_time = omp_get_wtime();
    printf("%s : Time: %f seconds\n", allocator_name, end_time - start_time);
    if (strcmp(allocator_name, "LYMalloc") == 0) {
        freeMemoryAllocator(num_threads);
    }
}