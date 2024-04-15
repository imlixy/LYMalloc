#include "LYMalloc.h"

static BlockList globalHeap;
static BlockList localHeaps;
#pragma omp threadprivate(localHeaps)

/*******
 * @brief Split a block of memory into multiple BlockNodes chained into BlockList
 * ****/
void splitMemoryToBlocks(char* mem, size_t totalSize, BlockList* list) {
    list->head = list->tail = NULL;
    BlockNode* prevBlock = NULL;
    for (int i = 0; i < totalSize; i += BLOCK_SIZE) {
        BlockNode* block = (BlockNode*)(mem + i);
        // block->size = BLOCK_SIZE;
        block->prev = prevBlock;
        block->next = NULL;
        if (prevBlock != NULL) {
            prevBlock->next = block;
        }
        else {
            list->head = block;  // The first block becomes the head
        }
        prevBlock = block;
    }
    list->tail = prevBlock;  // The last block becomes the tail
}

void initMemoryAllocator(int threadCount) {
    char* globalMemory = (char*)malloc(HEAP_SIZE * threadCount);
    splitMemoryToBlocks(globalMemory, HEAP_SIZE * threadCount, &globalHeap);

    #pragma omp parallel
    {
        char* localMemory = (char*)malloc(HEAP_SIZE);
        splitMemoryToBlocks(localMemory, HEAP_SIZE, &localHeaps);
    }
}

BlockNode* detachFirstBlock(BlockList* list) {
    if (list->head == NULL) {
        return NULL;
    }
    BlockNode* block = list->head;
    list->head = block->next;
    if (list->head == NULL) {
        list->tail = NULL;
    }
    else {
        list->head->prev = NULL;
    }
    
    return block;
}

void* customMalloc() {
    BlockNode* block = NULL;
    #pragma omp critical
    {
        block = detachFirstBlock(&localHeaps);
        if (block == NULL) {
            block = detachFirstBlock(&globalHeap);
            if (block == NULL) {
                block = (BlockNode*)malloc(sizeof(BlockNode) + BLOCK_SIZE);
            }
        }
        if (block) {
            block->lastUsed = time(NULL);
        }
    }
    return (void*)(block + 1);  // Returns a pointer to the back part of the memory block
}


void customFree(void* ptr) {
    BlockNode* block = (BlockNode*)ptr - 1;  // Get the location of the management structure
    block->lastUsed = time(NULL);
    #pragma omp critical
    {
        block->next = localHeaps.head;
        block->prev = NULL;
        if (localHeaps.head != NULL) {
            localHeaps.head->prev = block;
        }
            
        localHeaps.head = block;
        if (localHeaps.tail == NULL) {
            localHeaps.tail = block;
        }
    }
}

void reclaimMemory() {
    const time_t currentTime = time(NULL);
    const time_t MAX_UNUSED_DURATION = 300;  // 5分钟未使用则迁移

    #pragma omp parallel
    {
        BlockNode* current = localHeaps.head;
        BlockNode* prev = NULL;

        while (current != NULL) {
            if (difftime(currentTime, current->lastUsed) > MAX_UNUSED_DURATION) {
                // 从局部堆删除
                if (prev != NULL) {
                    prev->next = current->next;
                } else {
                    localHeaps.head = current->next;
                }
                if (current->next != NULL) {
                    current->next->prev = prev;
                }

                // 添加到全局堆
                #pragma omp critical
                {
                    current->next = globalHeap.head;
                    current->prev = NULL;
                    if (globalHeap.head != NULL) {
                        globalHeap.head->prev = current;
                    }
                    globalHeap.head = current;
                    if (globalHeap.tail == NULL) {
                        globalHeap.tail = current;
                    }
                }

                current = (prev ? prev->next : localHeaps.head);
            } else {
                prev = current;
                current = current->next;
            }
        }
    }
}




void freeMemoryAllocator() {
    #pragma omp parallel
    {
        BlockNode* cur = localHeaps.head;
        while (cur != NULL) {
            BlockNode* next = cur->next;
            free(cur);
            cur = next;
        }
    }
    
    BlockNode* cur = globalHeap.head;
    while (cur != NULL) {
        BlockNode* next = cur->next;
        free(cur);
        cur = next;
    }
}


int LYMalloc(int threadCount) {
    if (threadCount == 0) {
        threadCount = omp_get_max_threads();
    }
    initMemoryAllocator(threadCount);
    
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int* myData = (int*) customMalloc(sizeof(int));
        *myData = thread_id;

        #pragma omp critical
        printf("Thread %d allocated integer with value %d at address %p\n", thread_id, *myData, myData);
    }

    freeMemoryAllocator();
    return 0;
}
