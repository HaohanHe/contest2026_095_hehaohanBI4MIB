\[ English | [简体中文](../../../zh-cn/api/kernel/thread.md) \]

# Thread API

openvela provides POSIX-compatible thread (pthread) interfaces, supporting thread creation, synchronization, and attribute management.

Header file: `#include <pthread.h>`

## openvela Implementation Notes

openvela's pthread implementation is based on the NuttX RTOS kernel and differs from the standard Linux implementation in the following ways:

- **`pthread_t` is `pid_t`**: In openvela, the thread ID underlying type is `pid_t` (process ID), and can be passed directly to system calls such as `kill()`.
- **No process isolation**: openvela does not support Linux-style processes; all threads run in the same address space. The `PTHREAD_PROCESS_SHARED` attribute can be set but behaves the same as `PTHREAD_PROCESS_PRIVATE`.
- **Fixed contention scope**: Only `PTHREAD_SCOPE_SYSTEM` is supported; `PTHREAD_SCOPE_PROCESS` returns `ENOTSUP`.
- **Limited `fork()` support**: The `pthread_atfork()` interface exists, but `fork()` may be unavailable in an RTOS environment; it is mainly for POSIX compatibility.
- **Conditional compilation dependencies**: Some features require specific configuration options:
  - `CONFIG_SMP`: CPU affinity interfaces (`pthread_setaffinity_np`, etc.)
  - `CONFIG_PRIORITY_INHERITANCE`: Priority inheritance protocol (`PTHREAD_PRIO_INHERIT`)
  - `CONFIG_PRIORITY_PROTECT`: Priority ceiling protection (`PTHREAD_PRIO_PROTECT`, `prioceiling`-related interfaces)
  - `CONFIG_RR_INTERVAL > 0`: `SCHED_RR` scheduling policy
  - `CONFIG_SCHED_SPORADIC`: `SCHED_SPORADIC` scheduling policy


## Thread Creation and Management


### pthread_create

```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   pthread_startroutine_t start_routine, pthread_addr_t arg);
```

> **Type notes**: `pthread_startroutine_t` is equivalent to `void *(*)(void *)`, and `pthread_addr_t` is equivalent to `void *`; both are NuttX type aliases.

Creates a new thread and makes it runnable. The new thread begins execution from the `start_routine` function, which receives `arg` as its sole argument. The thread attribute object `attr` specifies various thread attributes such as stack size, scheduling policy, and priority.

If `attr` is `NULL`, the thread is created with default attributes. By default, threads are joinable with a default stack size and scheduling policy.

**Parameters**:

- `thread` Pointer to a `pthread_t` in which the new thread's ID is stored on success.
- `attr` Pointer to a thread attribute object. If `NULL`, default attributes are used (stack size `PTHREAD_STACK_DEFAULT`, scheduling policy `SCHED_OTHER`, joinable).
- `start_routine` Thread entry function with signature `void *(*)(void *)`. The new thread starts from this function.
- `arg` Argument passed to the entry function. To pass multiple arguments, pass a pointer to a struct.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EAGAIN` Insufficient system resources to create a new thread, or the system thread count limit has been reached.
- `EINVAL` Settings in `attr` are invalid.
- `EPERM` No permission to set the specified scheduling policy or parameters.

**Notes**:

- The new thread shares the same address space, file descriptors, and signal handling as the calling thread.
- If the thread is created with `PTHREAD_CREATE_DETACHED`, resources are released automatically upon termination, without needing `pthread_join()`.
- The thread becomes schedulable immediately after creation; creation order does not guarantee execution order.
- The thread's return value can be obtained via `pthread_join()`, or returned explicitly via `pthread_exit()`.
- Ensure arguments passed to the thread remain valid throughout the thread's execution; avoid passing the address of stack-local variables.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_exit

```c
void pthread_exit(void *exit_value);
```

Terminates the calling thread and returns a value that other threads can retrieve via `pthread_join()`. This function does not return to the caller.

Calling `pthread_exit()` is equivalent to returning from the thread entry function but can be called from any function invoked by the thread. On termination, the following cleanup actions are performed:

1. Cleanup functions registered via `pthread_cleanup_push()` are called (in reverse registration order).
2. Destructors for thread-specific data are called (for every non-`NULL` thread-specific data key).
3. If the thread is joinable, the thread ID and return value are retained until another thread calls `pthread_join()`.
4. If the thread is detached, all resources are released immediately.

**Parameters**:

- `exit_value` Thread return value, an untyped pointer that can carry the address of arbitrary data. `pthread_join()` on this thread can obtain this value. If the thread is canceled, the return value is `PTHREAD_CANCELED`.

**Returns**:

This function does not return. After calling, the calling thread terminates.

**Notes**:

- Do not call `pthread_exit()` in `main()`; it terminates the main thread but not the process, which may orphan other threads.
- If the thread is detached, `exit_value` is ignored, because no thread can obtain it via `pthread_join()`.
- Thread termination does not automatically close open file descriptors or release allocated memory, which are shared by the whole process.
- Returning from the thread entry function implicitly calls `pthread_exit()` with the return value as the exit value.
- If the thread holds mutexes, release them before calling `pthread_exit()`, otherwise deadlock may occur.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_join

```c
int pthread_join(pthread_t thread, pthread_addr_t *value);
```

> **Type notes**: `pthread_addr_t` is equivalent to `void *`, a NuttX type alias.

Blocks the calling thread until the specified `thread` terminates. If the thread has already terminated, `pthread_join()` returns immediately. On success, the target thread is "joined" and its resources are reclaimed.

Each joinable thread can be joined only once. Joining a thread that has already been joined or detached causes undefined behavior. A thread cannot join itself; doing so causes deadlock.

**Parameters**:

- `thread` ID of the thread to wait for. Must be joinable; cannot be a detached thread or a thread that has already been joined.
- `value` Pointer to a pointer that receives the thread's return value. If non-`NULL`, the target thread's return value (from `pthread_exit()` or the entry function return) is stored in `*value`. If the thread was canceled, `*value` is set to `PTHREAD_CANCELED`. Pass `NULL` if the return value is not needed.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EDEADLK` A deadlock was detected (e.g., a thread attempting to join itself), or `thread` specifies a thread that is waiting to join the calling thread.
- `EINVAL` `thread` is not joinable, or another thread is already waiting to join it.
- `ESRCH` No thread with ID `thread` can be found.

**Notes**:

- `pthread_join()` blocks the caller until the target thread terminates. If the target has already terminated, the call returns immediately.
- After joining, the thread ID is reclaimed and should not be used again.
- If multiple threads attempt to join the same thread simultaneously, the behavior is undefined.
- Failing to join a joinable thread leaks resources (similar to a memory leak); the thread's resources are not reclaimed.
- For threads whose return value is not needed, set them detached at creation or call `pthread_detach()` afterward.
- In openvela, the thread ID is actually a process ID (`pid_t`) and can be used with other system calls.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_detach

```c
int pthread_detach(pthread_t thread);
```

Marks the specified thread as detached. A detached thread releases all resources automatically upon termination, without needing another thread to call `pthread_join()`. Once detached, the thread cannot be joined, and its return value cannot be retrieved.

A thread can be made detached in two ways:
1. By setting `PTHREAD_CREATE_DETACHED` in the attribute object at creation.
2. By calling `pthread_detach()` after creation.

A thread can also detach itself via `pthread_detach(pthread_self())`.

**Parameters**:

- `thread` ID of the thread to detach. Can be another thread's ID or the calling thread's own ID (via `pthread_self()`).

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `thread` is not joinable (possibly already detached).
- `ESRCH` No thread with ID `thread` can be found.

**Notes**:

- After detaching, `pthread_join()` cannot be called for this thread; doing so returns an error.
- Detached state is irreversible; a detached thread cannot become joinable again.
- For threads whose return value or completion is not needed, set them detached to avoid resource leaks.
- Detaching does not affect thread execution, only how resources are reclaimed after termination.
- Calling `pthread_detach()` again on an already-detached thread returns `EINVAL`.
- The main thread can be detached, but this is usually not good practice because main thread termination ends the entire process.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_cancel

```c
int pthread_cancel(pthread_t thread);
```

Sends a cancellation request to the specified thread. Whether the thread responds depends on its cancel state (set via `pthread_setcancelstate()`) and cancel type (set via `pthread_setcanceltype()`).

If the cancellation request is delivered successfully, the target thread's state and type determine when and how the cancellation is processed:

- If the cancel state is `PTHREAD_CANCEL_DISABLE`, the request is pending until the state becomes `PTHREAD_CANCEL_ENABLE`.
- If the state is `PTHREAD_CANCEL_ENABLE` and the type is `PTHREAD_CANCEL_DEFERRED`, cancellation occurs at the next cancellation point.
- If the type is `PTHREAD_CANCEL_ASYNCHRONOUS`, cancellation may occur immediately (but may also be deferred).

**Parameters**:

- `thread` ID of the thread to cancel. Cannot cancel self (use `pthread_exit()`).

**Returns**:

Returns 0 on success, or an error code on failure:

- `ESRCH` No thread with ID `thread` can be found.

**Notes**:

