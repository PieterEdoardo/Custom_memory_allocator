#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stddef.h>
#include <string.h>

#include "allocator.h"

/******************
 * Configuration *
 ******************/
#define KiB(x) ((size_t)(x) * 1024)
#define MiB(x) (KiB(x) * 1024)
#define GiB(x) (MiB(x) * 1024)
#define MAX_POOLS 10
#define BLOCK_MAGIC 0xDEADBEEF
#define FREED_MAGIC 0xFEEDFACE
#define CANARY_VALUE 0xDEADC0DE
#define ALIGNMENT (_Alignof(max_align_t))        // 16 on my platform and most x86_64 Linux systems.
#define ALIGN_UP_CONST(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define HEAD_SIZE ALIGN_UP_CONST(sizeof(block_header_t), ALIGNMENT)
#define MEMORY_POOL_SIZE ALIGN_UP_CONST(sizeof(memory_pool_t), ALIGNMENT)
#define MIN_BLOCK_SIZE ALIGN_UP_CONST(HEAD_SIZE + (ALIGNMENT * 2), ALIGNMENT)
#define INITIAL_POOL_SIZE ALIGN_UP_CONST((MiB(1)), ALIGNMENT)         // 1MB memory pool

/*******************
 * Data Structures *
 *******************/
typedef struct block_header {
    size_t size;
    size_t user_size;
    struct block_header* next;
    struct block_header* prev;
    uint32_t magic;
    uint8_t is_free;
} block_header_t;

typedef struct memory_pool {
    void* memory;
    size_t size;
    block_header_t* free_list_head;
    struct memory_pool* next;
} memory_pool_t;

_Static_assert(ALIGNMENT >= _Alignof(max_align_t), "ALIGNMENT must match platform max alignment");
_Static_assert(HEAD_SIZE % ALIGNMENT == 0, "HEAD_SIZE must preserve payload alignment");
_Static_assert(MEMORY_POOL_SIZE % ALIGNMENT == 0, "Pool header must preserve block alignment");
_Static_assert(INITIAL_POOL_SIZE >= MEMORY_POOL_SIZE + MIN_BLOCK_SIZE, "Initial pool size is too small");

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

static void* allocate_from_block(block_header_t* block, size_t user_size, size_t actual_size) {
    printf("[ALLOC] Found free block: size=%zu at %p\n", block->size, (void*)block);

    if (block->size >= actual_size + HEAD_SIZE + MIN_BLOCK_SIZE) {
        block_header_t* new_block = (block_header_t*)
            ((char*)block + HEAD_SIZE + actual_size);

        new_block->size = block->size - actual_size - HEAD_SIZE;
        new_block->user_size = user_size;
        new_block->is_free = 1;
        new_block->next = block->next;
        new_block->prev = block;
        new_block->magic = FREED_MAGIC;

        if (block->next) block->next->prev = new_block;

        block->size = actual_size;
        block->next = new_block;

        stats.splits++;
        // printf("[SPLIT] Split block: allocated=%zu, remaining=%zu\n", user_size, new_block->size);
    }

    block->is_free = 0;
    block->magic = BLOCK_MAGIC;

    void* ptr = (char*)block + HEAD_SIZE;

    uint32_t* end_canary = (uint32_t*)((char*)ptr + block->user_size);
    *end_canary = CANARY_VALUE;

    // printf("[ALLOC] Returning pointer %p\n", ptr);
    return ptr;
}

void* my_malloc(size_t size) {
    if (!initialized) init_allocator();
    if (size == 0) return NULL;

    size_t aligned_size = align_up(size);
    size_t actual_size = aligned_size + sizeof(uint32_t);
    actual_size = align_up(actual_size);

    memory_pool_t* found_pool = NULL;
    block_header_t* found_block = NULL;

    found_block = find_block_in_pools(actual_size, &found_pool);

    if (!found_block) {
        size_t min_pool_size = MEMORY_POOL_SIZE + HEAD_SIZE +  actual_size + MIN_BLOCK_SIZE;
        memory_pool_t* new_pool = create_pool(min_pool_size);

        if (!new_pool) {
            printf("[ALLOC] FAILED: Could not create new pool\n");
            return NULL;
        }

        found_block = find_block_in_pools(actual_size, &found_pool);

        if (!found_block) {
            printf("[ALLOC] FAILED: Still no space after creating pool!\n");
            return NULL;
        }
    }

    stats.allocations++;
    return allocate_from_block(found_block, size, actual_size);
}

