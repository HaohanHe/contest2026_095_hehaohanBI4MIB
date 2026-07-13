\[ English | [简体中文](../../../zh-cn/api/kernel/mem.md) \]

# Memory Management API

openvela provides a flexible memory management system that supports the standard POSIX memory allocation interfaces as well as extended memory management features.

Headers: `#include <stdlib.h>` (standard allocation), `#include <nuttx/mm/mm.h>` (kernel heap / heap management)

## openvela Implementation Notes

- **Build mode impact**:
  - **Flat Build**: Only one user heap; `malloc/free` operate on it directly
  - **Protected Build**: Kernel heap + user heap, isolated by MPU protection
  - **Kernel Build**: Kernel heap + multiple user heaps (one per task group)
- **Alignment guarantee**: Memory returned by `malloc()` is aligned to `MM_ALIGN` (default 8 or 16 bytes)
- **Thread safety**: All standard allocation interfaces (malloc/free/calloc, etc.) are thread-safe in a multitasking environment
- **Deferred free**: The `*_delayfree()` family of interfaces is used for scenarios where memory cannot be freed immediately (for example, in interrupt context)
- **Known incompatibility**: `posix_memalign()` currently does not check the validity of the `alignment` parameter and does not return `EINVAL`


## Standard Memory Allocation


### malloc

```c
void *malloc(size_t size);
```

Allocate a memory block of the specified size from the user heap. `malloc()` searches the heap for a free block large enough and allocates the required memory from it. If `size` is 0, the behavior is implementation-defined; it may return `NULL` or a unique pointer.

The allocated memory is guaranteed to be aligned to `MM_ALIGN` (default `2 * sizeof(uintptr_t)`, i.e. 8 or 16 bytes), which ensures that access to any fundamental data type is properly aligned.

**Parameters**:

- `size` Size of memory to allocate, in bytes.

**Returns**:

Returns a pointer to the allocated memory on success, or `NULL` on failure with `errno` set:

- `ENOMEM` Insufficient memory available.

**Note**:

- The contents of the allocated memory are uninitialized and may contain arbitrary data.
- The returned pointer can be passed to `free()`, `realloc()`, and related functions.
- `malloc()` is thread-safe in a multitasking environment.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### free

```c
void free(void *ptr);
```

Release a memory block previously allocated by `malloc()`, `calloc()`, `realloc()`, `memalign()`, etc., making it available for subsequent allocations.

If `ptr` is `NULL`, no operation is performed. If `ptr` was not returned by a previous allocation function, or has already been freed, the behavior is undefined.

**Parameters**:

- `ptr` Pointer to the memory block to be freed.

**Returns**:

No return value.

**Note**:

- The freed pointer should not be used afterwards (dangling pointer).
- Do not free the same block twice (double free causes heap corruption).
- Do not free memory that was not dynamically allocated (such as stack variables or globals).
- After freeing, memory is not necessarily returned to the system; it may remain in the heap for subsequent allocations.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### calloc

```c
void *calloc(size_t n, size_t elem_size);
```

Allocate an array of `n` elements, each of `elem_size` bytes, for a total of `n * elem_size` bytes, and initialize all bytes to zero.

Compared with `malloc()`, `calloc()` has two advantages: it zero-initializes the memory, and it can detect multiplication overflow when computing the total size (depending on the implementation).

**Parameters**:

- `n` Number of elements to allocate.
- `elem_size` Size of each element, in bytes.

**Returns**:

Returns a pointer to the allocated and zeroed memory on success, or `NULL` on failure with `errno` set:

- `ENOMEM` Insufficient memory available.

**Note**:

- If `n` or `elem_size` is 0, the function may return `NULL` or a unique pointer.
- All bytes of the returned memory are set to 0.
- For data structures that require zero initialization, prefer `calloc()` over `malloc()` + `memset()`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### realloc

```c
void *realloc(void *ptr, size_t size);
```

Change the size of a previously allocated memory block. `realloc()` may expand or shrink the block in place, or allocate a new block and copy the existing data.