- `pthread_cancel()` only sends a request; it does not wait for the thread to actually terminate.
- A canceled thread's exit value is `PTHREAD_CANCELED`.
- Asynchronous cancellation (`PTHREAD_CANCEL_ASYNCHRONOUS`) is dangerous and should be used only in specific cases, because the thread may be canceled at any point, possibly causing resource leaks or data inconsistency.
- Deferred cancellation (`PTHREAD_CANCEL_DEFERRED`, the default) is safer; the thread is canceled only at cancellation points, including `pthread_testcancel()`, `pthread_join()`, `pthread_cond_wait()`, and other blocking calls.
- Cleanup handlers (registered via `pthread_cleanup_push()`) and thread-specific data destructors are run when a thread is canceled.
- If the thread holds locks or other resources, use cleanup handlers to ensure proper release.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_setcancelstate

```c
int pthread_setcancelstate(int state, int *oldstate);
```

Sets the calling thread's cancel state, controlling whether it can be canceled.

**Parameters**:

- `state` New cancel state. Valid values:
  - `PTHREAD_CANCEL_ENABLE` Cancellation enabled (default). The thread can respond to cancellation requests.
  - `PTHREAD_CANCEL_DISABLE` Cancellation disabled. Requests are held pending until the state becomes enabled.
- `oldstate` If non-`NULL`, stores the previous cancel state. Pass `NULL` if not needed.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `state` is not a valid cancel state.

**Notes**:

- Newly created threads default to `PTHREAD_CANCEL_ENABLE`.
- Disabling cancellation does not discard pending requests; it only defers processing.
- Even with cancellation disabled, the thread can still terminate via `pthread_exit()`.
- When executing critical sections (such as resource allocation and initialization), temporarily disable cancellation and re-enable it when done.
- The cancel state is thread-local; each thread has its own independent state.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_setcanceltype

```c
int pthread_setcanceltype(int type, int *oldtype);
```

Sets the calling thread's cancel type, controlling how it responds to cancellation requests.

**Parameters**:

- `type` New cancel type. Valid values:
  - `PTHREAD_CANCEL_DEFERRED` Deferred cancellation (default). Requests are handled at the next cancellation point. Cancellation points include blocking functions such as `pthread_testcancel()`, `pthread_join()`, `pthread_cond_wait()`, `pthread_cond_timedwait()`, and `sem_wait()`.
  - `PTHREAD_CANCEL_ASYNCHRONOUS` Asynchronous cancellation. The thread may be canceled at any time (actual behavior is implementation-dependent). This mode is dangerous and should be avoided.
- `oldtype` If non-`NULL`, stores the previous cancel type. Pass `NULL` if not needed.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `type` is not a valid cancel type.

**Notes**:

- Newly created threads default to `PTHREAD_CANCEL_DEFERRED`.
- Deferred cancellation is the recommended type because it only responds at well-defined cancellation points, ensuring the thread is in a known state.
- Asynchronous cancellation may interrupt the thread at any instruction, possibly causing resource leaks, data corruption, or undefined behavior. Use it only when you are sure the thread code is async-cancel-safe.
- In asynchronous cancellation, do not call non-async-cancel-safe functions, including most library functions.
- The cancel type is thread-local.
- Even if asynchronous cancel is set, the implementation may treat it as deferred.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_testcancel

```c
void pthread_testcancel(void);
```

Creates a cancellation point. If there is a pending cancellation request and the cancel state is enabled, the thread is canceled and does not return. This explicitly checks and responds to cancellation requests.

Cancellation points are where a thread can respond to cancellation requests. POSIX defines some functions (such as `pthread_join()` and `sem_wait()`) as required cancellation points, and `pthread_testcancel()` allows creating one at any point.

**Parameters**:

None.

**Returns**:

If there is no pending cancellation request, the function returns normally. If there is one, the function does not return and the thread is canceled.

**Notes**:

- If the cancel state is `PTHREAD_CANCEL_DISABLE`, `pthread_testcancel()` has no effect and returns directly.
- Typically used in long-running, computation-intensive code to provide opportunities to respond to cancellation requests.
- Call `pthread_testcancel()` periodically at appropriate locations (e.g., inside loops) to ensure timely response.
- If the thread is canceled, cleanup handlers and thread-specific data destructors run.
- Calling too frequently may impact performance; call at logically appropriate locations.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_self

```c
pthread_t pthread_self(void);
```

Gets the calling thread's ID. The ID uniquely identifies a thread within a process and can be used with functions such as `pthread_join()`, `pthread_detach()`, and `pthread_equal()`.

In openvela, `pthread_t` is actually a `pid_t`, i.e., the thread ID is the same as the process ID.

**Parameters**:

None.

**Returns**:

Returns the calling thread's ID. This function always succeeds.

**Notes**:

- Thread IDs remain constant for the lifetime of the thread.
- After termination and joining, thread IDs may be reused.
- Do not rely on the numeric value or order of thread IDs; they are opaque identifiers.
- `pthread_detach(pthread_self())` can be used to detach the current thread.
- In openvela, thread IDs can be used with system calls such as sending signals.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_equal

```c
int pthread_equal(pthread_t t1, pthread_t t2);
```

Compares two thread IDs for equality. Because the internal representation of a thread ID may be a complex data structure, POSIX requires this function instead of direct `==` comparison.

**Parameters**:

- `t1` First thread ID.
- `t2` Second thread ID.

**Returns**:

Returns nonzero if the two thread IDs are equal; 0 otherwise.

**Notes**:

- In openvela, `pthread_t` is a simple integer type (`pid_t`) and can be compared directly with `==`; however, for portability, use `pthread_equal()`.
- Terminated thread IDs may be reused, so ID uniqueness across thread lifetimes should not be assumed.
- Commonly used to check whether a thread ID is the current thread: `pthread_equal(thread_id, pthread_self())`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_yield

```c
void pthread_yield(void);
```

Voluntarily yields the processor to allow other threads to run. This is an optimization hint; actual behavior depends on the scheduling policy.

After calling `pthread_yield()`, the calling thread is placed at the end of its priority queue, and the scheduler selects the next runnable thread. If no other ready thread of equal or higher priority exists, the calling thread may continue immediately.

**Parameters**:

None.

**Returns**:

No return value.

**Notes**:

- This is a non-standard extension interface, but is available on many systems (Linux, BSD, etc.).
- In POSIX, use `sched_yield()` instead.
- Suitable for cooperative multitasking, where a thread voluntarily yields to others.
- Do not rely on `pthread_yield()` to solve synchronization problems; use appropriate synchronization primitives (mutexes, condition variables).
- Overuse may hurt performance; use only when clearly needed.
- In real-time systems, the behavior depends on the scheduling policy (FIFO, RR, etc.).

**POSIX Compatibility**: Compatible extension interface (non-POSIX standard, but widely supported).


### pthread_once

```c
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));
```

Ensures that the initialization function `init_routine` is called only once during the process lifetime, regardless of how many threads call `pthread_once()`. This is a thread-safe one-time initialization mechanism, commonly used for lazy initialization of global resources.

`once_control` must be a static or global variable initialized with `PTHREAD_ONCE_INIT`. Multiple threads can call `pthread_once()` simultaneously, but `init_routine` runs only once; other threads block until initialization completes.

**Parameters**:

- `once_control` Control variable that tracks initialization state. Must be initialized with `PTHREAD_ONCE_INIT` (e.g., `pthread_once_t once = PTHREAD_ONCE_INIT;`).
- `init_routine` Initialization function with no parameters and no return value. Called only once during the process lifetime.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `once_control` or `init_routine` is `NULL`.

**Notes**:

- `pthread_once()` is thread-safe and can be called simultaneously from multiple threads.
- The init routine should complete quickly to avoid blocking other threads waiting for initialization.
- If the init routine calls `pthread_once()` with the same `once_control`, behavior is undefined (possible deadlock).
- The init routine should not call `pthread_exit()` or be canceled; otherwise initialization is considered incomplete, and the next call runs the init routine again.
- Commonly used for singletons, global resource initialization, etc.
- `once_control` should not be modified directly; only via `pthread_once()`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_atfork

```c
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
```

Registers handlers to be executed around `fork()` calls. These handlers address issues that `fork()` introduces in multi-threaded environments, particularly lock state consistency.

When `fork()` is called, handlers are executed in the following order:
1. Before `fork()`, all `prepare` handlers are called in the parent process (in reverse registration order).
2. `fork()` creates the child process.
3. All `child` handlers are called in the child process (in registration order).
4. All `parent` handlers are called in the parent process (in registration order).

**Parameters**:

- `prepare` Called in the parent before `fork()`. Typically acquires all locks to ensure a consistent state. May be `NULL`.
- `parent` Called in the parent after `fork()`. Typically releases locks acquired in `prepare`. May be `NULL`.
- `child` Called in the child after `fork()`. Typically re-initializes lock state and other resources. May be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `ENOMEM` Insufficient memory to record the handlers.

**Notes**:

- Multiple calls to `pthread_atfork()` may register multiple handler groups.
- `prepare` handlers run in reverse registration order; `parent` and `child` handlers run in registration order.
- Using `fork()` in multithreaded programs is dangerous: the child inherits only the calling thread, but other threads' lock states are inherited, possibly causing deadlock.
- Handlers should run quickly and avoid calling functions that may block or use locks.
- In the child, only async-signal-safe functions should be called (such as the `exec()` family).
- As an RTOS, openvela may have limited or no `fork()` support; this interface is mainly for compatibility.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Thread Attributes