void* my_realloc(void* ptr, size_t size) {
    if (!initialized) init_allocator();

    // TODO: Case 1 - ptr is NULL
    if (ptr == NULL) {
        return my_malloc(size);
    }
    // TODO: Case 2 - size is 0
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    // TODO: Get header and validate
    block_header_t* header = (block_header_t*)((char*)ptr - HEAD_SIZE);

    if (header->magic != BLOCK_MAGIC) {
        printf("[ERROR] Invalid pointer passed to my_realloc:%p\n", ptr);
        return NULL;
    }
    // TODO: Calculate new size
    size_t new_actual_size = align_up(align_up(size) + sizeof(uint32_t));
    // TODO: Case 3a - Shrinking (new_size <= current_size)
    if (header->size >= new_actual_size) {
        header->user_size = size;

        uint32_t* end_canary = (uint32_t*)((char*)ptr + size);
        *end_canary = CANARY_VALUE;

        size_t leftover = header->size - new_actual_size;

        if (header->size >= new_actual_size + HEAD_SIZE + MIN_BLOCK_SIZE) {
            block_header_t* new_block = (block_header_t*)((char*)header + HEAD_SIZE + new_actual_size);

            new_block->size = leftover - HEAD_SIZE;
            new_block->user_size = 0;
            new_block->is_free = 1;
            new_block->next = header->next;
            new_block->prev = header;
            new_block->magic = FREED_MAGIC;

            if (header->next) header->next->prev = new_block;

            header->size = new_actual_size;
            header->next = new_block;

            stats.splits++;
        }

        return ptr;
    }

    // TODO: Case 3b - Growing in-place (next block is free)
    if (header->next && header->next->is_free) {
        size_t available_space = header->size + HEAD_SIZE + header->next->size;

        if (available_space >= new_actual_size) {

            block_header_t* old_next = header->next;
            header->size = available_space;
            header->next = old_next->next;

            if (header->next) header->next->prev = header;

            header->user_size = size;
            uint32_t* end_canary = (uint32_t*)((char*)ptr + size);
            *end_canary = CANARY_VALUE;
            size_t leftover = header->size - new_actual_size;

            if (header->size >= new_actual_size + HEAD_SIZE + MIN_BLOCK_SIZE) {
                block_header_t* new_block = (block_header_t*)((char*)header + HEAD_SIZE + new_actual_size);

                new_block->size = leftover - HEAD_SIZE;
                new_block->user_size = 0;
                new_block->is_free = 1;
                new_block->next = header->next;
                new_block->prev = header;
                new_block->magic = FREED_MAGIC;

                if (header->next) header->next->prev = new_block;

                header->size = new_actual_size;
                header->next = new_block;

                stats.splits++;
            }
            stats.coalesces++;
        }

        return ptr;
    }
    // TODO: Case 3c - Must relocate (allocate, copy, free)
    if (new_actual_size > header->size + header->next->size) {

        void* new_ptr = my_malloc(size);
        if (!new_ptr) return NULL;

        // Copy smaller of odl and new sizes
        size_t copy_size = (header->user_size < size) ? header->user_size : size;
        memcpy(new_ptr, ptr, copy_size);

        my_free(ptr);

        return new_ptr;
    }


    // Check canary value for buffer overflow
    // if (*end_canary != CANARY_VALUE) {
    //     printf("[ERROR] Buffer overflow detected at %p! Canary was 0x%X, expected 0x%X\n",ptr, *end_canary, CANARY_VALUE);
    // } else {
    //     printf("[CANARY] Buffer overflow check passed\n");
    // }
    return ptr;
}

