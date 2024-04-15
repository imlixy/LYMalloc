#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "allocator.h"
#include "benchmark.h"

int main() {
    int num_threads = 1;    // set number of threads
    omp_set_num_threads(num_threads);  // set number of threads of OpenMP

    // initialize allocator
    Allocator* allocator = create_allocator();

    // execute benchmark
    benchmark_Single_Allocation(allocator);
    benchmark_Single_Free(allocator);
    benchmark_Multiple_Allocations(allocator);
    benchmark_Multiple_Free(allocator);
    benchmark_Random_Allocations(allocator);
    benchmark_Random_Free(allocator);

    // destroy allocator
    destroy_allocator(allocator);

    return 0;
}
