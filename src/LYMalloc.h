#ifndef LYMALLOC_H
#define LYMALLOC_H

#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>


#define HEAP_SIZE 1024 * 1024  // Assuming 1MB of local heap per thread
#define ALIGN 63
#define SIZE 256
#define GSIZE 1024

typedef struct MemoryBlock {
    void* start;
    size_t length;
    struct MemoryBlock* next;
} MemoryBlock;

typedef struct {
    MemoryBlock* freeHead;
    MemoryBlock* usedHead;
} Heap;


void initHeap(Heap* heap, void* start, size_t length);
void* reclaimRoutine(void* arg);
void initMemoryAllocator(int threadCount);
void addToHeap(MemoryBlock** head, MemoryBlock* newBlock);
MemoryBlock* findAndDetachBlock(Heap* heap, size_t len);
MemoryBlock* findBlock(Heap* heap, size_t len);
void* LYMalloc(size_t size);
void LYFree(void* ptr);
void reclaimMemory(int num_threads);
void freeMemoryAllocator(int num_threads);

#endif