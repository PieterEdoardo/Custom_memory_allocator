#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include "allocator.h"

#define POOL_SIZE (1024 * 1024)       // 1MB memory pool
#define MIN_BLOCK_SIZE 32
#define BLOCK_MAGIC 0xDEADBEEF
#define FREED_MAGIC 0xFEEDFACE
#define CANARY_VALUE 0xDEADC0DE
#define ALIGNMENT 8

#define STRATEGY_FIRST_FIT 0
#define STRATEGY_NEXT_FIT 1
#define STRATEGY_BEST_FIT 2

#define ALLOCATOR_STRATEGY STRATEGY_NEXT_FIT

typedef struct block_header {
    unsigned int magic;
    uint8_t is_free;
    uint8_t padding[3];         // 3 bytes explicit padding.
    size_t size;
    struct block_header* next;  // Pointer to next block in the list
} block_header_t;

static struct {
    size_t allocations;
    size_t frees;
    size_t blocks_searched;
    size_t splits;
    size_t coalesces;
} stats = {0};

static void* memory_pool = NULL;
static block_header_t* free_list_head = NULL;
#if ALLOCATOR_STRATEGY == STRATEGY_NEXT_FIT
static block_header_t* last_allocated = NULL;  // ← ADD THIS
#endif
static int initialized = 0; // False

size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

void print_allocator_stats(void) {
    printf("\n=== Allocator Statistics ===\n");
    printf("Total allocations: %zu\n", stats.allocations);
    printf("Total frees: %zu\n", stats.frees);
    printf("Total splits: %zu\n", stats.splits);
    printf("Total coalesces: %zu\n", stats.coalesces);
    if (stats.allocations > 0) {
        printf("Avg blocks searched per allocation: %.2f\n",
               (float)stats.blocks_searched / stats.allocations);
    }
    printf("===========================\n\n");
}

void reset_allocator_stats(void) {
    stats.allocations = 0;
    stats.frees = 0;
    stats.blocks_searched = 0;
    stats.splits = 0;
    stats.coalesces = 0;
}

void init_allocator() {
    if (initialized) return;

    printf("[INIT] Requesting %zu bytes from OS via mmap()...\n", (size_t)POOL_SIZE);

    // Request memory from OS
    memory_pool = mmap(
        NULL,
        POOL_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory_pool == MAP_FAILED) {
        perror("[ERROR] mmap failed");
        memory_pool = NULL;
        return;
    }

    printf("[INIT] mmap returned: %p\n", memory_pool);
    printf("[INIT] MAP_FAILED is: %p\n", (void*)MAP_FAILED);

    if (memory_pool == NULL) {
        printf("[ERROR] memory_pool is NULL!\n");
        return;
    }

    printf("[INIT] Successfully allocated memory at %p\n", memory_pool);

    free_list_head = (block_header_t*)memory_pool;
    free_list_head->size = POOL_SIZE - sizeof(block_header_t);
    free_list_head->is_free = 1;
    free_list_head->next = NULL;
    free_list_head->magic = FREED_MAGIC;

    initialized = 1;
    printf("[INIT] Allocator initialized with %zu bytes\n", free_list_head->size);
}

void cleanup_allocator(void) {
    if (memory_pool && memory_pool != MAP_FAILED) {
        printf("[CLEANUP] Returning memory to OS via munmap()...\n");
        if (munmap(memory_pool, POOL_SIZE) == -1) {
            perror("[ERROR] munmap failed");
        } else {
            printf("[CLEANUP] Memory successfully returned to OS\n");
        }
        memory_pool = NULL;
        free_list_head = NULL;
        initialized = 0;
    }
}

