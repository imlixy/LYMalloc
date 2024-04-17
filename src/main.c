#include "LYMalloc.h"
#include "benchmark.h"

int main(int argc, char *argv[]) {
    int num_threads = atoi(argv[1]);
    int c = atoi(argv[2]);

    if (c == 1) {
        benchmark("System malloc", malloc, free, num_threads);
        fragmentation_test("System malloc", malloc, free, num_threads);
        false_sharing_test("System malloc" ,malloc, free, num_threads);
    }

    if (c == 2) {
        benchmark("LYMalloc", LYMalloc, LYFree, num_threads);       
        fragmentation_test("LYMalloc", LYMalloc, LYFree, num_threads);
        false_sharing_test("LYMalloc", LYMalloc, LYFree, num_threads); 
    }

    if (c == 3) {
        benchmark("System malloc", malloc, free, num_threads);
        benchmark("LYMalloc", LYMalloc, LYFree, num_threads); 
        fragmentation_test("System malloc", malloc, free, num_threads);      
        fragmentation_test("LYMalloc", LYMalloc, LYFree, num_threads);
        false_sharing_test("System malloc" ,malloc, free, num_threads);
        false_sharing_test("LYMalloc", LYMalloc, LYFree, num_threads); 
    }

    return 0;
}