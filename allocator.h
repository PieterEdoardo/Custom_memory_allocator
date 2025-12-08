#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

void init_allocator();
void* my_malloc(size_t size);
void my_free(void* ptr);
// void* my_realloc(void* ptr, size_t size);
void print_memory_state(void);
void cleanup_allocator();
void print_allocator_stats(void);
void reset_allocator_stats(void);
void benchmark_fragmentation_test(void);

#endif