### pthread_attr_init

```c
int pthread_attr_init(pthread_attr_t *attr);
```

Initializes a thread attribute object to default values. Attribute objects specify various thread attributes at creation time, such as stack size, scheduling policy, priority, and detach state.

After initialization, the attribute object contains the following defaults:
- Detach state: `PTHREAD_CREATE_JOINABLE`
- Stack size: `PTHREAD_STACK_DEFAULT` (system default)
- Scheduling policy: `SCHED_OTHER` (or the system default)
- Scheduling inheritance: `PTHREAD_INHERIT_SCHED`
- Scope: `PTHREAD_SCOPE_SYSTEM`

**Parameters**:

- `attr` Pointer to the attribute object to initialize.

**Returns**:

Returns 0 on success, or an error code on failure:

- `ENOMEM` Insufficient memory to initialize the attribute object.

**Notes**:

- After initialization, individual attributes can be modified via the `pthread_attr_set*()` functions.
- The same attribute object can be used to create multiple threads.
- When no longer needed, destroy the attribute object via `pthread_attr_destroy()` to release resources.
- Modifications to the attribute object do not affect threads already created; they only affect subsequent threads created using this object.
- In openvela, the attribute object is a simple struct that does not involve dynamic memory allocation.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_destroy

```c
int pthread_attr_destroy(pthread_attr_t *attr);
```

Destroys a thread attribute object, releasing its resources. A destroyed attribute object cannot be used again unless re-initialized.

**Parameters**:

- `attr` Pointer to the attribute object to destroy.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is not a valid attribute object.

**Notes**:

- Destroying an attribute object does not affect threads already created using it.
- A destroyed attribute object can be re-initialized via `pthread_attr_init()` and reused.
- In openvela, attribute objects typically do not involve dynamic memory; this function is mainly for POSIX compatibility.
- Always pair `pthread_attr_init()` with `pthread_attr_destroy()` following resource-management best practices.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setdetachstate

```c
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
```

Sets the detach state in a thread attribute object. The detach state determines how resources are reclaimed after the thread terminates.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `detachstate` Detach state. Valid values:
  - `PTHREAD_CREATE_JOINABLE` Joinable (default); requires another thread to call `pthread_join()` to reclaim resources.
  - `PTHREAD_CREATE_DETACHED` Detached; resources are released automatically upon termination.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `detachstate` is not a valid value.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_getdetachstate

```c
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
```

Gets the detach state from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `detachstate` Pointer to an integer variable for storing the detach state. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `detachstate` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setstacksize

```c
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
```

Sets the stack size in a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `stacksize` Stack size (bytes). Cannot be less than `PTHREAD_STACK_MIN`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `stacksize` is less than `PTHREAD_STACK_MIN`.

**Notes**:

- Set the stack size based on the thread's actual needs; too small may cause stack overflow.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_getstacksize

```c
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);
```

Gets the stack size from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object.
- `stacksize` Pointer to a `size_t` for storing the stack size. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `stacksize` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setstackaddr

```c
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr);
```

Sets the stack address in a thread attribute object. Allows the application to specify pre-allocated stack memory for the thread.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `stackaddr` Starting address of the stack memory. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `stackaddr` is `NULL`.

**Notes**:

- This interface is marked obsolete by POSIX; use `pthread_attr_setstack()` instead, which sets both stack address and stack size.
- When using a custom stack, the application is responsible for allocating and freeing the stack memory.
- Stack memory must remain valid for the thread's lifetime.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name (obsolete).


### pthread_attr_getstackaddr

```c
int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr);
```

Gets the stack address from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `stackaddr` Pointer to a pointer variable for storing the stack address. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `stackaddr` is `NULL`.

**Notes**:

- This interface is marked obsolete by POSIX; use `pthread_attr_getstack()` instead.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name (obsolete).


### pthread_attr_setstack

```c
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
```

Sets both stack address and stack size in a thread attribute object. A combined replacement for `pthread_attr_setstackaddr()` and `pthread_attr_setstacksize()`.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `stackaddr` Starting address of the stack memory. Cannot be `NULL`.
- `stacksize` Stack size (bytes). Cannot be less than `PTHREAD_STACK_MIN`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `stackaddr` is `NULL`, or `stacksize` is less than `PTHREAD_STACK_MIN`.

**Notes**:

- When using a custom stack, the application is responsible for allocating and freeing the memory, which must remain valid for the thread's lifetime.
- Stack size should account for local variables, function call depth, etc.
- In openvela, the value of `PTHREAD_STACK_MIN` depends on system configuration.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_getstack

```c
int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize);
```

Gets both stack address and stack size from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `stackaddr` Pointer to a pointer variable for storing the stack address. Cannot be `NULL`.
- `stacksize` Pointer to a `size_t` for storing the stack size. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr`, `stackaddr`, or `stacksize` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setguardsize

```c
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize);
```

Sets the stack guard size in a thread attribute object. The guard region is an inaccessible memory area at the end of the stack that detects stack overflow.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `guardsize` Guard size (bytes). Setting 0 disables stack protection.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**Notes**:

- If `pthread_attr_setstack()` specifies a custom stack, the guard setting may be ignored.
- The actual guard size may be rounded up by the system to a multiple of the page size.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_getguardsize

```c
int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize);
```

Gets the stack guard size from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object.
- `guardsize` Pointer to a `size_t` for storing the guard size. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `guardsize` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setschedpolicy

```c
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
```

Sets the scheduling policy in a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `policy` Scheduling policy. Valid values:
  - `SCHED_OTHER` Default scheduling policy.
  - `SCHED_FIFO` First-in, first-out real-time scheduling.
  - `SCHED_RR` Round-robin real-time scheduling (requires `CONFIG_RR_INTERVAL > 0`).
  - `SCHED_SPORADIC` Sporadic scheduling (requires `CONFIG_SCHED_SPORADIC`).

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `policy` is not a valid scheduling policy.

**Notes**:

- Availability of `SCHED_RR` and `SCHED_SPORADIC` depends on system configuration.



**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_getschedpolicy

```c
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy);
```

Gets the scheduling policy from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `policy` Pointer to an integer variable for storing the scheduling policy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `policy` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setschedparam

```c
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);
```

Sets the scheduling parameters in a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `param` Pointer to the scheduling parameter structure. Cannot be `NULL`. Main fields:
  - `sched_priority` Thread priority.
  - `sched_ss_low_priority` Sporadic scheduling low priority (requires `CONFIG_SCHED_SPORADIC`).
  - `sched_ss_repl_period` Sporadic scheduling replenish period.
  - `sched_ss_init_budget` Sporadic scheduling initial budget.
  - `sched_ss_max_repl` Sporadic scheduling maximum replenishments.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `param` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_getschedparam

```c
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param);
```

Gets the scheduling parameters from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `param` Pointer to a scheduling parameter structure for storing the result. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `param` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setinheritsched

```c
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched);
```

Sets the scheduling inheritance mode in a thread attribute object. Determines whether the new thread inherits the creator's scheduling attributes or uses those explicitly specified in the attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `inheritsched` Inheritance mode. Valid values:
  - `PTHREAD_INHERIT_SCHED` Inherit the creating thread's scheduling policy and parameters (default).
  - `PTHREAD_EXPLICIT_SCHED` Use the scheduling policy and parameters set in the attribute object.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `inheritsched` is not a valid value.

**Notes**:

- When set to `PTHREAD_EXPLICIT_SCHED`, also set the scheduling policy and parameters via `pthread_attr_setschedpolicy()` and `pthread_attr_setschedparam()`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_getinheritsched

```c
int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched);
```

Gets the scheduling inheritance mode from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `inheritsched` Pointer to an integer variable for storing the inheritance mode. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `inheritsched` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setscope

```c
int pthread_attr_setscope(pthread_attr_t *attr, int scope);
```

Sets the contention scope in a thread attribute object. The contention scope defines which threads the thread competes with for resources such as CPU.

**Parameters**:

- `attr` Pointer to the thread attribute object.
- `scope` Contention scope. Valid values:
  - `PTHREAD_SCOPE_SYSTEM` System-level contention; the thread competes with all threads in the system.
  - `PTHREAD_SCOPE_PROCESS` Process-level contention; the thread competes only with threads in the same process.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `scope` is not a valid value.
- `ENOTSUP` Requested contention scope is not supported. In openvela, `PTHREAD_SCOPE_PROCESS` is not supported.

**Notes**:

- openvela only supports `PTHREAD_SCOPE_SYSTEM`, which is also the default. Passing `PTHREAD_SCOPE_PROCESS` returns `ENOTSUP`.
- In an RTOS environment, all threads naturally contend at the system level.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_getscope

```c
int pthread_attr_getscope(const pthread_attr_t *attr, int *scope);
```

Gets the contention scope from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object.
- `scope` Pointer to an integer variable for storing the contention scope.

**Returns**:

Returns 0 on success.

**Notes**:

- In openvela, always returns `PTHREAD_SCOPE_SYSTEM`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_attr_setaffinity_np

```c
int pthread_attr_setaffinity_np(pthread_attr_t *attr, size_t cpusetsize, const cpu_set_t *cpuset);
```

Sets the CPU affinity mask in a thread attribute object. Threads created with this attribute are limited to running on the specified CPU set.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `cpusetsize` Size (in bytes) of the `cpuset` buffer; must be `sizeof(cpu_set_t)`.
- `cpuset` Pointer to the CPU set, specifying which CPUs the thread may run on. Cannot be `NULL`, and the set cannot be empty.

**Returns**:

Returns 0 on success.

**Notes**:

- Available only when SMP (`CONFIG_SMP`) is enabled.
- Unlike `pthread_setaffinity_np()`, this function sets the affinity in the attribute object, and it takes effect at `pthread_create()`.
- Use macros such as `CPU_ZERO()` and `CPU_SET()` to manipulate `cpu_set_t`.

**POSIX Compatibility**: Compatible with the Linux extension interface (non-POSIX standard).


### pthread_attr_getaffinity_np

```c
int pthread_attr_getaffinity_np(const pthread_attr_t *attr, size_t cpusetsize, cpu_set_t *cpuset);
```

Gets the CPU affinity mask from a thread attribute object.

**Parameters**:

- `attr` Pointer to the thread attribute object. Cannot be `NULL`.
- `cpusetsize` Size (in bytes) of the `cpuset` buffer; must be `sizeof(cpu_set_t)`.
- `cpuset` Pointer to a CPU set for storing the affinity mask. Cannot be `NULL`.

**Returns**:

Returns 0 on success.

**Notes**:

- Available only when SMP (`CONFIG_SMP`) is enabled.

**POSIX Compatibility**: Compatible with the Linux extension interface (non-POSIX standard).


## Thread Scheduling


### pthread_getschedparam

```c
int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param);
```

Gets the scheduling policy and parameters of the specified thread.

**Parameters**:

- `thread` Thread ID.
- `policy` Pointer to an integer variable for storing the scheduling policy. Cannot be `NULL`.
- `param` Pointer to a scheduling parameter structure for storing the result. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `policy` or `param` is `NULL`.
- `ESRCH` The specified thread cannot be found.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_setschedparam

```c
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param);
```

Sets the scheduling policy and parameters of the specified thread.

**Parameters**:

- `thread` Thread ID.
- `policy` Scheduling policy: `SCHED_FIFO`, `SCHED_RR`, `SCHED_OTHER`, or `SCHED_SPORADIC`.
- `param` Pointer to the scheduling parameter structure.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` Invalid argument.
- `ESRCH` The specified thread cannot be found.
- `EPERM` No permission to modify the scheduling parameters.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_setschedprio

