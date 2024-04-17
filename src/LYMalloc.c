#include "LYMalloc.h"

pthread_t reclaim_thread;
volatile int keep_running = 1;  // Flags that control the running of background threads

static BlockList globalHeap;
static ThreadHeap localHeaps;
#pragma omp threadprivate(localHeaps)

/*******
 * @brief Split a block of memory into multiple BlockNodes chained into BlockList
 * ****/
void splitMemoryToBlocks(char* mem, size_t totalSize, BlockList* list) {
    list->dummy.next = NULL;
    BlockNode* prevBlock = &list->dummy;
    int numBlocks = ceil(totalSize / BLOCK_SIZE);
    list->availableBlocks = numBlocks;

    for (int i = 0; i < numBlocks; ++i) {
        BlockNode* block = (BlockNode*)(mem + i * BLOCK_SIZE);
        prevBlock->next = block;
        prevBlock = block;
    }
}


void* reclaimRoutine(void* arg) {
    while (keep_running) {
        sleep(1);  // Performs a memory recall every 5 seconds
        reclaimMemory(omp_get_max_threads());
    }
    return NULL;
}


void initMemoryAllocator(int threadCount) {
    char* globalMemory = (char*)malloc(HEAP_SIZE * threadCount);
    splitMemoryToBlocks(globalMemory, HEAP_SIZE * threadCount, &globalHeap);

    #pragma omp parallel num_threads(threadCount)
    {
        char* localMemory = (char*)malloc(HEAP_SIZE);
        splitMemoryToBlocks(localMemory, HEAP_SIZE, &localHeaps.heap);
        localHeaps.blocksToReclaim = 4;
    }

    // Creating background recycling threads
    pthread_create(&reclaim_thread, NULL, reclaimRoutine, NULL);
}

void addToLocalHeap(BlockList* list, BlockNode* block) {
    if (list->dummy.next == NULL) {
        list->dummy.next = block;
    }
    else {
        BlockNode* current = &list->dummy;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = block;
    }
    // Increment availableBlocks for each added block
    ++list->availableBlocks;
}


BlockNode* findAndDetachBlocks(BlockList* list, int numBlocks) {
    if (list == NULL || numBlocks <= 0)
        return NULL;

    BlockNode* prev = &list->dummy;
    BlockNode* start = list->dummy.next;
    BlockNode* end = NULL;
    int count = 0;

    if (start != NULL) {
        end = start;
        count = 1;

        while (count < numBlocks && end != NULL && end->next == end + 1) {
            end = end->next;
            ++count;
        }

        if (count == numBlocks) {
            prev->next = end->next;
            end->next = NULL; // detach the segment
            list->availableBlocks -= numBlocks;
            return start;
        }

        //prev = start;
        //start = end->next;
    }

    return NULL;  // Not enough consecutive blocks were found
}


BlockNode* detachFirstBlock(BlockList* list) {
    BlockNode* block = list->dummy.next;
    if (block == NULL) {
        return NULL;
    }
    list->dummy.next = block->next;  // Remove the first block from the list
    block->next = NULL;  // Detach the block completely from the list
    return block;
}


BlockNode* findLastBlock(BlockNode* start) {
    while (start && start->next) {
        start = start->next;
    }
    return start;
}

