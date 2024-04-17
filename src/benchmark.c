#include "LYMalloc.h"
#include "benchmark.h"

void benchmark(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *), int num_threads) {
    double start_time, end_time;
    if (strcmp(allocator_name, "LYMalloc") == 0) {
        initMemoryAllocator(num_threads);
    }
    start_time = omp_get_wtime();

    #pragma omp parallel num_threads(num_threads)
    {
        for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
            char *memory = (char *)alloc_func(ALLOC_SIZE);
            memset(memory, 0, ALLOC_SIZE); // Simulate memory usage
            free_func(memory);
        }
    }

    end_time = omp_get_wtime();
    printf("%s - Threads: %d, Time: %f seconds\n", allocator_name, num_threads, end_time - start_time);
    if (strcmp(allocator_name, "LYMalloc") == 0) {
        freeMemoryAllocator(num_threads);
    }
}

void false_sharing_test(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *), int num_threads) {
    double start_time, end_time;
    if (strcmp(allocator_name, "LYMalloc") == 0) {
        initMemoryAllocator(num_threads);
    }
    start_time = omp_get_wtime();
    long *data = (long *)alloc_func(num_threads * CACHE_LINE_SIZE * sizeof(long));
    if (data == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        long *my_data = data + (tid * CACHE_LINE_SIZE / sizeof(long));
        
        for (int i = 0; i < NUM_ALLOCATIONS; i++) {
            #pragma omp atomic
            *my_data += 1;
        }
    }
    free_func(data);

    end_time = omp_get_wtime();
    if (strcmp(allocator_name, "LYMalloc") == 0) {
        freeMemoryAllocator(num_threads);
    }
    printf("%s - Duration without false sharing: %f seconds\n", allocator_name, end_time - start_time);
}

void fragmentation_test(char *allocator_name, void *(*alloc_func)(size_t), void (*free_func)(void *), int num_threads) {
    double start_time, end_time;
    srand(time(NULL)); // Seed the random number generator
    unsigned int seed = time(NULL); // Local seed for each thread

    if (strcmp(allocator_name, "LYMalloc") == 0) {
        initMemoryAllocator(num_threads);
    }

    start_time = omp_get_wtime();

    #pragma omp parallel num_threads(num_threads)
    {
        unsigned int local_seed = seed ^ (omp_get_thread_num() << 16);
        int rand_size;

        for (int i = 0; i < NUM_ALLOCATIONS; i++) {
            rand_size = (rand_r(&local_seed) % 1024) + 16; // Random size between 16 and 1040 bytes
            char *memory = (char *)alloc_func(rand_size);
            memset(memory, 0, rand_size);
            free_func(memory);
        }
    }

    end_time = omp_get_wtime();
    if (strcmp(allocator_name, "LYMalloc") == 0) {
        freeMemoryAllocator(num_threads);
    }
    printf("%s - Total time for %d allocations and deallocations: %f seconds\n", allocator_name, NUM_ALLOCATIONS, end_time - start_time);
}