```c
int pthread_setschedprio(pthread_t thread, int prio);
```

Sets the priority of the specified thread without changing the scheduling policy.

**Parameters**:

- `thread` Thread ID.
- `prio` New priority value.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` Priority value is invalid.
- `ESRCH` The specified thread cannot be found.

**Notes**:

- This function modifies only the priority; the current scheduling policy and other parameters (such as sporadic scheduling parameters) are preserved.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_setaffinity_np

```c
int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t *cpuset);
```

Sets the thread's CPU affinity mask. If the thread is not currently running on a CPU specified by `cpuset`, it is migrated to one of them.

**Parameters**:

- `thread` Thread ID.
- `cpusetsize` Size (in bytes) of the `cpuset` buffer; typically `sizeof(cpu_set_t)`.
- `cpuset` Pointer to a CPU set specifying CPUs the thread may run on.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` Invalid argument.
- `ESRCH` The specified thread cannot be found.

**Notes**:

- Available only when SMP (`CONFIG_SMP`) is enabled.
- Use macros such as `CPU_ZERO()` and `CPU_SET()` to manipulate `cpu_set_t`.

**POSIX Compatibility**: Compatible with the Linux extension interface (non-POSIX standard).


### pthread_getaffinity_np

```c
int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, cpu_set_t *cpuset);
```

Gets the thread's CPU affinity mask.

**Parameters**:

- `thread` Thread ID.
- `cpusetsize` Size (in bytes) of the `cpuset` buffer; typically `sizeof(cpu_set_t)`.
- `cpuset` Pointer to a CPU set for storing the thread's affinity mask.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` Invalid argument.
- `ESRCH` The specified thread cannot be found.

**Notes**:

- Available only when SMP (`CONFIG_SMP`) is enabled.

**POSIX Compatibility**: Compatible with the Linux extension interface (non-POSIX standard).


### pthread_setconcurrency

```c
int pthread_setconcurrency(int new_level);
```

Sets the concurrency level hint. This function hints to the system the desired number of concurrent threads; the system may use it to optimize thread scheduling.

**Parameters**:

- `new_level` Desired concurrency level. 0 means let the system decide. Cannot be negative.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `new_level` is negative.

**Notes**:

- This function is merely a hint; the system does not guarantee the actual concurrency level matches the set value.
- In openvela, this value is stored in a global variable and does not affect actual scheduling behavior.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_getconcurrency

```c
int pthread_getconcurrency(void);
```

Gets the current concurrency level hint value.

**Parameters**:

None.

**Returns**:

Returns the value previously set via `pthread_setconcurrency()`. Returns 0 if never set.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Mutexes


### pthread_mutex_init

```c
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
```

Initializes a mutex. Mutexes protect shared resources, ensuring that only one thread accesses the protected critical section at a time.

If `attr` is `NULL`, the mutex uses default attributes: type `PTHREAD_MUTEX_NORMAL`, no priority inheritance or protection protocol, and process-private.

A mutex can also be initialized statically with `PTHREAD_MUTEX_INITIALIZER`:
```c
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
```

**Parameters**:

- `mutex` Pointer to the mutex object to initialize.
- `attr` Pointer to a mutex attribute object. If `NULL`, default attributes are used. Attributes include mutex type (normal, recursive, errorcheck), priority protocol, robustness, and more.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EAGAIN` Insufficient system resources to initialize the mutex.
- `ENOMEM` Insufficient memory.
- `EPERM` Caller lacks permission.
- `EINVAL` Invalid attribute values in `attr`.

**Notes**:

- After initialization, the mutex is unlocked.
- Do not re-initialize an already-initialized mutex; this causes undefined behavior.
- When no longer needed, destroy the mutex via `pthread_mutex_destroy()`.
- Statically initialized mutexes do not require explicit destruction.
- Mutex type affects repeated-lock and error-detection behavior:
  - `PTHREAD_MUTEX_NORMAL`: No deadlock detection; repeated locking causes deadlock.
  - `PTHREAD_MUTEX_ERRORCHECK`: Detects deadlocks and errors; slightly lower performance.
  - `PTHREAD_MUTEX_RECURSIVE`: Allows the same thread to lock multiple times; requires an equal number of unlocks.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutex_destroy

```c
int pthread_mutex_destroy(pthread_mutex_t *mutex);
```

Destroys a mutex and releases its resources.

**Parameters**:

- `mutex` Pointer to the mutex to destroy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `mutex` is `NULL` or not properly initialized.
- `EBUSY` The mutex is currently locked; cannot destroy.

**Notes**:

- A locked mutex cannot be destroyed.
- Once destroyed, the mutex cannot be reused unless re-initialized.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutex_lock

```c
int pthread_mutex_lock(pthread_mutex_t *mutex);
```

Locks a mutex. If the mutex is currently unlocked, the calling thread acquires it and returns immediately. If another thread holds the lock, the calling thread blocks until it becomes available.

The specific behavior depends on the mutex type:
- **NORMAL (default)**: If the lock is already held by the current thread, relocking deadlocks.
- **ERRORCHECK**: If the lock is already held by the current thread, returns `EDEADLK`.
- **RECURSIVE**: If the lock is already held by the current thread, increments the lock count; requires an equal number of unlocks.

**Parameters**:


- `mutex` Pointer to the mutex to lock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EDEADLK` Mutex is `PTHREAD_MUTEX_ERRORCHECK` and the current thread already holds it (deadlock detection).
- `EINVAL` Mutex is not properly initialized.
- `EOWNERDEAD` The mutex is robust and the previous owner terminated without releasing it. The caller now owns the mutex; call `pthread_mutex_consistent()` to make it consistent, or unlock and no longer use it.
- `ENOTRECOVERABLE` The robust mutex is in an unrecoverable state and can no longer be used.

**Notes**:

- Threads holding locks should release them as soon as possible to avoid long waits by other threads.
- Avoid calling potentially blocking functions (e.g., I/O operations) while holding a lock; this can cause performance issues.
- Avoid nested locking of multiple mutexes, which can cause deadlock. If nested locking is required, use a consistent lock order.
- After locking, use a `try-finally` pattern or cleanup handlers to ensure the lock is always released.
- If the thread is canceled, use cleanup handlers (`pthread_cleanup_push/pop`) to ensure the lock is released.
- Priority inversion: If priority inheritance is enabled, a low-priority thread holding the lock has its priority temporarily raised to the highest priority among waiters.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutex_trylock

```c
int pthread_mutex_trylock(pthread_mutex_t *mutex);
```

Tries to lock a mutex (non-blocking). If the mutex is currently available, the function acquires the lock and returns immediately. If the mutex is already locked, returns `EBUSY` immediately without blocking.

