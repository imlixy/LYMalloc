/*
 * Summary:
 * This code implements a custom dynamic memory allocator designed to work with a multi-threaded
 * environment. It features a thread-local memory management strategy to reduce contention and 
 * improve performance. The allocator manages memory through separate local heaps for each thread 
 * and a global heap for overflow and shared allocations.
 * 
 * The code also includes a background thread responsible for reclaiming memory and balancing loads
 * between local and global heaps.
 *
 * Key Features:
 * - Thread-local heaps for efficient concurrent access.
 * - A global heap to manage overflow and shared resources.
 * - A background thread that performs memory reclamation and balancing.
 * - Functions for memory allocation (`LYMalloc`), deallocation (`LYFree`), and initialization
 *   (`initMemoryAllocator`).
 * - Integration of OpenMP for handling multi-threading.
 *
 * Author: Xinyu Li
 * Last Modified: 04/17/2024
 */
#include "LYMalloc.h"

pthread_t reclaim_thread;
volatile int keep_running = 1;  // Flags that control the running of background threads

static Heap globalHeap;
static Heap localHeaps;
#pragma omp threadprivate(localHeaps)


#include "LYMalloc.h"

pthread_t reclaim_thread;
volatile int keep_running = 1;  // Flags that control the running of background threads

static Heap globalHeap;
static Heap localHeaps;
#pragma omp threadprivate(localHeaps)

void initHeap(Heap* heap, void* start, size_t length) {
    MemoryBlock* block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
    block->start = start;
    block->length = length;
    block->next = heap->freeHead;
    heap->freeHead = block;

    heap->usedHead = NULL;
}

void* reclaimRoutine(void* arg) {
    while (keep_running) {
        sleep(1);  // Performs a memory recall every 5 seconds
        reclaimMemory(omp_get_max_threads());
    }
    return NULL;
}

void initMemoryAllocator(int threadCount) {
    // allocate globalHeap
    char* globalMemory = (char*)malloc(HEAP_SIZE * threadCount);
    initHeap(&globalHeap, globalMemory, HEAP_SIZE * threadCount);

    #pragma omp parallel num_threads(threadCount)
    {
        char* localMemory = (char*)malloc(HEAP_SIZE);
        initHeap(&localHeaps, localMemory, HEAP_SIZE);
    }

    pthread_create(&reclaim_thread, NULL, reclaimRoutine, NULL);
}

void addToHeap(MemoryBlock** head, MemoryBlock* newBlock) {
    if (*head == NULL || (*head)->length < newBlock->length) {
        newBlock->next = *head;
        *head = newBlock;
    }
    else {
        // Finding the correct position ensures that the list is sorted from largest to smallest by length
        MemoryBlock* current = *head;
        MemoryBlock* prev = NULL;

        while (current != NULL && current->length >= newBlock->length) {
            prev = current;
            current = current->next;
        }

        // insert
        newBlock->next = current;
        if (prev) {
            prev->next = newBlock;
        }
    }
}


MemoryBlock* findAndDetachBlock(Heap* heap, size_t len) {
    MemoryBlock *prev = NULL, *current = heap->freeHead;
    MemoryBlock *best_fit_prev = NULL, *best_fit = NULL;

    // traverse to find the fittest block
    while (current != NULL) {
        if (current->length >= len &&
            (current->next == NULL || current->next->length < len)) {
            best_fit_prev = prev;
            best_fit = current;
            break;
        }
        prev = current;
        current = current->next;
    }

    if (best_fit != NULL) {
        // remove from free list
        if (best_fit_prev) {
            best_fit_prev->next = best_fit->next;
        }
        else {
            heap->freeHead = best_fit->next;  // only one element in free list, update head
        }

        if (best_fit->length - len >= SIZE) {
            MemoryBlock* newBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
            newBlock->start = best_fit->start;
            newBlock->length = len;
            addToHeap(&heap->usedHead, newBlock);


            best_fit->start += len;
            best_fit->length -= len;
            addToHeap(&heap->freeHead, best_fit);  // Rejoin freeHead

            return newBlock;
            
        }

        best_fit->next = NULL;
        return best_fit;
    }

    return NULL;
}

MemoryBlock* findBlock(Heap* heap, size_t len) {
    MemoryBlock *prev = NULL, *current = heap->freeHead;
    MemoryBlock *best_fit_prev = NULL, *best_fit = NULL;

    // traverse to find the fittest block
    while (current != NULL) {
        if (current->length >= len &&
            (current->next == NULL || current->next->length < len)) {
            best_fit_prev = prev;
            best_fit = current;
            break;
        }
        prev = current;
        current = current->next;
    }

    if (best_fit != NULL) {
        // remove from free list
        if (best_fit_prev) {
            best_fit_prev->next = best_fit->next;
        }
        else {
            heap->freeHead = best_fit->next;  // only one element in free list, update head
        }

        best_fit->next = NULL;
        return best_fit;
    }

    return NULL;
}