If `ptr` is `NULL`, `realloc()` behaves like `malloc(size)`. If `size` is 0 and `ptr` is not `NULL`, it behaves like `free(ptr)` and returns `NULL`.

**Parameters**:

- `ptr` Pointer to a previously allocated memory block. If `NULL`, equivalent to `malloc(size)`.
- `size` New size, in bytes. If 0, the memory is freed and `NULL` is returned.

**Returns**:

Returns a pointer to the reallocated memory on success (possibly different from the original), or `NULL` on failure with `errno` set:

- `ENOMEM` Insufficient memory available. The original block is left unchanged and remains valid.

**Note**:

- If the new size is larger than the original, the content of the new area is uninitialized.
- If the new size is smaller than the original, data beyond the new size is lost.
- The returned pointer may differ from the original; after a successful call the original pointer must not be used.
- If `realloc()` fails, the original block is not freed and the caller is still responsible for freeing it.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### reallocarray

```c
void *reallocarray(void *ptr, size_t n, size_t elem_size);
```

Change the size of a previously allocated memory block to `n * elem_size` bytes. Unlike `realloc()`, `reallocarray()` safely checks for multiplication overflow, preventing security issues caused by integer overflow.

This function is particularly useful for reallocating arrays, because using `realloc(ptr, n * elem_size)` directly may, due to multiplication overflow, allocate a block much smaller than expected.

**Parameters**:

- `ptr` Pointer to a previously allocated memory block. If `NULL`, this is equivalent to allocating new memory.
- `n` Number of elements.
- `elem_size` Size of each element, in bytes.

**Returns**:

Returns a pointer to the reallocated memory on success, or `NULL` on failure with `errno` set:

- `ENOMEM` Insufficient memory available, or `n * elem_size` overflowed.

**Note**:

- If `n * elem_size` would cause integer overflow, the function safely returns `NULL`.
- As with `realloc()`, the original block is left unchanged on failure.

**POSIX Compatibility**: Compatible with the BSD/glibc extension interface.


### zalloc

```c
void *zalloc(size_t size);
```

Allocate a memory block of the specified size and initialize all bytes to zero. This is a convenience function provided by openvela, functionally equivalent to `calloc(1, size)` but with clearer semantics.

**Parameters**:

- `size` Size of memory to allocate, in bytes.

**Returns**:

Returns a pointer to the allocated and zeroed memory on success, or `NULL` on failure with `errno` set:

- `ENOMEM` Insufficient memory available.

**Note**:

- Unlike `malloc()`, the returned memory is zero-initialized.
- Functionally identical to `calloc(1, size)`, but more concise to call.

**POSIX Compatibility**: openvela extension interface.

---


The following interfaces are used to query heap usage and memory allocation statistics, which is useful for debugging and monitoring memory usage.


## Aligned Memory Allocation


### memalign

```c
void *memalign(size_t alignment, size_t size);
```

Allocate memory aligned to the specified boundary. This is useful for hardware access with specific alignment requirements (such as DMA buffers) or for SIMD operations.

**Parameters**:

- `alignment` Alignment boundary; must be a power of two (such as 16, 32, 64, 4096).
- `size` Size of memory to allocate, in bytes.

**Returns**:

Returns a pointer to the memory aligned to the specified boundary on success, or `NULL` on failure with `errno` set:

- `ENOMEM` Insufficient memory available.
- `EINVAL` `alignment` is not a power of two.

**Note**:

- The allocated memory can be released normally using `free()`.
- For memory that needs page alignment, use `valloc()` or `posix_memalign()`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name (obsolete; `posix_memalign()` is recommended).


### posix_memalign

```c
int posix_memalign(void **memptr, size_t alignment, size_t size);
```

Allocate memory aligned to the specified boundary and store the pointer in `memptr`. This is the POSIX standard replacement for `memalign()`.

The main difference from `memalign()` is that `posix_memalign()` reports errors through the return value instead of `errno`, which makes error handling more explicit.