This is the non-blocking version of `pthread_mutex_lock()`, suitable for scenarios where waiting is undesirable, such as polling or deadlock avoidance.

**Parameters**:

- `mutex` Pointer to the mutex to try to lock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EBUSY` The mutex is already locked and cannot be acquired. This is not a real error; it just indicates the lock is currently unavailable.
- `EINVAL` The mutex is not properly initialized.
- `EDEADLK` Mutex is `PTHREAD_MUTEX_ERRORCHECK` and the current thread already holds it.
- `EOWNERDEAD` The robust mutex's previous owner terminated without releasing it.
- `ENOTRECOVERABLE` The robust mutex is in an unrecoverable state.

**Notes**:

- If `EBUSY` is returned, the caller may retry later or perform other actions.
- For recursive mutexes, if the current thread already holds the lock, `pthread_mutex_trylock()` succeeds and increments the lock count.
- Commonly used to avoid deadlock: when trying to acquire multiple locks, release held locks and retry if some lock cannot be acquired.
- Do not call `pthread_mutex_trylock()` in a tight loop (busy-waiting); it wastes CPU resources.
- Suitable for non-blocking algorithms or timeout mechanisms.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutex_timedlock

```c
int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime);
```

Locks a mutex with a timeout. If the mutex cannot be acquired immediately, blocks until the lock becomes available or the timeout expires.

**Parameters**:

- `mutex` Pointer to the mutex to lock. Cannot be `NULL`.
- `abstime` Absolute timeout (based on `CLOCK_REALTIME`).

**Returns**:

Returns 0 on success, or an error code on failure:

- `ETIMEDOUT` The lock could not be acquired within the timeout.
- `EINVAL` `mutex` is `NULL` or not properly initialized.
- `EDEADLK` Mutex is `PTHREAD_MUTEX_ERRORCHECK` and the current thread already holds it.

**Notes**:

- For recursive mutexes, if the current thread already holds the lock, the call succeeds and increments the lock count, regardless of timeout.
- `abstime` is an absolute time, not a relative one. Compute it based on `clock_gettime(CLOCK_REALTIME, ...)`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutex_unlock

```c
int pthread_mutex_unlock(pthread_mutex_t *mutex);
```

Unlocks a mutex, making it available for other waiting threads. Only the owning thread can unlock; otherwise, behavior depends on the mutex type.

For recursive mutexes, each `pthread_mutex_unlock()` call decrements the lock count; the lock is truly released only when the count reaches zero.

**Parameters**:

- `mutex` Pointer to the mutex to unlock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EPERM` The current thread does not own the mutex. For `PTHREAD_MUTEX_ERRORCHECK`, unlocking a mutex not held returns this error. For `PTHREAD_MUTEX_NORMAL`, behavior is undefined.
- `EINVAL` The mutex is not properly initialized or has been destroyed.

**Notes**:

- Must be unlocked by the same thread that locked the mutex; other threads cannot unlock on its behalf.
- Unlocking an unlocked mutex is undefined (for NORMAL) or returns an error (for ERRORCHECK).
- After unlocking, if threads are waiting for the lock, one of them is awakened and acquires the lock. Which one is awakened depends on the scheduling policy.
- When an exception or cancellation occurs while holding the lock, ensure the lock is released via cleanup handlers or exception handling.
- Unlock operations should be as fast as possible; avoid invoking unlock outside the lock-protected critical section.
- For priority-inheritance mutexes, unlocking restores the thread's original priority.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutex_consistent

```c
int pthread_mutex_consistent(pthread_mutex_t *mutex);
```

Marks a robust mutex as consistent. When a robust mutex's previous owner terminates without releasing, `pthread_mutex_lock()` returns `EOWNERDEAD`; the new owner should call this function to restore consistency.

**Parameters**:

- `mutex` Pointer to a robust mutex in an inconsistent state. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `mutex` is `NULL`, or not a robust mutex, or not in an inconsistent state.

**Notes**:

- Requires `PTHREAD_MUTEX_ROBUST` set in the mutex attributes.
- If you unlock directly without calling this function, the mutex enters an unrecoverable state, and subsequent `pthread_mutex_lock()` calls return `ENOTRECOVERABLE`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutex_setprioceiling

```c
int pthread_mutex_setprioceiling(pthread_mutex_t *mutex, int prioceiling, int *old_ceiling);
```

Dynamically modifies the priority ceiling of a mutex. This function first locks the mutex, modifies the ceiling, and then unlocks, ensuring the operation is atomic.

**Parameters**:

- `mutex` Pointer to the mutex.
- `prioceiling` New priority ceiling value.
- `old_ceiling` If non-`NULL`, stores the previous ceiling.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` Invalid argument, or `CONFIG_PRIORITY_PROTECT` is not enabled.
- `EPERM` Cannot lock the mutex.

**Notes**:

- Requires `CONFIG_PRIORITY_PROTECT`. When disabled, always returns `EINVAL`.
- The mutex protocol must be `PTHREAD_PRIO_PROTECT` for this to be meaningful.
- This function internally performs lock/unlock; the caller must not already hold the mutex.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutex_getprioceiling

```c
int pthread_mutex_getprioceiling(const pthread_mutex_t *mutex, int *prioceiling);
```

Gets the current priority ceiling of a mutex.

**Parameters**:

- `mutex` Pointer to the mutex.
- `prioceiling` Pointer to an integer variable for storing the ceiling.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` Invalid argument, or `CONFIG_PRIORITY_PROTECT` is not enabled.

**Notes**:

- Requires `CONFIG_PRIORITY_PROTECT`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Mutex Attributes


### pthread_mutexattr_init

```c
int pthread_mutexattr_init(pthread_mutexattr_t *attr);
```

Initializes a mutex attribute object to default values. Defaults: type `PTHREAD_MUTEX_NORMAL`, protocol `PTHREAD_PRIO_NONE`, process-private, non-robust.

**Parameters**:

- `attr` Pointer to the mutex attribute object to initialize. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_destroy

```c
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
```

Destroys a mutex attribute object.

**Parameters**:

- `attr` Pointer to the mutex attribute object to destroy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_settype

```c
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);
```

Sets the mutex type. The type determines repeated-lock and error-detection behavior.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `type` Mutex type:
  - `PTHREAD_MUTEX_NORMAL` No deadlock detection; repeated locking causes deadlock (default).
  - `PTHREAD_MUTEX_ERRORCHECK` Detects deadlocks; repeated locking returns `EDEADLK`.
  - `PTHREAD_MUTEX_RECURSIVE` Allows the same thread to lock multiple times; requires an equal number of unlocks.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `type` is not a valid value.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_gettype

```c
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);
```

Gets the mutex type.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `type` Pointer to an integer variable for storing the mutex type. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `type` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_setpshared

```c
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);
```

Sets the process-shared attribute in a mutex attribute object.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `pshared` Process-shared attribute:
  - `PTHREAD_PROCESS_PRIVATE` (0) Process-private (default).
  - `PTHREAD_PROCESS_SHARED` (1) Shared across processes.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `pshared` is neither 0 nor 1.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_getpshared

```c
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared);
```

Gets the process-shared attribute from a mutex attribute object.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `pshared` Pointer to an integer variable for storing the process-shared attribute. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `pshared` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_setprotocol

```c
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol);
```

Sets the mutex priority protocol. The protocol determines how threads holding the lock address priority inversion.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `protocol` Priority protocol:
  - `PTHREAD_PRIO_NONE` No priority protocol (default).
  - `PTHREAD_PRIO_INHERIT` Priority inheritance; a low-priority thread holding the lock has its priority temporarily raised to the highest among waiters (requires `CONFIG_PRIORITY_INHERITANCE`).
  - `PTHREAD_PRIO_PROTECT` Priority ceiling protection (requires `CONFIG_PRIORITY_PROTECT`).

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `protocol` is invalid or unsupported.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_getprotocol

```c
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr, int *protocol);
```

Gets the mutex priority protocol.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `protocol` Pointer to an integer variable for storing the protocol. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `protocol` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_setrobust

```c
int pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int robust);
```

Sets the mutex robustness attribute. A robust mutex avoids permanent deadlock if its owner terminates abnormally.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `robust` Robustness attribute:
  - `PTHREAD_MUTEX_STALLED` Non-robust (default); the lock becomes permanently unusable if the owner terminates.
  - `PTHREAD_MUTEX_ROBUST` Robust; if the owner terminates, the next locker receives `EOWNERDEAD` and can recover via `pthread_mutex_consistent()`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `robust` is not a valid value.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_getrobust

```c
int pthread_mutexattr_getrobust(const pthread_mutexattr_t *attr, int *robust);
```

Gets the mutex robustness attribute.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `robust` Pointer to an integer variable for storing the robustness. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `robust` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_setprioceiling

```c
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int prioceiling);
```

Sets the priority ceiling in a mutex attribute object. When the mutex protocol is `PTHREAD_PRIO_PROTECT`, the holder's priority is raised to this ceiling to avoid priority inversion.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `prioceiling` Priority ceiling; must be between `sched_get_priority_min(SCHED_FIFO)` and `sched_get_priority_max(SCHED_FIFO)`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `prioceiling` is out of the valid priority range.

**Notes**:

