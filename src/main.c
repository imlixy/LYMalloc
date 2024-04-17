#include "LYMalloc.h"
#include "benchmark.h"

int main() {

    benchmark("System malloc", malloc, free);
    benchmark("Your Custom Allocator", customMalloc, customFree);
    benchmark("Jemalloc", je_malloc, je_free);
    // benchmark("Hoard", hoard_malloc, hoard_free); // Similar for Hoard

    false_sharing_test("System malloc" ,malloc, free);
    false_sharing_test("Your Custom Allocator", customMalloc, customFree);
    false_sharing_test("Jemalloc", je_malloc, je_free);

    fragmentation_test("System malloc", malloc, free);
    fragmentation_test("Your Custom Allocator", customMalloc, customFree);
    //fragmentation_test("Jemalloc", je_malloc, je_free);

    return 0;
}