**Parameters**:

- `memptr` Address at which to store the pointer to the allocated memory. On success, `*memptr` is set to the address of the allocated memory.
- `alignment` Alignment boundary; must be a multiple of `sizeof(void *)` and a power of two.
- `size` Size of memory to allocate, in bytes.

**Returns**:

- `0` Success; `*memptr` points to the allocated memory.
- `ENOMEM` Insufficient memory available.

**Note**:

- Unlike `memalign()`, errors are reported via the return value rather than `errno`.
- The allocated memory can be released normally using `free()`.
- The current openvela implementation does not check the validity of the `alignment` parameter and does not return `EINVAL` (the POSIX standard requires `EINVAL` when the parameter is invalid).

**POSIX Compatibility**: Partially compatible with the `POSIX` interface of the same name (does not return `EINVAL`).


### aligned_alloc

```c
void *aligned_alloc(size_t alignment, size_t size);
```

Allocate memory aligned to the specified boundary. This is the aligned memory allocation function introduced by the C11 standard.

**Parameters**:

- `alignment` Alignment boundary; must be a power of two.
- `size` Size of memory to allocate, in bytes. The C11 standard requires that `size` be a multiple of `alignment`, but many implementations do not enforce this.

**Returns**:

Returns a pointer to the memory aligned to the specified boundary on success, or `NULL` on failure.

**Note**:

- The allocated memory can be released normally using `free()`.
- As part of the C11 standard, this function is more portable than `memalign()`.

**POSIX Compatibility**: Compatible with the C11 standard interface.


### valloc

```c
void *valloc(size_t size);
```

Allocate memory aligned to a page boundary. The page size is system-defined, typically 4096 bytes.

This function is equivalent to `memalign(sysconf(_SC_PAGESIZE), size)`.

**Parameters**:

- `size` Size of memory to allocate, in bytes.

**Returns**:

Returns a pointer to page-aligned memory on success, or `NULL` on failure.

**Note**:

- The page size can be obtained via `sysconf(_SC_PAGESIZE)`.
- The allocated memory can be released normally using `free()`.
- This function is obsolete; new code should use `posix_memalign()` instead.

**POSIX Compatibility**: Compatible with the BSD extension interface (obsolete; `posix_memalign()` is recommended).


## Memory Information Queries


### mallinfo

```c
struct mallinfo mallinfo(void);
```

Obtain memory allocation statistics for the user heap. This is useful for monitoring application memory usage, detecting memory leaks, and optimizing memory use.

**Parameters**:

None.

**Returns**:

Returns a `struct mallinfo` structure containing the following fields:

- `arena` Total memory managed by the heap, in bytes.
- `ordblks` Number of free blocks.
- `aordblks` Number of allocated blocks.
- `mxordblk` Size of the largest free block, in bytes; represents the largest memory that a single allocation can obtain.
- `uordblks` Total size of allocated memory, in bytes.
- `fordblks` Total size of free memory, in bytes.
- `usmblks` Maximum amount of memory ever allocated (high-water mark).

**Note**:

- `uordblks + fordblks` should be close to `arena` (possibly slightly smaller due to heap management overhead).
- `mxordblk` represents the largest block that can currently be allocated successfully.

**POSIX Compatibility**: Compatible with the glibc extension interface.


### mallinfo_task

```c
struct mallinfo_task mallinfo_task(FAR const struct malltask *task);
```

Obtain memory allocation information for a specific task or a specific type. This is an openvela extension that can be used to track per-task memory usage and help locate memory leaks.

**Parameters**:

- `task` Pointer to a `struct malltask` structure, containing:
  - `pid` Process ID. This can be a specific process ID, or one of the following special values:
    - `PID_MM_MEMPOOL` (-1): Query memory pool allocations.
    - `PID_MM_LEAK` (-2): Query possible memory leaks (whose allocator has already exited).
    - `PID_MM_ALLOC` (-3): Query all allocated memory.
    - `PID_MM_FREE` (-4): Query free memory.
    - `PID_MM_BIGGEST` (-5): Query the largest memory block.
    - `PID_MM_ORPHAN` (-6): Query orphan memory blocks.
  - `seqmin` Minimum sequence number (requires `CONFIG_MM_RECORD_SEQNO`).
  - `seqmax` Maximum sequence number (requires `CONFIG_MM_RECORD_SEQNO`).