- Requires `CONFIG_PRIORITY_PROTECT`. When disabled, this function always returns `EINVAL`.
- Should be used together with `pthread_mutexattr_setprotocol(attr, PTHREAD_PRIO_PROTECT)`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_mutexattr_getprioceiling

```c
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr, int *prioceiling);
```

Gets the priority ceiling from a mutex attribute object.

**Parameters**:

- `attr` Pointer to the mutex attribute object. Cannot be `NULL`.
- `prioceiling` Pointer to an integer variable for storing the priority ceiling. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `prioceiling` is `NULL`, or `CONFIG_PRIORITY_PROTECT` is not enabled.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Condition Variables


### pthread_cond_init

```c
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
```

Initializes a condition variable. A condition variable can also be initialized statically with `PTHREAD_COND_INITIALIZER`.

**Parameters**:

- `cond` Pointer to the condition variable to initialize. Cannot be `NULL`.
- `attr` Condition variable attributes. If `NULL`, default attributes are used (`CLOCK_REALTIME`, process-private).

**Returns**:


Returns 0 on success, or an error code on failure:

- `EINVAL` `cond` is `NULL`.
- `ENOMEM` Insufficient memory.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_cond_destroy

```c
int pthread_cond_destroy(pthread_cond_t *cond);
```

Destroys a condition variable.

**Parameters**:

- `cond` Pointer to the condition variable to destroy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `cond` is `NULL` or not properly initialized.
- `EBUSY` A thread is waiting on the condition variable.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_cond_wait

```c
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
```

Waits for a condition variable to be signaled. This function atomically releases the mutex and blocks on the condition variable. When the condition variable is signaled via `pthread_cond_signal()` or `pthread_cond_broadcast()`, the thread wakes up, re-acquires the mutex, and returns.

Condition variables are commonly used to implement producer-consumer patterns or other scenarios requiring thread coordination. Typical usage:

```c
pthread_mutex_lock(&mutex);
while (!condition) {
    pthread_cond_wait(&cond, &mutex);
}
// Condition met; process data
pthread_mutex_unlock(&mutex);
```

**Parameters**:

- `cond` Pointer to the condition variable.
- `mutex` Pointer to the associated mutex. Must already be locked by the calling thread.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `cond` or `mutex` is not properly initialized, or a different mutex is used.
- `EPERM` The calling thread does not hold the mutex.

**Notes**:

- The associated mutex must be held before calling; otherwise behavior is undefined.
- On return, the mutex is re-acquired, even on error.
- Due to possible spurious wakeups, check the condition in a loop: `while (!condition) pthread_cond_wait(...)`. A spurious wakeup is when the thread wakes up without being signaled.
- The condition variable itself stores no state; it is merely a synchronization primitive. The actual condition (boolean expression) is maintained by the application, typically via shared variables.
- The mutex is atomically released while waiting, avoiding the race condition between releasing the lock and blocking.
- If the thread is canceled, the mutex is re-acquired and cleanup handlers are called. Cleanup handlers should release the lock.
- Multiple threads can wait on the same condition variable simultaneously.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_cond_timedwait

```c
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           const struct timespec *abstime);
```

Waits on a condition variable with a timeout. Behavior is the same as `pthread_cond_wait()`, but returns automatically after the timeout.

**Parameters**:

- `cond` Pointer to the condition variable.
- `mutex` Pointer to the associated mutex; must be locked before the call.
- `abstime` Absolute timeout. The clock source depends on the condition variable attributes (default `CLOCK_REALTIME`).

**Returns**:

Returns 0 on success, or an error code on failure:

- `ETIMEDOUT` No signal was received within the timeout.
- `EINVAL` Invalid arguments.

**Notes**:

- Even on timeout, the mutex is re-acquired.
- Still check the condition in a loop, as spurious wakeups are possible.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_cond_clockwait

```c
int pthread_cond_clockwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           clockid_t clockid, const struct timespec *abstime);
```

Waits on a condition variable using a specified clock. Allows specifying the clock source directly at wait time, without relying on the attribute object.

**Parameters**:

- `cond` Pointer to the condition variable.
- `mutex` Pointer to the associated mutex; must be locked before the call.
- `clockid` Clock ID, such as `CLOCK_REALTIME` or `CLOCK_MONOTONIC`.
- `abstime` Absolute timeout based on the specified clock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `ETIMEDOUT` No signal was received within the timeout.
- `EINVAL` Invalid arguments.

**POSIX Compatibility**: Compatible extension interface.


### pthread_cond_signal

```c
int pthread_cond_signal(pthread_cond_t *cond);
```

Wakes at least one thread waiting on the condition variable. If multiple threads are waiting, the scheduling policy determines which is awakened. If no thread is waiting, this call has no effect (signal is lost).

Unlike `pthread_cond_broadcast()`, `pthread_cond_signal()` wakes only one thread, suitable for scenarios where only one thread can handle the condition; this avoids unnecessary wakeups and context switches.

**Parameters**:

- `cond` Pointer to the condition variable to signal.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `cond` is not properly initialized.

**Notes**:

- You do not need to hold the associated mutex to call `pthread_cond_signal()`, but it is generally recommended to hold it to avoid race conditions.
- Awakened threads do not execute immediately; they first try to re-acquire the mutex. Releasing the lock immediately after signaling is a good practice.
- If you signal without holding the lock after modifying the condition, a lost-wakeup problem may occur: a waiter may be preempted between checking the condition and calling wait.
- Typical pattern:
  ```c
  pthread_mutex_lock(&mutex);
  // Modify shared state to make the condition true
  condition = true;
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  ```
- If the condition may satisfy multiple waiters, use `pthread_cond_broadcast()`.
- POSIX does not guarantee signal fairness; thread starvation is possible.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_cond_broadcast

```c
int pthread_cond_broadcast(pthread_cond_t *cond);
```

Wakes all threads waiting on the condition variable. All awakened threads compete to re-acquire the associated mutex. If no thread is waiting, the call has no effect.

Unlike `pthread_cond_signal()`, broadcast wakes all waiters, suitable when the condition change may affect multiple threads or when it is unclear which thread should be awakened.

**Parameters**:

- `cond` Pointer to the condition variable to broadcast.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `cond` is not properly initialized.

**Notes**:

- Similar to `pthread_cond_signal()`, the associated mutex should typically be held when calling.
- Awakened threads serialize on the mutex; only one thread at a time can acquire the mutex and proceed.
- Using broadcast may cause a "thundering herd": many threads wake up but only a few can actually handle the condition; the others go back to waiting, causing unnecessary context switches.
- Suitable scenarios:
  - Condition changes affect all waiters (e.g., resource state change, shutdown signal)
  - It is unclear which thread should handle the condition
  - All threads need to re-evaluate their waiting conditions
- Typical pattern:
  ```c
  pthread_mutex_lock(&mutex);
  // Modify shared state affecting all waiters
  shutdown = true;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mutex);
  ```
- If only one thread needs to be awakened, prefer `pthread_cond_signal()` for efficiency.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Condition Variable Attributes


### pthread_condattr_init

```c
int pthread_condattr_init(pthread_condattr_t *attr);
```

Initializes a condition variable attribute object to default values. Defaults:

- Process-shared attribute: `PTHREAD_PROCESS_PRIVATE`
- Clock attribute: `CLOCK_REALTIME`

Attribute objects are used when calling `pthread_cond_init()` to specify the behavior of the condition variable. The same attribute object can be used to initialize multiple condition variables.

**Parameters**:

- `attr` Pointer to the condition variable attribute object to initialize. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**Notes**:

- When no longer needed, destroy with `pthread_condattr_destroy()`.
- Modifications to the attribute object do not affect condition variables already created with it.
- To use `CLOCK_MONOTONIC` as the timeout clock (to avoid system time adjustments), call `pthread_condattr_setclock()` after initialization.
- In openvela, the attribute object is a simple struct (containing `pshared` and `clockid`), with no dynamic memory allocation.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_condattr_destroy

```c
int pthread_condattr_destroy(pthread_condattr_t *attr);
```

Destroys a condition variable attribute object, releasing its resources. A destroyed attribute object cannot be used again unless re-initialized with `pthread_condattr_init()`.

**Parameters**:

