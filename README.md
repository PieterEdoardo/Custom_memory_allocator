Custom Memory Allocator
=======================

Overview
--------
This project is a custom memory allocator written in C, designed as a systems engineering and learning project with an emphasis on correctness, clarity, and allocator design trade-offs.

The allocator was developed incrementally, resulting in three distinct implementations that demonstrate increasing sophistication in memory management strategies. The project is intended as an interview-grade systems programming showcase and as a foundation for further experimentation (e.g., multithreading, arenas, or C++ integration).

Allocator Variants
------------------
The repository contains three allocator implementations:

1. Static Pool Allocator
   - Fixed-size memory pool
   - First-fit allocation strategy
   - No pool growth
   - Minimal feature set
   - Purpose: establish core allocator mechanics

2. Dynamic Pool Allocator
   - Uses mmap() to request memory from the OS
   - Supports multiple allocation strategies:
     - First-fit
     - Next-fit
     - Best-fit
   - Single pool per allocator instance
   - Coalescing on free
   - Canary-based buffer overflow detection

3. Growing Pool Allocator (Current)
   - Multiple memory pools managed as a linked list
   - Pools grow dynamically using mmap()
   - Doubly-linked block list (prev/next)
   - In-place realloc expansion when possible
   - Canary-based corruption detection
   - Strong alignment guarantees using max_align_t
   - Extensive internal consistency checks
   - Designed to minimize fragmentation over time

Key Features
------------
- Custom malloc / free / realloc implementations
- Alignment-safe allocations for all fundamental types
- Block splitting and coalescing
- Fragmentation-aware allocation strategies
- Canary-based buffer overflow detection
- Defensive pointer validation
- Pool growth with configurable limits
- Detailed allocator statistics and debugging output

Why calloc() Is Not Implemented
-------------------------------
calloc() is intentionally omitted. It is semantically equivalent to:

    void* p = malloc(n * size);
    if (p) memset(p, 0, n * size);

From an allocator design perspective, calloc() is a policy wrapper rather than a core mechanism. Implementing calloc() would be trivial on top of my_malloc(), but it does not introduce new allocator concepts beyond those already exercised by malloc(), free(), and realloc().

For this project, the focus is on allocator internals rather than API completeness.

Testing
-------
The project includes a custom test suite covering:

- Basic allocation and deallocation
- Block splitting and coalescing
- Pool growth behavior
- Fragmentation scenarios
- realloc() shrinking and expansion
- In-place realloc optimization
- Canary corruption detection
- Error handling for invalid frees

All tests are designed to be deterministic and allocator-internal-state aware.

Build Instructions
------------------
Requirements:
- Linux (tested on x86_64)
- gcc with C11 support
- mmap() support

Build all allocator variants and tests:

    > make test_growing

Run the growing pool allocator tests:

    > ./test_growing

Design Notes
------------
This allocator prioritizes correctness, observability, and extensibility over raw performance.

Notable design decisions:
- Explicit alignment discipline enforced via static assertions
- user_size tracked separately from internal block size
- Conservative splitting to reduce fragmentation
- No implicit thread safety (by design)
- Clear separation between allocator policy and mechanism

Future Work
-----------
Possible extensions include:
- Thread-local arenas
- Lock-free fast paths
- Size-class segregation
- C++ RAII wrappers
- Optional calloc() implementation
- Debug vs release allocator modes

License
-------
This project is provided for educational and demonstration purposes.
