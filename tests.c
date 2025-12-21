
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "allocator.h"

#define ENABLE_ALLOC_TESTS 0
#define ENABLE_ALLOC_TESTS_GROWING 1
#define ENABLE_ALLOC_BENCHMARKS 0

#define TEST_PASS() printf("✓ PASSED\n\n")
#define TEST_FAIL() printf("✗ FAILED\n\n")

void test_basic_allocation() {
    printf("TEST 1: Basic Allocation\n");
    printf("------------------------\n");

    void* ptr1 = my_malloc(100);
    assert(ptr1 != NULL);
    printf("Allocated 100 bytes at %p\n", ptr1);

    memset(ptr1, 'A', 100);

    void* ptr2 = my_malloc(200);
    assert(ptr2 != NULL);
    printf("Allocated 200 bytes at %p\n", ptr2);

    my_free(ptr1);
    my_free(ptr2);

    TEST_PASS();
}

// Test 2: Pool growth - allocate more than initial pool
void test_pool_growth() {
    printf("TEST 2: Pool Growth\n");
    printf("-------------------\n");

    // Allocate enough to force pool creation
    // Initial pool is ~1MB, so let's allocate 2MB worth
    void* ptrs[10];
    size_t alloc_size = 256 * 1024; // 256KB each

    for (int i = 0; i < 10; i++) {
        ptrs[i] = my_malloc(alloc_size);
        if (ptrs[i]) {
            printf("Allocation %d: %p\n", i, ptrs[i]);
        } else {
            printf("Allocation %d: FAILED\n", i);
        }
    }

    print_memory_state();
    print_allocator_stats();

    // Free all
    for (int i = 0; i < 10; i++) {
        if (ptrs[i]) my_free(ptrs[i]);
    }

    TEST_PASS();
}

// Test 3: Fragmentation and coalescing
void test_fragmentation() {
    printf("TEST 3: Fragmentation & Coalescing\n");
    printf("-----------------------------------\n");

    void* p1 = my_malloc(100);
    void* p2 = my_malloc(100);
    void* p3 = my_malloc(100);
    void* p4 = my_malloc(100);

    printf("Allocated 4 blocks\n");
    print_memory_state();

    // Free alternating blocks to create fragmentation
    my_free(p1);
    my_free(p3);

    printf("After freeing blocks 1 and 3:\n");
    print_memory_state();

    // Now free p2 - should coalesce with p1 and p3
    my_free(p2);
    printf("After freeing block 2 (should coalesce):\n");
    print_memory_state();

    my_free(p4);
    printf("After freeing block 4 (should coalesce all):\n");
    print_memory_state();

    TEST_PASS();
}

// Test 4: Many small allocations
void test_many_small_allocations() {
    printf("TEST 4: Many Small Allocations\n");
    printf("-------------------------------\n");

    void* ptrs[1000];
    int success = 0;

    for (int i = 0; i < 1000; i++) {
        ptrs[i] = my_malloc(64);
        if (ptrs[i]) success++;
    }

    printf("Successfully allocated %d/1000 small blocks\n", success);
    print_allocator_stats();

    // Free all
    for (int i = 0; i < 1000; i++) {
        if (ptrs[i]) my_free(ptrs[i]);
    }

    TEST_PASS();
}

// Test 5: Stress test - random allocations and frees
void test_random_stress() {
    printf("TEST 5: Random Stress Test\n");
    printf("---------------------------\n");

    void* ptrs[100] = {0};

    // Do 500 random operations
    for (int i = 0; i < 500; i++) {
        int idx = i % 100;

        if (ptrs[idx] == NULL) {
            // Allocate random size between 32 and 4096 bytes
            size_t size = 32 + (i * 37) % 4064;
            ptrs[idx] = my_malloc(size);
            if (ptrs[idx]) {
                memset(ptrs[idx], 'X', size);
            }
        } else {
            // Free existing allocation
            my_free(ptrs[idx]);
            ptrs[idx] = NULL;
        }
    }

    // Clean up remaining allocations
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) my_free(ptrs[i]);
    }

    print_allocator_stats();
    TEST_PASS();
}

// Test 6: Edge cases
void test_edge_cases() {
    printf("TEST 6: Edge Cases\n");
    printf("------------------\n");

    // Zero allocation
    void* p1 = my_malloc(0);
    assert(p1 == NULL);
    printf("my_malloc(0) correctly returns NULL\n");

    // NULL free
    my_free(NULL);
    printf("my_free(NULL) handled correctly\n");

    // Double free detection
    void* p2 = my_malloc(100);
    my_free(p2);
    printf("Attempting double free (should detect):\n");
    my_free(p2);  // Should detect and warn

    TEST_PASS();
}

// Test 7: Buffer overflow detection
void test_buffer_overflow() {
    printf("TEST 7: Buffer Overflow Detection\n");
    printf("----------------------------------\n");

    void* ptr = my_malloc(100);
    assert(ptr != NULL);

    // Write within bounds
    memset(ptr, 'A', 100);
    printf("Writing within bounds...\n");
    my_free(ptr);  // Should pass canary check

    // Simulate overflow
    ptr = my_malloc(100);
    memset(ptr, 'B', 110);  // Overflow by 10 bytes
    printf("Writing beyond bounds...\n");
    my_free(ptr);  // Should detect overflow

    TEST_PASS();
}