- `attr` Pointer to the condition variable attribute object to destroy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL` or not a valid attribute object.

**Notes**:

- Destruction does not affect condition variables already created with this object.
- In openvela, the attribute object does not use dynamic memory; this function is mainly for POSIX compatibility.
- Always pair `pthread_condattr_init()` with `pthread_condattr_destroy()`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_condattr_getpshared

```c
int pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared);
```

Gets the process-shared attribute from a condition variable attribute object. The process-shared attribute determines whether the condition variable can be accessed by threads in multiple processes.

**Parameters**:

- `attr` Pointer to the condition variable attribute object. Cannot be `NULL`.
- `pshared` Pointer to an integer variable for storing the current process-shared attribute. Cannot be `NULL`. Returned value is one of:
  - `PTHREAD_PROCESS_PRIVATE` The condition variable may only be used by threads in the same process (default).
  - `PTHREAD_PROCESS_SHARED` The condition variable may be used by threads across processes, provided it is allocated in shared memory.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `pshared` is `NULL`.

**Notes**:

- In openvela, due to the RTOS memory model, `PTHREAD_PROCESS_SHARED` behavior may differ from Linux.
- Default is `PTHREAD_PROCESS_PRIVATE`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_condattr_setpshared

```c
int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared);
```

Sets the process-shared attribute of a condition variable attribute object. This attribute determines whether the condition variable can be manipulated by threads in different processes.

When set to `PTHREAD_PROCESS_SHARED`, any thread with access to the memory holding the condition variable can operate on it. When set to `PTHREAD_PROCESS_PRIVATE`, only threads in the same process as the initializer can operate on it. Using a private condition variable from threads in a different process is undefined.

**Parameters**:

- `attr` Pointer to the condition variable attribute object. Cannot be `NULL`.
- `pshared` Process-shared attribute value; must be one of:
  - `PTHREAD_PROCESS_PRIVATE` Process-private (default).
  - `PTHREAD_PROCESS_SHARED` Shared across processes.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `pshared` is neither `PTHREAD_PROCESS_SHARED` nor `PTHREAD_PROCESS_PRIVATE`.

**Notes**:

- When using `PTHREAD_PROCESS_SHARED`, the condition variable must be allocated in shared memory accessible to all relevant processes.
- The mutex associated with the condition variable should also be set as process-shared.
- Modifying the attribute object does not affect already-created condition variables.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_condattr_getclock

```c
int pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clock_id);
```

Gets the clock attribute from a condition variable attribute object. The clock attribute determines which clock `pthread_cond_timedwait()` uses for timeout calculation.

**Parameters**:

- `attr` Pointer to the condition variable attribute object. Cannot be `NULL`.
- `clock_id` Pointer to a `clockid_t` variable for storing the current clock attribute. Returned value is one of:
  - `CLOCK_REALTIME` System real-time clock (default); affected by system time adjustments (e.g., NTP).
  - `CLOCK_MONOTONIC` Monotonic clock; unaffected by system time adjustments.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**Notes**:

- Default clock is `CLOCK_REALTIME`.
- If the application requires accurate timeouts or the system time may be adjusted, `CLOCK_MONOTONIC` is recommended.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_condattr_setclock

```c
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id);
```

Sets the clock attribute of a condition variable attribute object. This attribute specifies the clock source `pthread_cond_timedwait()` uses for timeout calculation.

Choosing the right clock is critical for timeout behavior:
- `CLOCK_REALTIME`: Uses the system real-time clock. The timeout is an absolute time. If the system time is moved forward, premature timeouts may occur; if moved backward, timeouts may be delayed.
- `CLOCK_MONOTONIC`: Uses a monotonically increasing clock unaffected by system time adjustments; suitable when precise timeout control is needed.

**Parameters**:

- `attr` Pointer to the condition variable attribute object. Cannot be `NULL`.
- `clock_id` Clock ID; must be one of:
  - `CLOCK_REALTIME` System real-time clock (default).
  - `CLOCK_MONOTONIC` Monotonic clock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `clock_id` is neither `CLOCK_REALTIME` nor `CLOCK_MONOTONIC`.

**Notes**:

- openvela only supports `CLOCK_REALTIME` and `CLOCK_MONOTONIC`; passing other clock IDs (e.g., `CLOCK_PROCESS_CPUTIME_ID`) returns `EINVAL`.
- Modifying the clock attribute does not affect already-created condition variables; it only affects those created later using this attribute object.
- When using `CLOCK_MONOTONIC`, the `abstime` passed to `pthread_cond_timedwait()` should be based on `clock_gettime(CLOCK_MONOTONIC, ...)`.
- `pthread_cond_clockwait()` allows specifying the clock at wait time without relying on the attribute object.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Read-Write Locks


### pthread_rwlock_init

```c
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);
```

Initializes a read-write lock. Read-write locks allow multiple threads to hold read locks simultaneously, but write locks are exclusive.

**Parameters**:

- `rwlock` Pointer to the read-write lock to initialize. Cannot be `NULL`.
- `attr` Read-write lock attributes. If `NULL`, defaults are used.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `rwlock` is `NULL`.
- `ENOMEM` Insufficient memory.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlock_destroy

```c
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
```

Destroys a read-write lock.

**Parameters**:

- `rwlock` Pointer to the read-write lock to destroy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `rwlock` is `NULL`.
- `EBUSY` The lock is currently held.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlock_rdlock

```c
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
```

Acquires a read lock. If no write lock is currently held, the call succeeds immediately; otherwise it blocks. Multiple threads may hold read locks simultaneously.

**Parameters**:

- `rwlock` Pointer to the read-write lock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `rwlock` is not properly initialized.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlock_tryrdlock

```c
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
```

Tries to acquire a read lock (non-blocking).

**Parameters**:

- `rwlock` Pointer to the read-write lock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EBUSY` A write lock is held; cannot acquire a read lock.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlock_timedrdlock

```c
int pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock, const struct timespec *abstime);
```

Acquires a read lock with a timeout. If the read lock cannot be acquired immediately, blocks until it becomes available or the timeout expires. Timeout is based on `CLOCK_REALTIME`.

**Parameters**:

- `rwlock` Pointer to the read-write lock.
- `abstime` Absolute timeout (based on `CLOCK_REALTIME`).

**Returns**:

Returns 0 on success, or an error code on failure:

- `ETIMEDOUT` The read lock could not be acquired within the timeout.
- `EBUSY` Read lock unavailable.
- `EINVAL` Invalid arguments.

**Notes**:

- Internally calls `pthread_rwlock_clockrdlock()` with `CLOCK_REALTIME`.
- For `CLOCK_MONOTONIC`, use `pthread_rwlock_clockrdlock()`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlock_clockrdlock

```c
int pthread_rwlock_clockrdlock(pthread_rwlock_t *rwlock, clockid_t clockid,
                               const struct timespec *abstime);
```

Acquires a read lock with a timeout using a specified clock.

**Parameters**:

- `rwlock` Pointer to the read-write lock.
- `clockid` Clock ID, such as `CLOCK_REALTIME` or `CLOCK_MONOTONIC`.
- `abstime` Absolute timeout based on the specified clock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `ETIMEDOUT` The read lock could not be acquired within the timeout.
- `EINVAL` Invalid arguments.

**POSIX Compatibility**: Compatible extension interface.


### pthread_rwlock_wrlock

```c
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
```

Acquires a write lock. Write locks are exclusive; the call waits until all read and write locks are released.

**Parameters**:

- `rwlock` Pointer to the read-write lock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `rwlock` is not properly initialized.
- `EAGAIN` The number of writers has reached the maximum.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlock_trywrlock

```c
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
```

Tries to acquire a write lock (non-blocking).

**Parameters**:

- `rwlock` Pointer to the read-write lock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EBUSY` A read or write lock is held; cannot acquire a write lock.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlock_timedwrlock

```c
int pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock, const struct timespec *abstime);
```

Acquires a write lock with a timeout. If the write lock cannot be acquired immediately, blocks until it becomes available or the timeout expires. Timeout is based on `CLOCK_REALTIME`.

**Parameters**:

- `rwlock` Pointer to the read-write lock.
- `abstime` Absolute timeout (based on `CLOCK_REALTIME`).

**Returns**:

Returns 0 on success, or an error code on failure:

- `ETIMEDOUT` The write lock could not be acquired within the timeout.
- `EAGAIN` The number of writers has reached the maximum.
- `EINVAL` Invalid arguments.

**Notes**:

- Internally calls `pthread_rwlock_clockwrlock()` with `CLOCK_REALTIME`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlock_clockwrlock

```c
int pthread_rwlock_clockwrlock(pthread_rwlock_t *rwlock, clockid_t clockid,
                               const struct timespec *abstime);
```

Acquires a write lock with a timeout using a specified clock.

**Parameters**:

- `rwlock` Pointer to the read-write lock.
- `clockid` Clock ID, such as `CLOCK_REALTIME` or `CLOCK_MONOTONIC`.
- `abstime` Absolute timeout based on the specified clock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `ETIMEDOUT` The write lock could not be acquired within the timeout.
- `EAGAIN` The number of writers has reached the maximum.
- `EINVAL` Invalid arguments.

**POSIX Compatibility**: Compatible extension interface.


### pthread_rwlock_unlock

```c
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
```

Releases a read-write lock (either read or write).

**Parameters**:

- `rwlock` Pointer to the read-write lock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `rwlock` is not properly initialized.
- `EPERM` The current thread does not hold the lock.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Read-Write Lock Attributes


### pthread_rwlockattr_init

```c
int pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
```

Initializes a read-write lock attribute object to default values. Default process-shared attribute is `PTHREAD_PROCESS_PRIVATE`.

**Parameters**:

- `attr` Pointer to the read-write lock attribute object to initialize. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlockattr_destroy

```c
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
```

Destroys a read-write lock attribute object.

**Parameters**:

- `attr` Pointer to the read-write lock attribute object to destroy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlockattr_setpshared

```c
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared);
```

Sets the process-shared attribute of a read-write lock attribute object.

**Parameters**:

- `attr` Pointer to the read-write lock attribute object. Cannot be `NULL`.
- `pshared` Process-shared attribute: `PTHREAD_PROCESS_PRIVATE` or `PTHREAD_PROCESS_SHARED`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `pshared` is not a valid value.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_rwlockattr_getpshared

```c
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared);
```

Gets the process-shared attribute from a read-write lock attribute object.

**Parameters**:

- `attr` Pointer to the read-write lock attribute object. Cannot be `NULL`.
- `pshared` Pointer to an integer variable for storing the attribute. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `pshared` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Barriers


### pthread_barrier_init

```c
int pthread_barrier_init(pthread_barrier_t *barrier,
                         const pthread_barrierattr_t *attr, unsigned int count);