**Returns**:

Returns a `struct mallinfo_task` structure containing:

- `aordblks` Number of blocks allocated by the task.
- `uordblks` Total size of memory allocated by the task, in bytes.

**Note**:

- Using `PID_MM_LEAK` quickly surfaces memory blocks left behind by exited tasks, which are very likely to be memory leaks.
- The sequence number feature requires `CONFIG_MM_RECORD_SEQNO` to be enabled and can be used to track allocations within a specific time range.

**POSIX Compatibility**: openvela extension interface.


### malloc_size

```c
size_t malloc_size(void *ptr);
```

Return the actual usable size of a previously allocated memory block. Due to memory alignment and heap management requirements, the memory actually allocated may be larger than requested.

**Parameters**:

- `ptr` Pointer to a previously allocated memory block.

**Returns**:

Returns the actual usable size of the block, in bytes. This value is normally greater than or equal to the size requested at allocation.

**Note**:

- The alias `malloc_usable_size()` (glibc compatible) can also be used.
- It is safe to use the entire range of memory indicated by the returned value.
- If `ptr` is not a valid allocation pointer, the behavior is undefined.

**POSIX Compatibility**: Compatible with the glibc/macOS extension interface.


### mallopt

```c
int mallopt(int param, int value);
```

Adjust memory allocator parameters. This function provides a way to control the behavior of the memory allocator.

**Parameters**:

- `param` Parameter to adjust. Defined options include:
  - `M_TRIM_THRESHOLD` Trim threshold
  - `M_TOP_PAD` Top padding
  - `M_MMAP_THRESHOLD` mmap threshold
  - `M_MMAP_MAX` Maximum number of mmap allocations
  - `M_CHECK_ACTION` Check action
  - `M_PERTURB` Memory perturbation
  - `M_ARENA_TEST` arena test
  - `M_ARENA_MAX` Maximum number of arenas
- `value` Parameter value.

**Returns**:

Returns a non-zero value on success, 0 on failure.

**Note**:

- The current openvela implementation always returns 1 (success) and does not actually process any parameters.
- This interface is provided for compatibility only.

**POSIX Compatibility**: Compatible with the glibc extension interface (interface-only compatibility, no actual functionality).

---




## Kernel Heap Interfaces


### kmm_initialize

```c
void kmm_initialize(void *heap_start, size_t heap_size);
```

Initialize the kernel heap. This function is usually called early during system startup by board-level initialization code.

**Parameters**:

- `heap_start` Start address of the kernel heap memory.
- `heap_size` Size of the kernel heap, in bytes.

**Returns**:

No return value.

**Note**:

- This function should be called only once; repeated calls result in undefined behavior.
- Requires `CONFIG_MM_KERNEL_HEAP` to be enabled.


### kmm_malloc

```c
void *kmm_malloc(size_t size);
```

Allocate memory from the kernel heap. Behaves like `malloc()`, but allocates from the kernel heap instead of the user heap.

**Parameters**:

- `size` Size of memory to allocate, in bytes.

**Returns**:

Returns a pointer to the allocated memory on success, or `NULL` on failure.

**Note**:

- Memory allocated here can only be released using `kmm_free()`.
- Memory allocated on the kernel heap should not be passed to user-mode code.


### kmm_free

```c
void kmm_free(void *mem);
```

Release memory previously allocated from the kernel heap by `kmm_malloc()`, `kmm_calloc()`, and similar functions.

**Parameters**:

- `mem` Pointer to the memory block to be freed. If `NULL`, no operation is performed.

**Returns**:

No return value.

**Note**:

