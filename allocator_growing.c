#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include "allocator.h"

/******************
 * Configuration *
 ******************/
#define INITIAL_POOL_SIZE (1024 * 1024)         // 1MB memory pool
#define MAX_POOLS 10
#define MIN_BLOCK_SIZE 32
#define BLOCK_MAGIC 0xDEADBEEF
#define FREED_MAGIC 0xFEEDFACE
#define CANARY_VALUE 0xDEADC0DE
#define ALIGNMENT (alignof(max_align_t))        // 16 on my platform and most x86_64 Linux systems.
#define ALIGN_UP_CONST(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define HEAD_SIZE ALIGN_UP_CONST(sizeof(block_header_t), ALIGNMENT)
#define MEMORY_POOL_SIZE ALIGN_UP_CONST(sizeof(memory_pool_t), ALIGNMENT)

#define STRATEGY_FIRST_FIT 0
#define STRATEGY_NEXT_FIT 1
#define STRATEGY_BEST_FIT 2

#define ALLOCATOR_STRATEGY STRATEGY_NEXT_FIT

/*******************
 * Data Structures *
 *******************/
typedef struct block_header {
    size_t size;
    struct block_header* next;
    uint32_t magic;
    uint8_t is_free;
} block_header_t;

typedef struct memory_pool {
    void* memory;
    size_t size;
    block_header_t* free_list_head;
    struct memory_pool* next;
} memory_pool_t;

_Static_assert(ALIGNMENT >= alignof(max_align_t), "ALIGNMENT too small for platform");
_Static_assert(HEAD_SIZE % ALIGNMENT == 0, "HEAD_SIZE must preserve payload alignment");
_Static_assert(MEMORY_POOL_SIZE % ALIGNMENT == 0, "Pool header must preserve block alignment");

static memory_pool_t* pool_list_head = NULL;
static block_header_t* last_allocated = NULL;
static int initialized = 0; // False

static struct {
    size_t allocations;
    size_t frees;
    size_t blocks_searched;
    size_t splits;
    size_t coalesces;
    size_t pools_created;
} stats = {0};

/*********************
 * Helper Functions *
 *********************/
size_t align_up(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static size_t calculate_next_pool_size(size_t min_required) {
    size_t next_size = INITIAL_POOL_SIZE;

    memory_pool_t* current = pool_list_head;

    while (current) {
        if (current->size > next_size) {
            next_size = current->size;
        }
        current = current->next;
    }

    next_size *= 2;

    if (next_size < min_required) {
        next_size = min_required;
    }

    return next_size;
}

static memory_pool_t* create_pool(size_t min_size) {
    size_t pool_count = 0;
    memory_pool_t* temp = pool_list_head;
    while (temp) {
        pool_count++;
        temp = temp->next;
    }

    if (pool_count >= MAX_POOLS) {
        printf("[ERROR] Maximum number of pools (%d) reached\n", MAX_POOLS);
        return NULL;
    }

    size_t pool_size = calculate_next_pool_size(min_size);

    printf("[POOL] Creating pool #%zu of %zu bytes...\n", pool_count + 1, pool_size);

    void* memory = mmap(
        NULL,
        pool_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory == MAP_FAILED) {
        perror("[ERROR] mmap failed for new pool");
        return NULL;
    }

    memory_pool_t* new_pool = (memory_pool_t*)memory;
    new_pool->memory = memory;
    new_pool->size = pool_size;
    new_pool->next = pool_list_head;

    char* usable_start = (char*)memory + MEMORY_POOL_SIZE;
    size_t usable_size = pool_size - MEMORY_POOL_SIZE;

    new_pool->free_list_head = (block_header_t*)usable_start;
    new_pool->free_list_head->size = usable_size - HEAD_SIZE;
    new_pool->free_list_head->is_free = 1;
    new_pool->free_list_head->next = NULL;
    new_pool->free_list_head->magic = FREED_MAGIC;

    pool_list_head = new_pool;

    stats.pools_created++;
    printf("[POOL] Created at %p with %zu bytes usable. Total pools: %zu\n", memory, new_pool->free_list_head->size, stats.pools_created);

    return new_pool;
}

static block_header_t* find_block_in_pools(size_t actual_size, memory_pool_t** found_pool) {
    if (!pool_list_head) return NULL;

    size_t blocks_checked = 0;
    memory_pool_t* current_pool = pool_list_head;
    while (current_pool) {
        block_header_t* current_block = current_pool->free_list_head;
        while (current_block) {
            blocks_checked++;
            if (current_block->is_free && current_block->size >= actual_size) {
                *found_pool = current_pool;
                stats.blocks_searched += blocks_checked;
                return current_block;
            }
            current_block = current_block->next;
        }
        current_pool = current_pool->next;
    }

    stats.blocks_searched += blocks_checked;
    return NULL;
}

void init_allocator(void) {
    if (initialized) return;

    printf("[INIT] Initializing growing pool allocator...\n");

    if (!create_pool(INITIAL_POOL_SIZE)) {
        printf("[ERROR] Failed to create initial pool\n");
        return;
    }

    initialized = 1;
    printf("[INIT] Allocator initialized\n");
}

static void* allocate_from_block(block_header_t* block, size_t actual_size) {
    printf("[ALLOC] Found free block: size=%zu at %p\n", block->size, (void*)block);

    if (block->size >= actual_size + HEAD_SIZE + MIN_BLOCK_SIZE) {
        block_header_t* new_block = (block_header_t*)
            ((char*)block + HEAD_SIZE + actual_size);

        new_block->size = block->size - actual_size - HEAD_SIZE;
        new_block->is_free = 1;
        new_block->next = block->next;
        new_block->magic = FREED_MAGIC;

        block->size = actual_size;
        block->next = new_block;

        stats.splits++;
        // printf("[SPLIT] Split block: allocated=%zu, remaining=%zu\n", user_size, new_block->size);
    }

    block->is_free = 0;
    block->magic = BLOCK_MAGIC;

    void* ptr = (char*)block + HEAD_SIZE;

    uint32_t* end_canary = (uint32_t*)((char*)ptr + actual_size - sizeof(uint32_t));
    *end_canary = CANARY_VALUE;

    // printf("[ALLOC] Returning pointer %p\n", ptr);
    return ptr;
}