```

Initializes a barrier. Barriers synchronize multiple threads; all threads must arrive at the barrier before any can continue.

**Parameters**:

- `barrier` Pointer to the barrier to initialize. Cannot be `NULL`.
- `attr` Barrier attributes. If `NULL`, defaults are used.
- `count` Number of threads required to reach the barrier. Must be greater than 0.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `barrier` is `NULL`, or `count` is 0.
- `ENOMEM` Insufficient memory.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_barrier_destroy

```c
int pthread_barrier_destroy(pthread_barrier_t *barrier);
```

Destroys a barrier.

**Parameters**:

- `barrier` Pointer to the barrier to destroy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `barrier` is `NULL`.
- `EBUSY` Threads are waiting on the barrier.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_barrier_wait

```c
int pthread_barrier_wait(pthread_barrier_t *barrier);
```

Waits at a barrier. When all threads (the number specified by the `count` parameter of `pthread_barrier_init`) have called this function, all are released simultaneously.

**Parameters**:

- `barrier` Pointer to the barrier.

**Returns**:

One thread returns `PTHREAD_BARRIER_SERIAL_THREAD` (this thread can be used for cleanup work); others return 0.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Barrier Attributes


### pthread_barrierattr_init

```c
int pthread_barrierattr_init(pthread_barrierattr_t *attr);
```

Initializes a barrier attribute object to default values. Default process-shared attribute is `PTHREAD_PROCESS_PRIVATE`.

**Parameters**:

- `attr` Pointer to the barrier attribute object to initialize. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_barrierattr_destroy

```c
int pthread_barrierattr_destroy(pthread_barrierattr_t *attr);
```

Destroys a barrier attribute object.

**Parameters**:

- `attr` Pointer to the barrier attribute object to destroy. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_barrierattr_setpshared

```c
int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int pshared);
```

Sets the process-shared attribute of a barrier attribute object.

**Parameters**:

- `attr` Pointer to the barrier attribute object. Cannot be `NULL`.
- `pshared` Process-shared attribute: `PTHREAD_PROCESS_PRIVATE` or `PTHREAD_PROCESS_SHARED`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` is `NULL`, or `pshared` is not a valid value.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_barrierattr_getpshared

```c
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *attr, int *pshared);
```

Gets the process-shared attribute from a barrier attribute object.

**Parameters**:

- `attr` Pointer to the barrier attribute object. Cannot be `NULL`.
- `pshared` Pointer to an integer variable for storing the attribute. Cannot be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `attr` or `pshared` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Spinlocks


### pthread_spin_init

```c
int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
```

Initializes a spinlock. Spinlocks acquire the lock via busy-waiting, suitable for very short critical sections.

**Parameters**:

- `lock` Pointer to the spinlock to initialize. Cannot be `NULL`.
- `pshared` Shared attribute: `PTHREAD_PROCESS_PRIVATE` or `PTHREAD_PROCESS_SHARED`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `lock` is `NULL`.

**Notes**:

- Do not use spinlocks for scenarios where the lock is held for long periods; this wastes CPU.
- Do not call potentially blocking functions while holding a spinlock.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_spin_destroy

```c
int pthread_spin_destroy(pthread_spinlock_t *lock);
```

Destroys a spinlock.

**Parameters**:

- `lock` Pointer to the spinlock to destroy.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `lock` is not properly initialized.
- `EBUSY` The spinlock is currently locked.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_spin_lock

```c
int pthread_spin_lock(pthread_spinlock_t *lock);
```

Acquires a spinlock. If the lock is already held, the calling thread busy-waits until the lock is available.

**Parameters**:

- `lock` Pointer to the spinlock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `lock` is not properly initialized.
- `EDEADLK` The current thread already holds the lock (implementation-dependent).

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_spin_trylock

```c
int pthread_spin_trylock(pthread_spinlock_t *lock);
```

Tries to acquire a spinlock (non-blocking).

**Parameters**:

- `lock` Pointer to the spinlock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EBUSY` The spinlock is already locked.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_spin_unlock

```c
int pthread_spin_unlock(pthread_spinlock_t *lock);
```

Releases a spinlock.

**Parameters**:

- `lock` Pointer to the spinlock.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `lock` is not properly initialized.
- `EPERM` The current thread does not hold the lock.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Thread-Specific Data


### pthread_key_create

```c
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
```

Creates a thread-specific data key. Each thread can use the key to store and retrieve its own private data.

**Parameters**:

- `key` Pointer to a `pthread_key_t` variable for storing the created key. Cannot be `NULL`.
- `destructor` Destructor called automatically for non-`NULL` data when the thread exits. May be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EAGAIN` System key count limit reached (`PTHREAD_KEYS_MAX`).
- `ENOMEM` Insufficient memory.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_key_delete

```c
int pthread_key_delete(pthread_key_t key);
```

Deletes a thread-specific data key. Destructors are not called, and per-thread data associated with the key is not released.

**Parameters**:

- `key` Key to delete.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `key` is invalid.

**Notes**:

- After deleting the key, each thread should release its associated data; otherwise memory leaks occur.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_setspecific

```c
int pthread_setspecific(pthread_key_t key, const void *value);
```

Sets thread-specific data for the calling thread.

**Parameters**:

- `key` Data key; must be a valid key created by `pthread_key_create()`.
- `value` Value to store. May be `NULL`.

**Returns**:

Returns 0 on success, or an error code on failure:

- `EINVAL` `key` is invalid.
- `ENOMEM` Insufficient memory.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_getspecific

```c
void *pthread_getspecific(pthread_key_t key);
```

Gets thread-specific data for the calling thread.

**Parameters**:

- `key` Data key.

**Returns**:

Returns the value associated with the key. Returns `NULL` if the key is invalid or no value has been set.

**Notes**:

- This function does not return an error code; it cannot distinguish between "not set" and "set to NULL".

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Thread Cleanup


### pthread_cleanup_push

```c
void pthread_cleanup_push(void (*routine)(void *), void *arg);
```

Registers a thread cleanup function. The cleanup function runs when the thread is canceled, calls `pthread_exit()`, or calls `pthread_cleanup_pop(1)`.

**Parameters**:

- `routine` Cleanup function. Cannot be `NULL`.
- `arg` Argument passed to the cleanup function.

**Returns**:

No return value.

**Notes**:

- Must be paired with `pthread_cleanup_pop()` and in the same function scope.
- Cleanup functions run in reverse registration order (last registered runs first).
- Commonly used to ensure mutexes are released on thread cancellation.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### pthread_cleanup_pop

```c
void pthread_cleanup_pop(int execute);
```

Removes the most recently registered cleanup function and optionally executes it.

**Parameters**:

- `execute` If nonzero, removes and executes the cleanup function; if zero, only removes it.

**Returns**:

No return value.

**Notes**:

- Must be paired with `pthread_cleanup_push()`.
- Even if `execute` is 0, the cleanup function is removed from the stack.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Extension Interfaces


### pthread_setname_np

```c
int pthread_setname_np(pthread_t thread, const char *name);
```

Sets a thread's name. Thread names are used for debugging and logging and can be viewed via `ps` or debuggers.

**Parameters**:

- `thread` Thread ID.
- `name` Thread name string.

**Returns**:

Returns 0 on success, or an error code on failure:

- `ESRCH` The specified thread cannot be found.
- `EINVAL` `name` is `NULL`.

**POSIX Compatibility**: Compatible with the Linux extension interface.


### pthread_getname_np

```c
int pthread_getname_np(pthread_t thread, char *name, size_t len);
```

Gets a thread's name.

**Parameters**:

- `thread` Thread ID.
- `name` Buffer for storing the name.
- `len` Buffer size.

**Returns**:

Returns 0 on success, or an error code on failure:

- `ESRCH` The specified thread cannot be found.
- `EINVAL` `name` is `NULL`.

**POSIX Compatibility**: Compatible with the Linux extension interface.


### pthread_gettid_np

```c
pid_t pthread_gettid_np(pthread_t thread);
```

Gets the thread's kernel thread ID (`pid_t`).

**Parameters**:

- `thread` Thread ID.

**Returns**:

Returns the kernel thread ID.

**Notes**:

- In openvela, `pthread_t` is itself `pid_t`, so this function returns the input value directly.

**POSIX Compatibility**: Compatible extension interface.


### pthread_getcpuclockid

```c
int pthread_getcpuclockid(pthread_t thread, clockid_t *clockid);
```

Gets the CPU clock ID of a thread. This clock measures CPU time consumed by the specified thread.

**Parameters**:

- `thread` Thread ID.
- `clockid` Pointer to a `clockid_t` variable for storing the clock ID.

**Returns**:

Returns 0 on success, or an error code on failure:

- `ESRCH` The specified thread cannot be found.
- `EINVAL` `clockid` is `NULL`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.
