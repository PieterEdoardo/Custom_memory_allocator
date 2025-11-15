#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

/******************
 * Allocator code *
 ******************/

// pool and block size
#define POOL_SIZE 4096 // 4kb memory pool
#define MIN_BLOCK_SIZE 32// Minimum size for splitting

// Block header structure
// Metadata for each memory block
typedef struct block_header {
    size_t size;                // Size without header
    int is_free;                // Boolean
    struct block_header* next;  // Pointer to next block in the list
} block_header_t;


static char memory_pool[POOL_SIZE];
static block_header_t* free_list_head = NULL;
static int initialized = 0; // False

// Allocator initiation
void init_allocator() {
    if (initialized) return; // skip if true

    // Treat start of pool as the first header
    free_list_head = (block_header_t*)memory_pool;
    free_list_head->size = POOL_SIZE - sizeof(block_header_t);
    free_list_head->is_free = 1; // True
    free_list_head->next = NULL;

    initialized = 1; // True
    printf("[INIT] Allocator initialized with %zu bytes\n", free_list_head->size);
}

// Custom malloc implementation
void* my_malloc(size_t size) {
    if (!initialized) init_allocator(); // Init if not done yet.
    if (size == 0) return NULL; // Can't allocate if no room.

    // Align size to 8 bytes for better performance
    size = (size + 7) & ~7;

    block_header_t* current = free_list_head;

    // First-fit strategy: find first block that's big enough.
    while (current != NULL) {
        if (current ->is_free && current->size >= size) {
            printf("[ALLOC] Found free block: size=%zu at %p\n", current->size, (void*)current);

            // Should block be split?
            // Only split if remaining space is useful (> MIN_BLOCK_SIZE)
            if (current->size >= size + sizeof(block_header_t) + MIN_BLOCK_SIZE) {
                block_header_t* new_block = (block_header_t*) ((char*)current + sizeof(block_header_t) + size);

                new_block->size = current->size - size - sizeof(block_header_t);
                new_block->is_free = 1; // True
                new_block->next = current->next;

                // Update current block
                current->size = size;
                current->next = new_block;

                printf("[SPLIT] Split block: allocated=%zu, remaining=%zu\n", size, new_block->size);
            }

            // Mark block as not free
            current->is_free = 0;

            // Return pointer to data area (skip header)
            void* ptr = (char*)current + sizeof(block_header_t);
            printf("[ALLOC] Returning pointer %p\n", ptr);
            return ptr;
        }
        current = current->next;
    }
    printf("[ALLOC] FAILED: No suitable block found for size %zu\n", size);
    return NULL;
}

// Custom free implementation
void my_free(void* ptr) {
    if (!ptr) return;

    printf("[FREE] Freeing pointer %p\n", ptr);

    // Get header from user pointer
    block_header_t* header = (block_header_t*) ((char*)ptr - sizeof(block_header_t));

    // Mark as free
    header->is_free = 1;

    // Coalesce with next block if it's free
    if (header->next && header->next->is_free) {
        printf("[COALESCE] Merging with next block: %zu + %zu\n", header->size, header->next->size);
        header->size += sizeof(block_header_t) + header->next->size;
        header->next = header->next->next;
    }

    //
}

// Debug function to print memory state
void print_memory_state() {

}
/*********************
 * Test program code *
 *********************/

int main() {
    printf("Custom Memory Allocator Test\n");
    init_allocator();
    return 0;
}