void my_free(void* ptr) {
    if (!ptr) return;

    block_header_t* header = (block_header_t*)((char*)ptr - HEAD_SIZE);

    // Double free checks
    if (header->magic == FREED_MAGIC) {
        printf("[ERROR] Double free detected at %p!\n", ptr);
        return;
    }

    // Alignment check
    if ((uintptr_t)ptr % ALIGNMENT != 0) {
        printf("[ERROR] Invalid pointer passed to my_free: %p\n", ptr);
        return;
    }

    // Invalid pointer checks
    if (header->magic != BLOCK_MAGIC) {
        printf("[ERROR] Invalid pointer passed to my_free: %p\n", ptr);
        return;
    }

    // Header sanity check
    if (header->size == 0) {
        printf("[ERROR] Pointer passed to my_free refers to header with invalid size: %p\n", ptr);

        return;
    }

    unsigned int* end_canary = (unsigned int*)((char*)ptr + header->user_size);
    if (*end_canary != CANARY_VALUE) {
        printf("[ERROR] Buffer overflow detected at %p! Canary was 0x%X, expected 0x%X\n",ptr, *end_canary, CANARY_VALUE);
        // Continue to free, but user knows there was corruption.
    } else {
        printf("[CANARY] Buffer overflow check passed\n");
    }

    header->magic = FREED_MAGIC;
    header->is_free = 1;



    memory_pool_t* block_pool = NULL;
    memory_pool_t* current_pool = pool_list_head;

    // Check for header within this pool's memory range.
    while (current_pool) {
        if ((void*)header >= current_pool->memory && (void*)header < current_pool->memory + current_pool->size) {
            block_pool = current_pool;
            break;
        }
        current_pool = current_pool->next;
    }

    if (!block_pool) {
        printf("[ERROR] Could not find pool for freed block!\n");
        return;
    }

    // Coalesce with next block if it's free
    if (header->next && header->next->is_free) {
        if ((void*)header->next >= block_pool->memory && (char*)header->next < (char*)block_pool->memory + block_pool->size) {
            printf("[COALESCE] Merging with next block: %zu + %zu\n", header->size, header->next->size);
            header->size += HEAD_SIZE + header->next->size;
            header->next = header->next->next;
            stats.coalesces++;

            if (header->next) header->next->prev = header;
        }
    }

    // Coalesce with previous block if it's free
    if (header->prev && header->prev->is_free) {
        printf("[COALESCE] Merging with previous block: %zu + %zu\n", header->prev->size, header->size);
        header->prev->size += HEAD_SIZE + header->size;
        header->prev->next = header->next;
        stats.coalesces++;

        if (header->next) header->next->prev = header->prev;
    }
    stats.frees++;
}

void print_memory_state(void) {
    printf("\n=== Memory State ===\n");

    if (!pool_list_head) {
        printf("No pools allocated\n");
        printf("===================\n\n");
        return;
    }

    memory_pool_t* current_pool = pool_list_head;
    int pool_num = 1;
    size_t total_free = 0;
    size_t total_used = 0;

    while (current_pool) {
        printf("\n--- Pool #%d (size: %zu bytes, addr: %p) ---\n",
               pool_num, current_pool->size, current_pool->memory);

        block_header_t* current_block = current_pool->free_list_head;
        int block_num = 0;

        while (current_block) {
            printf("  Block %d: size=%zu, %s, addr=%p\n",
                   block_num++,
                   current_block->size,
                   current_block->is_free ? "FREE" : "ALLOCATED",
                   (void*)current_block);

            if (current_block->is_free) {
                total_free += current_block->size;
            } else {
                total_used += current_block->size;
            }

            current_block = current_block->next;
        }

        current_pool = current_pool->next;
        pool_num++;
    }

    printf("\n--- Summary ---\n");
    printf("Total pools: %zu\n", stats.pools_created);
    printf("Total free: %zu bytes\n", total_free);
    printf("Total used: %zu bytes\n", total_used);
    printf("===================\n\n");
}

void cleanup_allocator(void) {
    printf("[CLEANUP] Freeing all pools...\n");

    memory_pool_t* current = pool_list_head;
    int pool_count = 0;

    while (current) {
        memory_pool_t* next = current->next;

        printf("[CLEANUP] Freeing pool at %p (size: %zu)\n",
               current->memory, current->size);

        if (munmap(current->memory, current->size) == -1) {
            perror("[ERROR] munmap failed");
        }

        pool_count++;
        current = next;
    }

    printf("[CLEANUP] Freed %d pools\n", pool_count);

    pool_list_head = NULL;
    last_allocated = NULL;
    initialized = 0;

    // Reset stats
    stats.allocations = 0;
    stats.frees = 0;
    stats.blocks_searched = 0;
    stats.splits = 0;
    stats.coalesces = 0;
    stats.pools_created = 0;
}

void print_allocator_stats(void) {
    printf("\n=== Allocator Statistics ===\n");
    printf("Total allocations: %zu\n", stats.allocations);
    printf("Total frees: %zu\n", stats.frees);
    printf("Total splits: %zu\n", stats.splits);
    printf("Total coalesces: %zu\n", stats.coalesces);
    printf("Pools created: %zu\n", stats.pools_created);

    if (stats.allocations > 0) {
        printf("Avg blocks searched per allocation: %.2f\n",
               (float)stats.blocks_searched / stats.allocations);
    }

    printf("============================\n\n");
}

void reset_allocator_stats(void) {
    stats.allocations = 0;
    stats.frees = 0;
    stats.blocks_searched = 0;
    stats.splits = 0;
    stats.coalesces = 0;
    // Don't reset pools_created - that's structural info
}