void* customMalloc(size_t size) {
    int numBlocksNeeded = ceil((double)size / BLOCK_SIZE);
    BlockNode* firstBlock = NULL;
    BlockNode* lastBlock = NULL;

    // Try to allocate memory blocks from the local heap first
    #pragma omp critical(localHeaps)
    {
        if (localHeaps.heap.availableBlocks >= numBlocksNeeded) {
            firstBlock = findAndDetachBlocks(&localHeaps.heap, numBlocksNeeded);
            localHeaps.heap.availableBlocks -= numBlocksNeeded;
            numBlocksNeeded = 0;
        }
        //else {
        //    firstBlock = findAndDetachBlocks(&localHeaps.heap, localHeaps.heap.availableBlocks);
        //    numBlocksNeeded -= localHeaps.heap.availableBlocks;
        //    localHeaps.heap.availableBlocks = 0;
        //}
    }

    // If the localHeap is not enough, try to migrate memory blocks from the globalHeap to the localHeap
    if (numBlocksNeeded > 0) {
        #pragma omp critical(globalHeap)
        {
            if (globalHeap.availableBlocks >= numBlocksNeeded) {
                firstBlock = findAndDetachBlocks(&globalHeap, numBlocksNeeded);
                if (firstBlock != NULL) {
                    globalHeap.availableBlocks -= numBlocksNeeded;
                    // migrate to localHeap
                    addToLocalHeap(&localHeaps.heap, firstBlock);
                    localHeaps.heap.availableBlocks += numBlocksNeeded;
                }
                globalHeap.availableBlocks -= numBlocksNeeded;


            }
        }

        #pragma omp critical(localHeaps)
        {
            if (localHeaps.heap.availableBlocks >= numBlocksNeeded) {
                firstBlock = findAndDetachBlocks(&localHeaps.heap, numBlocksNeeded);
                localHeaps.heap.availableBlocks -= numBlocksNeeded;
                numBlocksNeeded = 0;
            }
        }
    }

    //If localHeap still does not satisfy the request, allocate remaining from globalHeap
    if (numBlocksNeeded > 0 && globalHeap.availableBlocks >= numBlocksNeeded) {
        #pragma omp critical(globalHeap)
        {
            while (numBlocksNeeded > 0 && globalHeap.head != NULL) {
                block = detachFirstBlock(&globalHeap);
                if (block == NULL) {
                    break;
                }
                if (lastBlock != NULL) {
                    lastBlock->next = block;
                    block->prev = lastBlock;
                }
                else {
                    firstBlock = block;
                }
                lastBlock = block;
                --numBlocksNeeded;
            }
            globalHeap.availableBlocks -= (numBlocksNeeded > 0 ? numBlocksNeeded : 0);
        }
    }

    // If blocks are still needed, allocate directly from the system
    if (numBlocksNeeded > 0) {
        while (numBlocksNeeded-- > 0) {
            block = (BlockNode*)malloc(sizeof(BlockNode) + BLOCK_SIZE);
            if (lastBlock != NULL) {
                lastBlock->next = block;
                block->prev = lastBlock;
            }
            else {
                firstBlock = block;
            }
            lastBlock = block;
        }
    }

    if (firstBlock) {
        BlockNode* block = firstBlock;
        while (block) {
            block->lastUsed = time(NULL);
            block = block->next;
        }
        return (void*)(firstBlock + 1);
    }

    return NULL;  // Memory allocation failed
}



void customFree(void* ptr) {
    BlockNode* block = (BlockNode*)((char*)ptr - sizeof(BlockNode));

    // Directly attach block to the head of the localHeap
    #pragma omp critical(localHeaps)
    {
        BlockNode* lastBlock = findLastBlock(block);
        lastBlock->next = localHeaps.heap.dummy.next;
        localHeaps.heap.dummy.next = block;
    }

    // Update lastUsed timestamps
    while (block) {
        block->lastUsed = time(NULL);
        block = block->next;
    }
}


void reclaimMemory(int num_threads) {
    const time_t currentTime = time(NULL);
    const time_t MAX_UNUSED_DURATION = 30;  // Migrate if not used for 30s

    #pragma omp parallel num_threads(num_threads)
    {
        BlockNode* current = localHeaps.heap.head;
        BlockNode* prev = NULL;

        while (current != NULL) {
            if (difftime(currentTime, current->lastUsed) > MAX_UNUSED_DURATION) {
                // Delete from localHeap
                if (prev != NULL) {
                    prev->next = current->next;
                }
                else {
                    localHeaps.heap.head = current->next;
                }

                if (current->next != NULL) {
                    current->next->prev = prev;
                }

                // Add to globalHeap
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

                current = (prev ? prev->next : localHeaps.heap.head);
            }
            else {
                prev = current;
                current = current->next;
            }
        }
    }
}


void freeMemoryAllocator() {
    // Stop background threads
    keep_running = 0;
    pthread_join(reclaim_thread, NULL);

    #pragma omp parallel
    {
        BlockNode* cur = localHeaps.heap.head;
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
    /*
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int* myData = (int*) customMalloc(sizeof(int));
        *myData = thread_id;

        #pragma omp critical
        printf("Thread %d allocated integer with value %d at address %p\n", thread_id, *myData, myData);
    }
    */

    freeMemoryAllocator();
    return 0;
}
