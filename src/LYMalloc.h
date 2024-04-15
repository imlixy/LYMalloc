#ifndef LYMALLOC_H
#define LYMALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

typedef struct BlockNode {
    struct BlockNode* prev;
    struct BlockNode* next;
    // size_t size;
    time_t lastUsed;  // Record the last time a memory block was used
} BlockNode;

typedef struct {
    BlockNode* head;
    BlockNode* tail;
} BlockList;

typedef struct {
    BlockList heap;
    int blocksToReclaim;
} ThreadHeap;

#define HEAP_SIZE 1024 * 1024  // Assuming 1MB of local heap per thread
#define BLOCK_SIZE 256
#define BLOCK_COUNT 1024
#define NUM_SIZE_CLASSES 3

void splitMemoryToBlocks(char* mem, size_t totalSize, BlockList* list);
void initMemoryAllocator(int threadCount);
BlockNode* detachFirstBlock(BlockList* list);
void* customMalloc();
void customFree(void* ptr);
void reclaimMemory();
void freeMemoryAllocator();
int LYMalloc(int threadCount);


#endif