- Only memory allocated from the kernel heap may be released here; it must not be used to free user heap memory.
- `kmm_heapmember()` can be used to check whether a pointer belongs to the kernel heap.


### kmm_calloc

```c
void *kmm_calloc(size_t n, size_t elem_size);
```

Allocate zeroed memory from the kernel heap. Behaves like `calloc()`, but allocates from the kernel heap.

**Parameters**:

- `n` Number of elements to allocate.
- `elem_size` Size of each element, in bytes.

**Returns**:

Returns a pointer to the allocated and zeroed memory on success, or `NULL` on failure.


### kmm_realloc

```c
void *kmm_realloc(void *oldmem, size_t newsize);
```

Reallocate kernel heap memory. Behaves like `realloc()`, but operates on the kernel heap.

**Parameters**:

- `oldmem` Pointer to a block previously allocated from the kernel heap. If `NULL`, equivalent to `kmm_malloc(newsize)`.
- `newsize` New size, in bytes. If 0, the memory is freed.

**Returns**:

Returns a pointer to the reallocated memory on success (possibly different from the original), or `NULL` on failure (the original block is left unchanged).


### kmm_zalloc

```c
void *kmm_zalloc(size_t size);
```

Allocate zeroed memory from the kernel heap. Behaves like `zalloc()`, but allocates from the kernel heap.

**Parameters**:

- `size` Size of memory to allocate, in bytes.

**Returns**:

Returns a pointer to the allocated and zeroed memory on success, or `NULL` on failure.


### kmm_memalign

```c
void *kmm_memalign(size_t alignment, size_t size);
```

Allocate aligned memory from the kernel heap. Behaves like `memalign()`, but allocates from the kernel heap.

**Parameters**:

- `alignment` Alignment boundary; must be a power of two.
- `size` Size of memory to allocate, in bytes.

**Returns**:

Returns a pointer to the memory aligned to the specified boundary on success, or `NULL` on failure.

**Note**:

- Useful for kernel scenarios that require aligned memory, such as DMA buffers.


### kmm_malloc_size

```c
size_t kmm_malloc_size(void *mem);
```

Return the actual usable size of an allocated block in the kernel heap.

**Parameters**:

- `mem` Pointer to a block allocated from the kernel heap.

**Returns**:

Returns the actual usable size of the block, in bytes.

**POSIX Compatibility**: openvela/NuttX extension interface.


### kmm_mallinfo

```c
struct mallinfo kmm_mallinfo(void);
```

Obtain allocation information for the kernel heap. Behaves like `mallinfo()`, but returns statistics for the kernel heap.

**Parameters**:

None.

**Returns**:

Returns a `struct mallinfo` structure containing memory allocation statistics for the kernel heap:

- `arena` Total size of the kernel heap, in bytes.
- `ordblks` Number of free blocks.
- `uordblks` Size of used memory, in bytes.
- `fordblks` Size of free memory, in bytes.
- `mxordblk` Size of the largest contiguous free block, in bytes.

**Note**:

- Can be used to monitor kernel memory usage.


### kmm_heapmember

```c
bool kmm_heapmember(void *mem);
```

Check whether a memory address belongs to the kernel heap.

**Parameters**:

- `mem` Memory address to check.

**Returns**:

Returns `true` if the address belongs to the kernel heap; otherwise `false`.

**Note**:

- Can be used to decide whether a block should be freed with `kmm_free()` or `free()`.
- Used in memory management code to route free requests to the correct heap.


### kmm_addregion

```c
void kmm_addregion(void *heapstart, size_t heapsize);
```

Add a new memory region to the kernel heap. Allows the kernel heap to use non-contiguous memory regions.

**Parameters**:

- `heapstart` Start address of the new memory region.
- `heapsize` Size of the new memory region, in bytes.

**Returns**:

No return value.

**Note**:

- Requires `CONFIG_MM_KERNEL_HEAP` to be enabled.
- The maximum number of regions is configured by `CONFIG_MM_REGIONS`.

**POSIX Compatibility**: openvela/NuttX extension interface.


