
#include <stdio.h>
#include <string.h>

#include "allocator.h"

#define ENABLE_ALLOC_TESTS 0
#define ENABLE_ALLOC_BENCHMARKS 1

/****************
 * Test program *
 ****************/
int main() {
#if ENABLE_ALLOC_TESTS
    printf("Custom Memory Allocator Test\n");
    init_allocator();

    printf("--- Test 1: Basic Allocation ---\n");
    int* a = my_malloc(sizeof(int) * 10);  // 40 bytes
    char* b = my_malloc(100);
    double* c = my_malloc(sizeof(double) * 5);  // 40 bytes
    int* e = my_malloc(sizeof(int) * 10);
    print_memory_state();

    printf("--- Test 2: Using Allocated Memory ---\n");
    if (a) {
        a[0] = 42;
        a[9] = 99;
        printf("a[0] = %d, a[9] = %d\n", a[0], a[9]);
    }
    if (b) {
        strcpy(b, "Hello from custom allocator!");
        printf("b = \"%s\"\n", b);
    }
    printf("\n");

    printf("--- Test 3: Freeing Memory ---\n");
    my_free(b);
    print_memory_state();

    printf("--- Test 4: Coalescing ---\n");
    my_free(a);
    print_memory_state();

    my_free(c);
    print_memory_state();

    printf("--- Test 5: Reuse Freed Memory ---\n");
    char* d = my_malloc(200);
    if (d) {
        strcpy(d, "Reusing freed memory!");
        printf("d = \"%s\"\n", d);
    }
    print_memory_state();

    printf("--- Test 6: Allocation Failure ---\n");
    void* huge = my_malloc(10000);  // Too large
    if (!huge) {
        printf("Allocation failed as expected (requested too much)\n");
    }
    print_memory_state();

    printf("--- Test 7: Double Free ---\n");
    my_free(e);
    my_free(e);
    print_memory_state();

    printf("--- Test 8: Buffer Overflow Detection ---\n");
    int* overflow_test = my_malloc(10 * sizeof(int));  // 40 bytes
    if (overflow_test) {
        // Write within bounds - OK
        overflow_test[0] = 1;
        overflow_test[9] = 10;

        // Write OUT of bounds - corrupts canary!
        overflow_test[12] = 999;  // 12 * 4 = 48 bytes (past the 40 allocated)

        printf("Attempting to free buffer with overflow...\n");
        my_free(overflow_test);  // Should detect corruption!
    }
    print_memory_state();

    printf("--- Test 9: Alignment Verification ---\n");
    for (int i = 0; i < 5; i++) {
        void* ptr = my_malloc(40);
        printf("Allocated pointer: %p (address mod 8 = %lu)\n",
               ptr, (unsigned long)ptr % 8);
        if ((unsigned long)ptr % 8 != 0) {
            printf("❌ MISALIGNED!\n");
        } else {
            printf("✓ Aligned\n");
        }
    }

    my_free(d);
    print_memory_state();

    print_allocator_stats();

    printf("--- Cleanup ---");
    cleanup_allocator();

#elif ENABLE_ALLOC_BENCHMARKS
    printf("\n\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║         ALLOCATION STRATEGY BENCHMARK          ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    printf("=== Fragmentation Stress Test ===\n");
    printf("This test creates fragmentation and measures strategy performance.\n\n");

    init_allocator();
    reset_allocator_stats();

    // Phase 1: Allocate many different-sized blocks
    printf("Phase 1: Allocating 50 varied-size blocks...\n");
    void* ptrs[100] = {NULL};
    int sizes[] = {16, 32, 64, 128, 256, 48, 80, 24, 96, 160};

    for (int i = 0; i < 50; i++) {
        size_t size = sizes[i % 10];
        ptrs[i] = my_malloc(size);
    }

    printf("After allocations:\n");
    print_memory_state();

    // Phase 2: Free every other block (creates fragmentation!)
    printf("\nPhase 2: Freeing every other block (creates fragmentation)...\n");
    for (int i = 1; i < 50; i += 2) {
        my_free(ptrs[i]);
        ptrs[i] = NULL;
    }

    printf("After creating fragmentation:\n");
    print_memory_state();

    // Phase 3: Try to allocate into fragmented space
    printf("\nPhase 3: Allocating into fragmented space (strategy matters here!)...\n");
    reset_allocator_stats();  // Reset to measure only this phase

    for (int i = 0; i < 20; i++) {
        size_t size = 32 + (i * 8);  // Varied sizes: 32, 40, 48, 56...
        void* ptr = my_malloc(size);
        if (ptr) {
            // Use first available slot
            for (int j = 0; j < 100; j++) {
                if (ptrs[j] == NULL) {
                    ptrs[j] = ptr;
                    break;
                }
            }
        }
    }

    printf("After reallocating:\n");
    print_memory_state();

    printf("\nPhase 3 Statistics (THIS IS WHERE STRATEGIES DIFFER):\n");
    print_allocator_stats();

    // Cleanup
    for (int i = 0; i < 100; i++) {
        if (ptrs[i] != NULL) {
            if (ptrs[i]) my_free(ptrs[i]);
        } else {
            printf("[BENCHMARK ERROR] ptrs[%d] = NULL\n", i);
        }
    }

    cleanup_allocator();
    printf("\n");
#endif

    return 0;
}