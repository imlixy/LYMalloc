#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include "allocator.h" 
#include "benchmark.h"

size_t num_allocs;
size_t *sizes;
size_t *alignments;

void benchmark_Single_Allocation(Allocator* allocator, size_t size, size_t alignment) {
    printf("BENCHMARK: Single Allocation\n");
    printf("\tSize: \t%zu\n", size);
    printf("\tAlignment: \t%zu\n", alignment);

    // start timer
    double start = omp_get_wtime();

    // perform allocation
    void *ptr = allocator->Allocator(allocator, size, alignment);
        if (ptr == NULL) {
            fprintf(stderr, "Allocation failed\n");
            return;
        }
    
    // check alignment
    if ((uintptr_t)ptr % alignment != 0) {
        fprintf(stderr, "Incorrect alignment\n");
    }

    // end timer
    double end = omp_get_wtime();

    // calculate elapsed time in milliseconds
    double elapsed = (end - start) / * 1000.0;

    // print time
    printf("Allocation Time: %.2f ms\n", elapsed);

    // free memory
    allocator->Free(allocator, ptr);

}

void benchmark_Single_Free(Allocator* allocator, size_t size, size_t alignment) {
    printf("BENCHMARK: Single Free\n");
    printf("\tSize: \t%zu\n", size);
    printf("\tAlignment: \t%zu\n", alignment);

    // perform allocation
    void *ptr = allocator->Allocator(allocator, size, alignment);
    if (ptr == NULL) {
        fprintf(stderr, "Allocation failed for free\n");
        return;
    }

    // start timer
    double start = omp_get_wtime();

    // execute free
    allocator->Free(allocator, ptr);

    // end timer
    double end = omp_get_wtime();

    // calculate elapsed time
    double elapsed = (end - start) * 1000.0; 

    // print time
    printf("Free time: %.2f ms\n", elapsed);
}

void benchmark_Multiple_Allocation(Allocator* allocator) {
    printf("BENCHMARK: Multiple Allocation\n");
    printf("Performing %zu allocations.\n", num_allocs);

    // initialize allocator
    allocator->init(allocator);

    // start timer
    double start = omp_get_wtime();

    // perform multiple allocations in parallel using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < num_allocs; i++) {
        void* ptr = allocator->Allocate(allocator, sizes[i], alignments[i]);
        if (ptr == NULL) {
            #pragma omp critical
            fprintf(stderr, "Allocation failed for size %zu and alignment %zu.\n", sizes[i], alignments[i]);
        }
    }

    // wait for all threads to finish
    #pragma omp barrier

    // end timer
     double end = omp_get_wtime();

    // calculate elapsed time
    double elapsed = (end - start) * 1000.0;

    // print time
    printf("Total allocation time: %.2f ms for %zu operations.\n", elapsed, num_allocs);
}

void benchmark_Multiple_Free(Allocator* allocator) {
    printf("BENCHMARK: Multiple Free\n");
    printf("Performing %zu frees.\n", num_allocs);

    // initialize allocator
    allocator->init(allocator);

    // allocate memory for later free
    void** pointers = malloc(num_allocs * sizeof(void*));
    if (pointers == NULL) {
        fprintf(stderr, "Failed to allocate memory for pointers.\n");
        return;
    }

    // perform multiple allocations
    for (size_t i = 0; i < num_allocs; i++) {
        pointers[i] = allocator->Allocate(allocator, sizes[i], alignments[i]);
        if (pointers[i] == NULL) {
            fprintf(stderr, "Allocation failed for size %zu and alignment %zu.\n", sizes[i], alignments[i]);
        }
    }

    // start timer
   double start = omp_get_wtime();

    // perform multiple frees in parallel using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < num_allocs; i++) {
        if (pointers[i] != NULL) {
            allocator->Free(allocator, pointers[i]);
        }
    }

    // end timer
    double end = omp_get_wtime();

    // calculate elapsed time
    double elapsed = (end - start) * 1000.0;

    // print time
    printf("Total free time: %.2f ms for %zu operations.\n", elapsed, num_allocs);

    // free memory
    free(pointers);
}

void benchmark_Random_Allocation(Allocator* allocator) {
    printf("BENCHMARK: Random Allocation\n");

    // initialize random seed
    srand((unsigned)time(NULL));

    // initialize allocator
    allocator->init(allocator);

    // allocate memory for pointers
    void** pointers = malloc(num_allocs * sizeof(void*));
    if (pointers == NULL) {
        fprintf(stderr, "Failed to allocate memory for pointers.\n");
        return;
    }

    // start timer
    double start = omp_get_wtime();

    // perform random allocations in parallel using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < num_allocs; i++) {

        // ensure different seeds in different threads
        int thread_id = omp_get_thread_num();
        unsigned int seed = time(NULL) ^ (thread_id + 1); // Ensure different seeds in different threads

        // randomly select size and alignment
        size_t index = rand_r(&seed) % num_allocs;
        size_t size = sizes[index];
        size_t alignment = alignments[index];

        // perform allocation
        void* ptr = allocator->Allocate(allocator, size, alignment);
        if (ptr == NULL) {
            # pragma omp critical
            fprintf(stderr, "Allocation failed for size %zu and alignment %zu at index %zu.\n", size, alignment, index);
        } else {
            pointers[i] = ptr;
        }
    }

    // end timer
    double end = omp_get_wtime();

    // calculate elapsed time
    double elapsed = (end - start) * 1000.0;

    // print time
    printf("Total random allocation time: %.2f ms for %zu operations.\n", elapsed, num_allocs);

    // free memory
    for (size_t i = 0; i < num_allocs; i++) {
        if (pointers[i] != NULL) {
            allocator->Free(allocator, pointers[i]);
        }
    }

    // clean up
    free(pointers);
}

void benchmark_Random_Free(Allocator* allocator) {
    printf("BENCHMARK: Random Free\n");

    // initialize random seed
    srand((unsigned)time(NULL));

    // initialize allocator
    allocator->init(allocator);

    // execute random free operations and store pointers
    void** pointers = malloc(num_allocs * sizeof(void*));
    if (pointers == NULL) {
        fprintf(stderr, "Failed to allocate memory for pointers.\n");
        return;
    }

    // perform random allocations
    for (size_t i = 0; i < num_allocs; i++) {
        pointers[i] = allocator->Allocate(allocator, sizes[i], alignments[i]);
        if (pointers[i] == NULL) {
            fprintf(stderr, "Allocation failed for size %zu and alignment %zu.\n", sizes[i], alignments[i]);
            free(pointers);
            return;
        }
    }

    // start timer
     double start = omp_get_wtime();

    // perform random frees in parallel using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < num_allocs; i++) {
        // ensure different seeds in different threads
        int thread_id = omp_get_thread_num();
        unsigned int seed = time(NULL) ^ (thread_id + 1); 
        
        size_t index = rand() % num_allocs; 
        if (pointers[index] != NULL) {
            allocator->Free(allocator, pointers[index]);
            pointers[index] = NULL;
        }
    }

    // end timer
     double end = omp_get_wtime();

    // calculate elapsed time
    double elapsed = (end - start) * 1000.0;

    // print time
    printf("Total random free time: %.2f ms for %zu operations.\n", elapsed, num_allocs);

    // free memory
    free(pointers);
}