### kmm_extend

```c
void kmm_extend(void *mem, size_t size, int region);
```

Extend a specific memory region of the kernel heap. The additional memory must be physically adjacent to the existing region.

**Parameters**:

- `mem` Start address of the additional memory.
- `size` Size of the additional memory, in bytes.
- `region` Region index (starting from 0).

**Returns**:

No return value.

**POSIX Compatibility**: openvela/NuttX extension interface.


### kmm_delayfree

```c
void kmm_delayfree(void *mem);
```

Deferred free of kernel heap memory. The free operation is postponed until a safe point, for scenarios in which memory cannot be freed immediately, such as interrupt context or while holding a spinlock.

**Parameters**:

- `mem` Pointer to the kernel heap memory to be freed.

**Returns**:

No return value.

**Note**:

- Use this function instead of `kmm_free()` when freeing memory from an interrupt handler.

**POSIX Compatibility**: openvela/NuttX extension interface.


### kmm_memdump

```c
void kmm_memdump(const struct mm_memdump_s *dump);
```

Dump memory allocation information for the kernel heap to the system log, used for debugging memory leaks.

**Parameters**:

- `dump` Pointer to a dump condition structure specifying filter conditions (PID, sequence number range, etc.).

**Returns**:

No return value.

**Note**:

- Requires `CONFIG_MM_BACKTRACE` to be enabled to obtain allocation backtrace information.

**POSIX Compatibility**: openvela/NuttX extension interface.


### kmm_checkcorruption

```c
void kmm_checkcorruption(void);
```

Check whether the kernel heap has memory corruption.

**Parameters**:

None.

**Returns**:

No return value. If corruption is detected, an assertion is triggered or debug information is printed.

**Note**:

- Requires `CONFIG_MM_HEAP_CHECK` to be enabled.
- Used for debugging memory-related issues.

---




## Heap Management Interfaces


### mm_initialize

```c
struct mm_heap_s *mm_initialize(const char *name, void *heapstart, size_t heapsize);
```

Initialize a new heap. Allocates and initializes the heap management structure and sets up the initial free block.

**Parameters**:

- `name` Name of the heap, used for debugging and identification.
- `heapstart` Start address of the heap memory.
- `heapsize` Size of the heap, in bytes.

**Returns**:

Returns a pointer to the initialized heap structure on success, or `NULL` on failure.

**Note**:

- The heap size must be large enough to accommodate the heap management overhead.
- The system can have multiple independent heaps, such as user heap, kernel heap, graphics heap, and so on.


### mm_uninitialize

```c
void mm_uninitialize(struct mm_heap_s *heap);
```

Destroy a heap, releasing the resources occupied by the heap management structure.

**Parameters**:

- `heap` Pointer to the heap structure to be destroyed.

**Returns**:

No return value.

**Note**:

- Before destruction, ensure there are no live allocations in the heap.


### mm_addregion

```c
void mm_addregion(struct mm_heap_s *heap, void *heapstart, size_t heapsize);
```

Add a new memory region to an existing heap. Allows the heap to use non-contiguous memory regions.

**Parameters**:

- `heap` Pointer to the heap structure.
- `heapstart` Start address of the new memory region.
- `heapsize` Size of the new memory region, in bytes.

**Returns**:

No return value.

**Note**:

- The added region may be physically non-contiguous with existing regions.
- The maximum number of regions is configured by `CONFIG_MM_REGIONS`.


### mm_extend

```c
void mm_extend(struct mm_heap_s *heap, void *mem, size_t size, int region);
```

Extend a specific memory region of the heap. The additional memory must be adjacent to the existing region.

**Parameters**:

- `heap` Pointer to the heap structure.
- `mem` Start address of the additional memory region.
- `size` Size of the additional memory region, in bytes.
- `region` Region index (starting from 0).

**Returns**:

No return value.

**Note**:

- The added memory region must be physically adjacent to the existing region.
- Unlike `mm_addregion()`, this function extends an existing region instead of adding a new one.


### mm_brkaddr

