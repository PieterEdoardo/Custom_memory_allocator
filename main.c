#include <stdio.h>
#include <string.h>
#include <stddef.h>

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
void my_malloc(size_t size) {
    if (!initialized) init_allocator(); // Init if not done yet.
    if (size == 0) return NULL; // Can't allocate if no room.

    // Align size to 8 bytes for better performance
    size = (size + 7) & ~7;

    block_header_t* current = free_list_head;

    // First-fit strategy: find first block that's big enough.
    while (current =! NULL) {
        if (current ->is_free && current->size >= size) {
            printf("[ALLOC] Found free block: size=%zu at %p\n", current->size, (void*)current);

            // Should block be split?
            // Only split if remaining space is useful (> MIN_BLOCK_SIZE)

        }
    }
}

// Custom free implementation
void my_free() {

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