void* LYMalloc(size_t size) {
    size_t len = (size + 63) & ~63;
    MemoryBlock* block = NULL;

    // Try to allocate from local heap first
    #pragma omp critical(localHeaps)
    {
        if (localHeaps.freeHead && localHeaps.freeHead->length >= len) {
            // find the best fit block
            block = findAndDetachBlock(&localHeaps, len);
        }
    }

    // If not enough memory in local heap, try global heap
    if (!block) {
        #pragma omp critical(globalHeap)
        {
            if (globalHeap.freeHead && globalHeap.freeHead->length >= len) {
                block = findBlock(&globalHeap, len);
                if (block) {
                    if (block->length - len >= GSIZE ) {
                        size_t remaining_length = block->length - len;
                        void* remaining_start = (char*)block->start + len;

                        MemoryBlock* newBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
                        newBlock->start = remaining_start;
                        newBlock->length = remaining_length;
                        addToHeap(&globalHeap.freeHead, newBlock);
                        
                        addToHeap(&localHeaps.freeHead, block);  // Rejoin freeHead            
                    }
                }
            }
        }
    }

    // If still no block found, allocate directly from system
    if (!block) {
        block = (MemoryBlock*)malloc(sizeof(MemoryBlock) + len);
        if (block) {
            block->start = (void*)((char*)block + sizeof(MemoryBlock));
            block->length = len;
            block->next = NULL;
        }
    }

    return block ? block->start : NULL;
}

void LYFree(void* ptr) {
    if (ptr == NULL)
        return;

    // Lock the local heaps as we're going to modify them
    #pragma omp critical(localHeaps)
    {
        MemoryBlock *current = localHeaps.usedHead;
        MemoryBlock *prev = NULL;

        // Traverse the used list to find the block that matches the pointer
        while (current != NULL) {
            if ((char*)current->start == (char*)ptr) {
                // Found the block, remove it from the used list
                if (prev) {
                    prev->next = current->next;
                }
                else {
                    localHeaps.usedHead = current->next;  // Update head if it's the first block
                }

                // Before adding to the free list, clear the next pointer
                current->next = NULL;

                // Add the block to the free list, keeping it sorted
                addToHeap(&localHeaps.freeHead, current);

                break;
            }
            prev = current;
            current = current->next;
        }

        if (current == NULL) {
            // If we get here, it means the block was not found in the used list
            //fprintf(stderr, "error free!\n");
        }
    }
}

void reclaimMemory(int num_threads) {
    srand(time(NULL));
    unsigned int seed = time(NULL) ^ (omp_get_thread_num() << 16); // Seed the random number generator

    // Randomly decide how many blocks to move and free
    int blocks_to_reclaim = rand_r(&seed) % 3 + 1; // Move 1 to 3 blocks from local to global
    int blocks_to_free = rand_r(&seed) % 3 + 1; // Free 1 to 3 blocks from global

    int selected_thread_id = rand_r(&seed) % 3 + 1;

    #pragma omp parallel num_threads(num_threads)
    {
        int thread_id = omp_get_thread_num();

        if (thread_id == selected_thread_id) {
            // Perform memory reclaim only for the selected thread
            #pragma omp critical(localHeaps)
            {
                MemoryBlock *prev = NULL, *current = localHeaps.freeHead;
                int reclaimed_count = 0;

                while (current != NULL && reclaimed_count < blocks_to_reclaim) {
                    if (rand() % 2) { // Randomly decide to move this block
                        if (prev) {
                            prev->next = current->next;
                        } else {
                            localHeaps.freeHead = current->next;
                        }

                        MemoryBlock *toMove = current;
                        current = current->next; // Move to the next block

                        toMove->next = NULL;

                        #pragma omp critical(globalHeap)
                        {
                            addToHeap(&globalHeap.freeHead, toMove);
                        }

                        reclaimed_count++;
                    } else {
                        prev = current;
                        current = current->next;
                    }
                }
            }
        }

        // Free blocks from global heap
        if (omp_get_thread_num() == 0) { // Let only the master thread handle this
            #pragma omp critical(globalHeap)
            {
                MemoryBlock *prev = NULL, *current = globalHeap.freeHead;
                int freed_count = 0;

                while (current != NULL && freed_count < blocks_to_free) {
                    if (rand() % 2) { // Randomly decide to free this block
                        if (prev) {
                            prev->next = current->next;
                        } else {
                            globalHeap.freeHead = current->next;
                        }

                        MemoryBlock *toFree = current;
                        current = current->next; // Move to the next block

                        free(toFree); // Free the block
                        freed_count++;
                    } else {
                        prev = current;
                        current = current->next;
                    }
                }
            }
        }
    }
}

void freeMemoryAllocator(int num_threads) {
    // Stop the background thread first
    keep_running = 0;  // Assuming keep_running is a global volatile int used to control the reclaim thread
    pthread_join(reclaim_thread, NULL);  // Wait for the reclaim thread to finish

    // Free global heap memory
    MemoryBlock *block = globalHeap.freeHead;
    MemoryBlock *next_block;
    while (block != NULL) {
        next_block = block->next;
        free(block);  // Free the structure
        block = next_block;
    }

    // Free local heaps
    #pragma omp parallel num_threads(num_threads)
    {
        //int thread_id = omp_get_thread_num();
        MemoryBlock *local_block = localHeaps.freeHead;
        MemoryBlock *local_next_block;

        // Free all blocks in the local freeHead list
        while (local_block != NULL) {
            local_next_block = local_block->next;
            free(local_block);  // Free the structure
            local_block = local_next_block;
        }

        local_block = localHeaps.usedHead;
        // Additionally free all blocks in the local usedHead list if not already done
        while (local_block != NULL) {
            local_next_block = local_block->next;
            free(local_block);  // Free the structure
            local_block = local_next_block;
        }
    }
}