```c
void *mm_brkaddr(struct mm_heap_s *heap, int region);
```

Get the current break address (end address of the region) of a specific region of the heap.

**Parameters**:

- `heap` Pointer to the heap structure.
- `region` Region index (starting from 0).

**Returns**:

Returns the current break address of the specified region.

**Note**:

- Used to determine the memory location to which the region can be extended.


### mm_sbrk

```c
int mm_sbrk(struct mm_heap_s *heap, intptr_t incr, void **mem);
```

Extend or shrink the break address of the heap (similar to UNIX sbrk semantics).

**Parameters**:

- `heap` Pointer to the heap structure.
- `incr` Increment (positive to expand, negative to shrink).
- `mem` Output parameter; returns the previous break address.

**Returns**:

Returns 0 on success, -1 on failure.

**POSIX Compatibility**: openvela/NuttX extension interface.


### mm_heapmember

```c
bool mm_heapmember(struct mm_heap_s *heap, void *mem);
```

Check whether a memory address belongs to any region of the specified heap.

**Parameters**:

- `heap` Pointer to the heap structure.
- `mem` Memory address to check.

**Returns**:

Returns `true` if the address belongs to the specified heap; otherwise `false`.

**Note**:

- Used to determine where an allocation came from so that the correct interface is used to free it.


### mm_free

```c
void mm_free(struct mm_heap_s *heap, void *mem);
```

Free memory from the specified heap. This is the low-level heap release interface; both `free()` and `kmm_free()` call it internally.

**Parameters**:

- `heap` Pointer to the heap structure.
- `mem` Pointer to the memory block to be freed.

**Returns**:

No return value.

**POSIX Compatibility**: openvela/NuttX extension interface.


### mm_malloc_size

```c
size_t mm_malloc_size(struct mm_heap_s *heap, void *mem);
```

Return the actual usable size of an allocated block in the specified heap.

**Parameters**:

- `heap` Pointer to the heap structure.
- `mem` Pointer to an allocated memory block.

**Returns**:

Returns the actual usable size of the block, in bytes.

**POSIX Compatibility**: openvela/NuttX extension interface.


### mm_delayfree

```c
void mm_delayfree(struct mm_heap_s *heap, void *mem);
```

Deferred free of memory from the specified heap. Applicable to scenarios in which memory cannot be freed immediately, such as interrupt context or while holding a spinlock.

**Parameters**:

- `heap` Pointer to the heap structure.
- `mem` Pointer to the memory block to be freed.

**Returns**:

No return value.

**POSIX Compatibility**: openvela/NuttX extension interface.


### mm_heapfree

```c
size_t mm_heapfree(struct mm_heap_s *heap);
```

Query the total amount of free memory in the specified heap.

**Parameters**:

- `heap` Pointer to the heap structure.

**Returns**:

Returns the total size of free memory in the heap, in bytes.

**POSIX Compatibility**: openvela/NuttX extension interface.


### mm_heapfree_largest

```c
size_t mm_heapfree_largest(struct mm_heap_s *heap);
```

Query the size of the largest contiguous free block in the specified heap. This determines the largest memory that a single allocation can obtain.

**Parameters**:

- `heap` Pointer to the heap structure.

**Returns**:

Returns the size of the largest contiguous free block, in bytes.

**POSIX Compatibility**: openvela/NuttX extension interface.


### mm_notify_pressure

```c
void mm_notify_pressure(size_t remaining, size_t largest);
```

Send a memory pressure notification. When heap free memory falls below a threshold, this notifies registered listeners to release caches and other reclaimable memory.

**Parameters**:

- `remaining` Current amount of remaining free memory, in bytes.
- `largest` Current size of the largest contiguous free block, in bytes.

**Returns**:

No return value.

**POSIX Compatibility**: openvela/NuttX extension interface.


## User Heap Interfaces


### umm_initialize

```c
void umm_initialize(void *heap_start, size_t heap_size);
```

Initialize the user heap. This function is usually called during system startup by initialization code.

