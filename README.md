# Custom Memory Allocator

A from-scratch implementation of dynamic memory allocation in C, demonstrating fundamental concepts of memory management, allocation strategies, and system programming.

## Overview

This project implements three versions of a custom memory allocator, each showcasing different levels of complexity and techniques:

- **V1 (Static)**: Fixed memory pool allocator using static storage
- **V2 (Dynamic)**: OS-backed allocator using `mmap()` with multiple allocation strategies
- **V3 (Coming Soon)**: Compacting allocator with defragmentation support

## Features

### Core Functionality
- ✅ Dynamic memory allocation and deallocation
- ✅ First-fit, best-fit, and next-fit allocation strategies
- ✅ Automatic block splitting and coalescing
- ✅ Memory alignment (8-byte boundaries)

### Safety & Debugging
- ✅ Buffer overflow detection (canary values)
- ✅ Double-free detection (magic numbers)
- ✅ Memory leak prevention through coalescing
- ✅ Detailed debug logging
- ✅ Memory state visualization

### Performance
- ✅ Multiple allocation strategies with trade-offs
- ✅ Statistics tracking (blocks searched, splits, coalesces)
- ✅ Minimal fragmentation through intelligent coalescing

## Project Structure
```
custom_allocator/
├── allocator.h              # Public API
├── allocator_static.c       # V1: Static pool allocator (educational)
├── allocator_dynamic.c      # V2: Dynamic allocator with mmap()
├── tests.c                  # Comprehensive tests and performance benchmarks
├── Makefile                 # Build system
└── README.md                # This file
```

## Building & Running

### Prerequisites
- GCC or Clang compiler
- Linux/Unix system (uses `mmap()` system call)
- Make build system

### Compilation
```bash
# Build all versions
make

# Build specific version
make test_static    # V1: Static allocator
make test_dynamic   # V2: Dynamic allocator
make benchmark      # Performance benchmarks

# Clean build artifacts
make clean
```

### Running Tests
```bash
# Run static allocator tests
./test_static

# Run dynamic allocator tests
./test_dynamic

# Run benchmarks
./benchmark
```

## Allocation Strategies

The dynamic allocator supports three allocation strategies, selectable at compile-time:

### 1. First-Fit
```c
#define ALLOCATOR_STRATEGY STRATEGY_FIRST_FIT
```

**Algorithm**: Returns the first free block that's large enough.

**Pros**:
- Fast allocation (O(n) but often finds quickly)
- Simple implementation
- Good for sequential allocation patterns

**Cons**:
- Can cause fragmentation at beginning of pool
- May not utilize memory optimally

**Best for**: General-purpose allocation, simple workloads

---

### 2. Best-Fit
```c
#define ALLOCATOR_STRATEGY STRATEGY_BEST_FIT
```

**Algorithm**: Searches all free blocks and returns the smallest one that fits.

**Pros**:
- Minimizes wasted space per allocation
- Better memory utilization
- Reduces fragmentation

**Cons**:
- Slower (must check all blocks)
- Can create many tiny unusable fragments

**Best for**: Memory-constrained environments, varied allocation sizes

---

### 3. Next-Fit
```c
#define ALLOCATOR_STRATEGY STRATEGY_NEXT_FIT
```

**Algorithm**: Like first-fit, but starts searching from the last allocation point.

**Pros**:
- Spreads allocations across entire pool
- Reduces fragmentation at beginning
- Often faster than first-fit

**Cons**:
- Can miss opportunities for optimal placement
- Slightly more complex state management

**Best for**: Long-running programs, uniform allocation patterns

## Performance Comparison

*(Run `./benchmark` to generate these statistics)*

| Strategy   | Avg Blocks Searched | Allocations | Fragmentation | Speed |
|-----------|---------------------|-------------|---------------|-------|
| First-Fit | 36.80               | 1000        | Moderate      | Fast  |
| Best-Fit  | 187.25              | 1000        | Low           | Slow  |
| Next-Fit  | 1.95                | 1000        | Low           | Fast  |

**Observations**:
- Best-fit searches the most blocks but produces least fragmentation
- Next-fit provides good balance between speed and memory utilization
- First-fit is fastest but can fragment memory at pool start

## API Reference

### Initialization
```c
void init_allocator(void);
void cleanup_allocator(void);
```

### Memory Management
```c
void* my_malloc(size_t size);
void my_free(void* ptr);
```

### Debugging & Statistics
```c
void print_memory_state(void);
void print_allocator_stats(void);
void reset_allocator_stats(void);
```