static void* allocate_from_block(block_header_t* current, size_t size, size_t actual_size){
    printf("[ALLOC] Found free block: size=%zu at %p\n", current->size, (void*)current);

    if (current->size >= actual_size + sizeof(block_header_t) + MIN_BLOCK_SIZE) {
        block_header_t* new_block = (block_header_t*)((char*)current + sizeof(block_header_t) + actual_size);

        new_block->size = current->size - actual_size - sizeof(block_header_t);
        new_block->is_free = 1;
        new_block->next = current->next;
        new_block->magic = FREED_MAGIC;

        current->size = actual_size;
        current->next = new_block;
        stats.splits++;
        printf("[SPLIT] Split block: allocated=%zu, remaining=%zu\n", size, new_block->size);
    }

    current->is_free = 0;
    current->magic = BLOCK_MAGIC;

    void* ptr = (char*)current + sizeof(block_header_t);

    unsigned int* end_canary = (unsigned int*)((char*)ptr + actual_size - sizeof(unsigned int));
    *end_canary = CANARY_VALUE;

    printf("[ALLOC] Returning pointer %p (canary placed at offset %zu)\n", ptr, size);
    return ptr;
}
#if ALLOCATOR_STRATEGY == STRATEGY_FIRST_FIT
static block_header_t* find_first_fit(size_t actual_size) {
    block_header_t* current = free_list_head;
    size_t blocks_checked = 0;

    while (current != NULL) {
        blocks_checked++;
        if (current->is_free && current->size >= actual_size) {
            stats.blocks_searched += blocks_checked;
            return current;
        }
        current = current->next;
    }

    stats.blocks_searched += blocks_checked;
    return NULL;
}
#elif ALLOCATOR_STRATEGY == STRATEGY_NEXT_FIT
static block_header_t* find_next_fit(size_t actual_size) {
    block_header_t* start = last_allocated ? last_allocated : free_list_head;
    block_header_t* current = start;
    size_t blocks_checked = 0;


    while (current != NULL) {
        blocks_checked++;
        if (current->is_free && current->size >= actual_size) {
            stats.blocks_searched += blocks_checked;
            last_allocated = current;
            return current;
        }
        current = current->next;
    }

    //If nothing found, search from beginning.
    current = free_list_head;
    while (current != start) {
        blocks_checked++;
        if (current->is_free && current->size >= actual_size) {
            stats.blocks_searched += blocks_checked;
            last_allocated = current;
            return current;
        }
        current = current->next;
    }
    stats.blocks_searched += blocks_checked;
    // Still nothing?
    return NULL;
}
#elif ALLOCATOR_STRATEGY == STRATEGY_BEST_FIT
static block_header_t* find_best_fit(size_t actual_size) {
    block_header_t* current = free_list_head;
    block_header_t* best = NULL;
    size_t blocks_checked = 0;

    while (current != NULL) {
        blocks_checked++;
        if (current->is_free && current->size >= actual_size) {
            stats.blocks_searched += blocks_checked;
            // smaller?
            if (best == NULL || current->size < best->size) {
                best = current;
                // perfect?
                if (best->size == actual_size) {
                    break;
                }
            }
        }
        current = current->next;
    }
    stats.blocks_searched += blocks_checked;
    return best;
}
#endif

void* my_malloc(size_t size) {
    if (!initialized) init_allocator();
    if (size == 0) return NULL;

    size_t actual_size = align_size(size) + sizeof(unsigned int);
    actual_size = align_size(actual_size);

    block_header_t* found_block = NULL;

#if ALLOCATOR_STRATEGY == STRATEGY_FIRST_FIT
    found_block = find_first_fit(actual_size);
#elif ALLOCATOR_STRATEGY == STRATEGY_NEXT_FIT
    found_block = find_next_fit(actual_size);
#elif ALLOCATOR_STRATEGY == STRATEGY_BEST_FIT
    found_block = find_best_fit(actual_size);
#endif

    stats.allocations++;

    if (!found_block) {
        printf("[ALLOC] FAILED: No suitable block found for size %zu\n", size);
        return NULL;
    }

    return allocate_from_block(found_block, size, actual_size);
}

void my_free(void* ptr) {
    if (!ptr) return;

    printf("[FREE] Freeing pointer %p\n", ptr);

    // Get header from user pointer
    block_header_t* header = (block_header_t*) ((char*)ptr - sizeof(block_header_t));

    if (header->magic == FREED_MAGIC) {
        printf("[ERROR] Double free detected at %p!\n", ptr);
        return;
    }

    if (header->magic != BLOCK_MAGIC) {
        printf("[ERROR] Invalid pointer passed to my_free: %p\n", ptr);
        return;
    }

    // Check end canary for buffer overflow
    unsigned int* end_canary = (unsigned int*)((char*)ptr + header->size - sizeof(unsigned int));
    if (*end_canary != CANARY_VALUE) {
        printf("[ERROR] Buffer overflow detected at %p! Canary was 0x%X, expected 0x%X\n",ptr, *end_canary, CANARY_VALUE);
        // Continue to free, but user knows there was corruption.
    } else {
        printf("[CANARY] Buffer overflow check passed\n");
    }

    header->magic = FREED_MAGIC;
    header->is_free = 1;

    // Coalesce with next block if it's free
    if (header->next && header->next->is_free) {
        stats.coalesces++;
        printf("[COALESCE] Merging with next block: %zu + %zu\n", header->size, header->next->size);
        header->size += sizeof(block_header_t) + header->next->size;
        header->next = header->next->next;
    }

    // Coalesce with previous block if it's free
    // Need to find previous block by walking from head
    block_header_t* current = free_list_head;
    while (current && current->next != header) {
        current = current->next;
    }

    if (current && current->is_free) {
        printf("[COALESCE] Merging with previous block: %zu + %zu\n", current->size, header->size);
        current->size += sizeof(block_header_t) + header->size;
        current->next = header->next;
    }
    stats.frees++;
}

void print_memory_state() {
    printf("\n=== Memory State ===\n");
    block_header_t* current = free_list_head;
    int block_num = 0;
    size_t total_free = 0;
    size_t total_allocated = 0;

    while (current != NULL) {
        printf("Block %d: size=%zu, %s, addr=%p\n",
            block_num++,
            current->size,
            current->is_free ? "FREE" : "ALLOCATED",
            (void*)current);

        if (current->is_free) {
            total_free += current->size;
        } else {
            total_allocated += current->size;
        }

        current = current->next;
    }

    printf("Total free: %zu bytes\n", total_free);
    printf("Total used: %zu bytes\n", total_allocated);
    printf("===================\n\n");
}