**Parameters**:

- `heap_start` Start address of the heap memory.
- `heap_size` Size of the heap, in bytes.

**Returns**:

No return value.

**Note**:

- This function should be called only once.
- The user heap is the default heap for standard interfaces such as `malloc()`.


### umm_heapmember

```c
bool umm_heapmember(void *mem);
```

Check whether a memory address belongs to the user heap.

**Parameters**:

- `mem` Memory address to check.

**Returns**:

Returns `true` if the address belongs to the user heap; otherwise `false`.

**Note**:

- Combined with `kmm_heapmember()`, can be used to determine the origin of a memory block.

---




### umm_addregion

```c
void umm_addregion(void *heapstart, size_t heapsize);
```

Add a new memory region to the user heap.

**Parameters**:

- `heapstart` Start address of the new memory region.
- `heapsize` Size of the new memory region, in bytes.

**Returns**:

No return value.

**POSIX Compatibility**: openvela/NuttX extension interface.


### umm_extend

```c
void umm_extend(void *mem, size_t size, int region);
```

Extend a specific memory region of the user heap. The additional memory must be adjacent to the existing region.

**Parameters**:

- `mem` Start address of the additional memory.
- `size` Size of the additional memory, in bytes.
- `region` Region index.

**Returns**:

No return value.

**POSIX Compatibility**: openvela/NuttX extension interface.


### umm_delayfree

```c
void umm_delayfree(void *mem);
```

Deferred free of user heap memory. Applicable in interrupt context.

**Parameters**:

- `mem` Pointer to the user heap memory to be freed.

**Returns**:

No return value.

**POSIX Compatibility**: openvela/NuttX extension interface.


## Debugging and Diagnostics


### mm_memdump

```c
void mm_memdump(struct mm_heap_s *heap, const struct mm_memdump_s *dump);
```

Dump memory allocation information of a heap, used for debugging memory leaks and analyzing memory usage.

**Parameters**:

- `heap` Pointer to the heap structure. If `NULL`, the user heap is dumped.
- `dump` Pointer to a `struct mm_memdump_s` structure specifying the dump conditions.

**Returns**:

No return value.

**Note**:

- `mm_memdump_s` is a type alias for `malltask`; the structure fields are the same as the `malltask` described in `mallinfo_task()`.
- The output includes, for each allocated block, the address, size, allocator PID, and the allocation backtrace (if `CONFIG_MM_BACKTRACE` is enabled).
- Commonly used to diagnose memory leaks and to identify which code allocated memory but did not free it.


### mm_checkcorruption

```c
void mm_checkcorruption(struct mm_heap_s *heap);
```

Check whether the heap has memory corruption. This function traverses all memory blocks in the heap and verifies their integrity.

**Parameters**:

- `heap` Pointer to the heap structure to be checked.

**Returns**:

No return value. If corruption is detected, an assertion is triggered or debug information is printed.

**Note**:

- Requires `CONFIG_DEBUG_MM` to be enabled.
- Issues detected include: block header corruption, double free, out-of-bounds writes, and so on.
- This function traverses the entire heap and has a performance impact; it is mainly used for debugging.


### umm_checkcorruption

```c
void umm_checkcorruption(void);
```

Check whether the user heap has memory corruption. This is a convenience wrapper around `mm_checkcorruption()` for the user heap.

**Parameters**:

None.

**Returns**:

No return value. If corruption is detected, an assertion is triggered or debug information is printed.

**Note**:

- Requires `CONFIG_DEBUG_MM` to be enabled.
- This function can be called to perform a check when memory issues are suspected.


### umm_memdump

```c
void umm_memdump(const struct mm_memdump_s *dump);
```

Dump memory allocation information of the user heap to the system log.

**Parameters**:

- `dump` Pointer to the dump condition structure.

**Returns**:

No return value.

**Note**:

- Requires `CONFIG_MM_BACKTRACE` to be enabled to obtain backtrace information.

**POSIX Compatibility**: openvela/NuttX extension interface.
