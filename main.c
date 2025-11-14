#include <stdio.h>
#include <string.h>
#include <stddef.h>

/******************
 * Allocator code *
 ******************/

// pool and block size
#define POOL_SIZE 4096 // 4kb memory pool
#define MIN_BLOCK_size // Minimum seze for splitting

// Block header structure
// Metadata for each memory block
static char memory_pool[100];

typedef struct block_header {
    size_t size;                // Size without header
    int is_free;                // Boolean
    struct block_header* next;  // Pointer to next block in the list
} block_header_t


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
    printf("[INIT] Allocator initialized with %zu bytes\n", free_list_head->size)
}

// Custom malloc implementation

// Custom free implementation

// Debug function to print memory state

/*********************
 * Test program code *
 *********************/

void main() {
    printf("Custom Memory Allocator Test\n");
    init_allocator();
}
