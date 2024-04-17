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
    list->head = list->tail = NULL;
    BlockNode* prevBlock = NULL;
    int numBlocks = totalSize / BLOCK_SIZE;
    list->availableBlocks = numBlocks;
    for (int i = 0; i < numBlocks; ++i) {
        BlockNode* block = (BlockNode*)(mem + i * BLOCK_SIZE);
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

void addToLocalHeap(BlockNode* block) {
    if (localHeaps.heap.tail != NULL) {
        localHeaps.heap.tail->next = block;
        block->prev = localHeaps.heap.tail;
        localHeaps.heap.tail = block;
    }
    else {      // localHeap is empty
        localHeaps.heap.head = localHeaps.heap.tail = block;
        block->prev = block->next = NULL;
    }
    ++localHeaps.heap.availableBlocks;
}

BlockNode* findAndDetachBlocks(BlockList* list, int numBlocks) {
    if (list == NULL || numBlocks <= 0)
        return NULL;

    BlockNode* start = list->head;
    BlockNode* end = NULL;
    int count = 0;

    // Traverse the list to find enough consecutive blocks
    while (start != NULL) {
        end = start;
        count = 1;

        // Try to find enough consecutive blocks
        while (count < numBlocks && end != NULL && end->next == end + 1) {
            end = end->next;
            ++count;
        }

        if (count == numBlocks) {
            // Separate the blocks
            if (start->prev)
                start->prev->next = end->next;
            if (end->next)
                end->next->prev = start->prev;

            if (start == list->head)
                list->head = end->next;
            if (end == list->tail)
                list->tail = start->prev;

            start->prev = NULL;
            end->next = NULL;

            return start;
        }
        start = end->next;
    }

    return NULL;  // Not enough consecutive blocks were found
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

BlockNode* findLastBlock(BlockNode* start) {
    while (start && start->next) {
        start = start->next;
    }
    return start;
}

void* customMalloc(size_t size) {
    int numBlocksNeeded = ceil((double)size / BLOCK_SIZE);
    BlockNode* block = NULL;
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
        else {
            firstBlock = findAndDetachBlocks(&localHeaps.heap, localHeaps.heap.availableBlocks);
            numBlocksNeeded -= localHeaps.heap.availableBlocks;
            localHeaps.heap.availableBlocks = 0;
        }
    }

    // If the localHeap is not enough, try to migrate memory blocks from the globalHeap to the localHeap
    if (numBlocksNeeded > 0) {
        #pragma omp critical(globalHeap)
        {           
            BlockNode* tmpBlock = NULL;
            int i = 0;
            for (i = 0; i < localHeaps.blocksToReclaim && globalHeap.head != NULL; ++i) {
                block = detachFirstBlock(&globalHeap);
                if (block == NULL) {
                    break;
                }
                addToLocalHeap(block); // Add block to localHeap
            }
            globalHeap.availableBlocks -= i;
            localHeaps.blocksToReclaim += 2;

            // Try to allocate the remaining needed blocks from localHeap now updated
            if (i > 0) {
                int tmp = (localHeaps.heap.availableBlocks < numBlocksNeeded) ? localHeaps.heap.availableBlocks : numBlocksNeeded;
                tmpBlock = findAndDetachBlocks(&localHeaps.heap, tmp);
                localHeaps.heap.availableBlocks -= tmp;
                if (firstBlock == NULL)
                    firstBlock = tmpBlock;
                else
                    lastBlock->next = tmpBlock;
                if (tmpBlock) {
                    lastBlock = (tmpBlock->next == NULL) ? tmpBlock : findLastBlock(tmpBlock);
                    numBlocksNeeded -= i;
                }
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

    // Update last used times and return
    if (firstBlock) {
        block = firstBlock;
        while (block) {
            block->lastUsed = time(NULL);
            block = block->next;
        }
        return (void*)(firstBlock + 1);
    }

    return NULL;  // Memory allocation failed
}


void customFree(void* ptr) {
    // First get the starting address of the actual BlockNode structure
    BlockNode* block = (BlockNode*)((char*)ptr - sizeof(BlockNode));
    BlockNode* current = block;

    // Need to determine the beginning and end of this series of blocks
    while (current->prev && current->prev == current - 1) {
        current = current->prev;
    }

    BlockNode* start = current;
    BlockNode* end = block;

    while (end->next && end->next == end + 1) {
        end->next->prev = end;
        end = end->next;
    }

    // Update lastUsed timestamps for each block
    current = start;
    while (current != end->next) {
        current->lastUsed = time(NULL);
        current = current->next;
    }

    #pragma omp critical(localHeaps)
    {
        // Add the entire chain of consecutive blocks back to the head of the localHeap
        if (localHeaps.heap.head != NULL) {
            localHeaps.heap.head->prev = end;
        }
        end->next = localHeaps.heap.head;
        localHeaps.heap.head = start;
        start->prev = NULL;

        if (localHeaps.heap.tail == NULL) {  // If the heap is empty, update the tail pointer
            localHeaps.heap.tail = end;
        }
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