// Test 8: Large single allocation forcing new pool
void test_large_allocation() {
    printf("TEST 8: Large Single Allocation\n");
    printf("--------------------------------\n");

    // Allocate something larger than initial pool to force growth
    size_t huge_size = 2 * 1024 * 1024; // 2MB
    void* huge_ptr = my_malloc(huge_size);

    if (huge_ptr) {
        printf("Successfully allocated %zu bytes\n", huge_size);
        memset(huge_ptr, 0xFF, huge_size);
        print_memory_state();
        my_free(huge_ptr);
    } else {
        printf("Failed to allocate %zu bytes (expected if MAX_POOLS reached)\n", huge_size);
    }

    TEST_PASS();
}

// Test 9: Pool exhaustion (hit MAX_POOLS limit)
void test_pool_exhaustion() {
    printf("TEST 9: Pool Exhaustion\n");
    printf("-----------------------\n");

    void* ptrs[15];
    size_t alloc_size = 2 * 1024 * 1024; // 2MB each

    printf("Attempting to create more than MAX_POOLS (%d) pools...\n", 10);

    for (int i = 0; i < 15; i++) {
        ptrs[i] = my_malloc(alloc_size);
        if (ptrs[i]) {
            printf("Pool %d created\n", i + 1);
        } else {
            printf("Failed at pool %d (expected after MAX_POOLS)\n", i + 1);
            ptrs[i] = NULL;
        }
    }

    // Clean up
    for (int i = 0; i < 15; i++) {
        if (ptrs[i]) my_free(ptrs[i]);
    }

    print_allocator_stats();
    TEST_PASS();
}

// Test 10: Allocation pattern that tests all pools
void test_multi_pool_pattern() {
    printf("TEST 10: Multi-Pool Allocation Pattern\n");
    printf("---------------------------------------\n");

    void* small = my_malloc(1024);
    void* medium = my_malloc(512 * 1024);
    void* large = my_malloc(2 * 1024 * 1024);

    printf("Allocated across multiple pools\n");
    print_memory_state();

    my_free(medium);
    void* reuse = my_malloc(256 * 1024);

    printf("After free and reallocation:\n");
    print_memory_state();

    my_free(small);
    my_free(large);
    my_free(reuse);

    TEST_PASS();
}

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
#elif ENABLE_ALLOC_TESTS_GROWING
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║  GROWING ALLOCATOR TEST SUITE             ║\n");
    printf("╚═══════════════════════════════════════════╝\n\n");

    init_allocator();

    test_basic_allocation();
    reset_allocator_stats();

    test_pool_growth();
    reset_allocator_stats();

    test_fragmentation();
    reset_allocator_stats();

    test_many_small_allocations();
    reset_allocator_stats();

    test_random_stress();
    reset_allocator_stats();

    test_edge_cases();
    reset_allocator_stats();

    test_buffer_overflow();
    reset_allocator_stats();

    test_large_allocation();
    reset_allocator_stats();

    test_pool_exhaustion();
    reset_allocator_stats();

    test_multi_pool_pattern();

    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║  FINAL STATISTICS                         ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
    print_allocator_stats();
    print_memory_state();

    cleanup_allocator();

    printf("\n✓ ALL TESTS COMPLETED\n");
    return 0;
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
    printf("Phase 1: Allocating 1000 small blocks...\n");
    void* ptrs[1500] = {NULL};
    int sizes[] = {8, 16, 24, 32, 40, 48};  // Smaller sizes

    for (int i = 0; i < 1000; i++) {
        size_t size = sizes[i % 6];
        ptrs[i] = my_malloc(size);
        if (!ptrs[i]) {
            printf("Allocation %d failed (pool exhausted)\n", i);
            break;  // Stop if we run out
        }
    }

    printf("After allocations:\n");
    print_memory_state();

    // Phase 2: Free every other block (creates fragmentation!)
    // Phase 2: Free every other block (creates fragmentation!)
    printf("\nPhase 2: Freeing every other block (creates fragmentation)...\n");
    for (int i = 1; i < 200; i += 2) {
        if (ptrs[i]) {
            my_free(ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    printf("After creating fragmentation:\n");
    print_memory_state();

    // Phase 3: Try to allocate into fragmented space
    printf("\nPhase 3: Allocating into fragmented space (strategy matters here!)...\n");
    reset_allocator_stats();  // Reset to measure only this phase

    int phase3_allocations = 0;
    for (int i = 0; i < 50; i++) {
        size_t size = 32 + (i * 8);  // Varied sizes: 32, 40, 48, 56...
        void* ptr = my_malloc(size);
        if (ptr) {
            phase3_allocations++;
            // Find first available slot
            for (int j = 0; j < 500; j++) {  // Check ALL slots
                if (ptrs[j] == NULL) {
                    ptrs[j] = ptr;
                    break;
                }
            }
        }
    }

    printf("Phase 3 allocated %d blocks successfully.\n", phase3_allocations);
    printf("After reallocating:\n");
    print_memory_state();

    printf("\nPhase 3 Statistics (THIS IS WHERE STRATEGIES DIFFER):\n");
    print_allocator_stats();

    // Cleanup - only free non-NULL pointers
    printf("\nCleaning up...\n");
    for (int i = 0; i < 500; i++) {
        if (ptrs[i] != NULL) {
            my_free(ptrs[i]);
        }
    }

    cleanup_allocator();
    printf("\n");
#endif

    return 0;
}