## Example Usage
```c
#include "allocator.h"

int main() {
    // Initialize the allocator
    init_allocator();
    
    // Allocate memory
    int* array = (int*)my_malloc(100 * sizeof(int));
    char* string = (char*)my_malloc(256);
    
    // Use the memory
    array[0] = 42;
    strcpy(string, "Hello from custom allocator!");
    
    // View memory state
    print_memory_state();
    
    // Free memory
    my_free(array);
    my_free(string);
    
    // View statistics
    print_allocator_stats();
    
    // Clean up
    cleanup_allocator();
    return 0;
}
```

## Technical Implementation Details

### Memory Layout

Each allocated block has a header containing metadata:
```c
typedef struct block_header {
    unsigned int magic;      // Validation (0xDEADBEEF / 0xFEEDFACE)
    uint8_t is_free;        // Allocation status
    uint8_t padding[3];     // Alignment padding
    size_t size;            // Block size (includes canary)
    struct block_header* next;  // Linked list pointer
} block_header_t;
```

**Memory structure:**
```
[Header: 24 bytes][User Data: N bytes][Canary: 4 bytes]
```

### Safety Features

**Buffer Overflow Detection:**
- Canary value (0xDEADC0DE) placed at end of each allocation
- Checked on `my_free()` to detect overflows
- Catches common programming errors

**Double-Free Protection:**
- Magic number validation (0xDEADBEEF for allocated, 0xFEEDFACE for freed)
- Prevents use-after-free and double-free vulnerabilities
- Detects invalid pointers

**Alignment:**
- All allocations aligned to 8-byte boundaries
- Ensures compatibility with all data types
- Optimal CPU cache performance

### Coalescing Algorithm

When freeing memory, adjacent free blocks are automatically merged:
```
Before:  [FREE 100][USED 50][FREE 200]
After freeing middle block:
        [FREE 374]  (100 + 24 + 50 + 24 + 200)
```

This prevents fragmentation and maintains large contiguous free blocks.

## Version Comparison

### V1: Static Allocator
- **Memory**: Fixed 4KB static array
- **Allocation**: First-fit only
- **Use Case**: Educational, embedded systems
- **Pros**: Simple, portable, no system calls
- **Cons**: Fixed size, limited functionality

### V2: Dynamic Allocator
- **Memory**: 1MB from OS via `mmap()`
- **Allocation**: First-fit, best-fit, or next-fit
- **Use Case**: General-purpose applications
- **Pros**: Large pool, multiple strategies, true dynamic memory
- **Cons**: Platform-specific (Linux/Unix)

## Limitations & Future Work

### Current Limitations
- Single-threaded (no mutex protection)
- Fixed pool size (doesn't grow dynamically)
- No `realloc()` implementation
- Platform-specific (Linux/Unix only)

### Planned Enhancements (V3+)
- [ ] Memory compaction / defragmentation
- [ ] Handle-based allocation system
- [ ] Thread-safety with mutexes
- [ ] Growing memory pools
- [ ] Segregated free lists (size classes)
- [ ] Cross-platform support (Windows VirtualAlloc)
- [ ] `realloc()` and `calloc()` implementations

## Learning Objectives

This project demonstrates understanding of:

✅ **Low-level C programming**
- Pointer arithmetic and manipulation
- Struct layout and alignment
- Bitwise operations for alignment

✅ **Data structures**
- Linked lists
- Memory management structures
- Metadata organization

✅ **System programming**
- System calls (`mmap`, `munmap`)
- Memory mapping
- OS-level resource management

✅ **Algorithm design**
- First-fit, best-fit, next-fit strategies
- Coalescing algorithms
- Trade-off analysis

✅ **Software engineering**
- Modular design
- Testing and debugging
- Performance benchmarking
- Documentation

## References & Inspiration

- [glibc malloc implementation (ptmalloc)](https://sourceware.org/glibc/wiki/MallocInternals)
- [Doug Lea's malloc](http://gee.cs.oswego.edu/dl/html/malloc.html)
- [jemalloc](https://github.com/jemalloc/jemalloc)
- [The C Programming Language](https://en.wikipedia.org/wiki/The_C_Programming_Language) - Kernighan & Ritchie

## Author

**Your Name**
- GitHub: [@yourusername](https://github.com/yourusername)
- Email: your.email@example.com

## License

This project is open source and available under the MIT License.

---

## Acknowledgments

Built as a learning project to understand memory management fundamentals. Special thanks to the systems programming community for resources and inspiration.