#include "LYMalloc.h"
#include "benchmark.h"

void benchmark(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *)) {
    double start_time, end_time;
    omp_set_num_threads(NUM_THREADS);
    start_time = omp_get_wtime();

    #pragma omp parallel
    {
        for (int i = 0; i < NUM_ALLOCATIONS; i++) {
            char *memory = (char *)alloc_func(ALLOC_SIZE);
            memset(memory, 0, ALLOC_SIZE); // Simulate memory usage
            free_func(memory);
        }
    }

    end_time = omp_get_wtime();
    printf("%s - Threads: %d, Time: %f seconds\n", allocator_name, NUM_THREADS, end_time - start_time);
}

void false_sharing_test(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *)) {
    double start_time, end_time;
    start_time = omp_get_wtime();
    long *data = (long *)alloc_func(NUM_THREADS * CACHE_LINE_SIZE * sizeof(long));
    
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        int tid = omp_get_thread_num();
        long *my_data = data + (tid * CACHE_LINE_SIZE / sizeof(long));
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            *my_data += 1;
        }
    }
    free_func(data);

    end_time = omp_get_wtime();
    printf("%s - Duration without false sharing: %f seconds\n", allocator_name, end_time - start_time);
}

void fragmentation_test(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *)) {
    double start_time, end_time;
    srand(time(NULL)); // Initialization, should only be called once.
    int rand_size;

    omp_set_num_threads(NUM_THREADS);
    start_time = omp_get_wtime();

    #pragma omp parallel
    {
        for (int i = 0; i < NUM_ALLOCATIONS; i++) {
            rand_size = (rand() % 1024) + 16; // Random size between 16 and 1040 bytes
            char *memory = (char *)alloc_func(rand_size);
            memset(memory, 0, rand_size);
            free_func(memory);
        }
    }

    end_time = omp_get_wtime();
    printf("%s - Total time for %d allocations and deallocations: %f seconds\n", allocator_name, NUM_THREADS, end_time - start_time);
}