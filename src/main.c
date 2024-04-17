#include "LYMalloc.h"
#include "benchmark.h"

int main(int argc, char *argv[]) {
    int c = atoi(argv[1]);
    int num_threads = atoi(argv[2]);
    int iteration = atoi(argv[3]);

    if (c == 1) {
        benchmark("System malloc", malloc, free, num_threads, iteration);
    }

    if (c == 2) {
        benchmark("LYMalloc", LYMalloc, LYFree, num_threads, iteration);
    }

    return 0;
}