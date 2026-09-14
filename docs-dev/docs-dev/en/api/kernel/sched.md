\[ English | [简体中文](../../../zh-cn/api/kernel/sched.md) \]

# Scheduling Management API

openvela provides POSIX-compliant task scheduling interfaces, supporting multiple scheduling policies and task management capabilities.

Header file: `#include <sched.h>`


## openvela Implementation Notes

- **Scheduling policies**: Supports `SCHED_FIFO` (first-in, first-out), `SCHED_RR` (round-robin, requires `CONFIG_RR_INTERVAL > 0`), `SCHED_SPORADIC` (requires `CONFIG_SCHED_SPORADIC`), and `SCHED_OTHER` (mapped to SCHED_FIFO).
- **Priority range**: Typically 1–255; larger numeric values indicate higher priority. Priority 0 is reserved for the idle task.
- **Return value style**: The `task_*` family returns negative error codes (such as `-EINVAL`), while the `sched_*` family follows POSIX conventions by returning -1 and setting `errno`.
- **SMP support**: CPU affinity interfaces require `CONFIG_SMP` to be enabled.
- **task vs pthread**: `task_create()` creates a native openvela task, while `pthread_create()` creates a POSIX thread. They share the underlying scheduler, but tasks do not support pthread-specific features (such as TSD or cleanup handlers).


## Task Management


### task_create

```c
int task_create(const char *name, int priority, int stack_size,
                main_t entry, char * const argv[]);
```

Creates a new task and makes it ready. The new task starts executing from the `entry` function and may receive the argument array `argv`. After creation, the task is immediately in the ready state, and runs according to its priority and scheduling policy.

Unlike `pthread_create()`, `task_create()` is an openvela-specific lightweight task creation interface. The created task is not a POSIX thread but a native openvela task. The task stack is automatically allocated and managed by the system.

**Parameters**:

- `name` Task name (string), used for debugging and identification. Maximum length is determined by `CONFIG_TASK_NAME_SIZE`. The name may be `NULL`, but a meaningful name is recommended for debugging.
- `priority` Task priority (integer). The valid range depends on the scheduling policy:
  - Real-time priorities: typically 1–255 (query via `sched_get_priority_min/max()`)
  - Larger values mean higher priority
  - Priority 0 is typically reserved for the idle task
  - Use `SCHED_PRIORITY_DEFAULT` or set according to system needs
- `stack_size` Task stack size (bytes). Must be large enough to accommodate local variables, function calls, and interrupt handling. At least 2048 bytes is recommended; complex tasks may need larger stacks. The stack size is automatically aligned to system requirements.
- `entry` Task entry function of type `main_t` with signature `int (*)(int argc, char *argv[])`. The task terminates when this function returns; the return value serves as the task exit status.
- `argv` Argument array (pointer-to-string array) passed to the task. Must be `NULL`-terminated. Similar to `main()`'s `argv`. The argument strings are copied, so the original strings can be freed after the call. Pass `NULL` or an empty array `{NULL}` if no arguments are needed.

**Returns**:

- Success: Returns the new task's PID (process ID, positive integer). Can be used for subsequent task control operations (such as `task_delete()`, `sched_setparam()`).
- Failure: Returns a negative error code:
  - `-EINVAL`: Invalid argument (e.g., priority out of range, stack size is 0)
  - `-ENOMEM`: Insufficient memory; failed to allocate the task control block or stack
  - `-EAGAIN`: Insufficient system resources; task count limit reached

**Notes**:

- **Task vs thread**: Tasks created by `task_create()` are native openvela tasks, not POSIX threads. Tasks are more lightweight than threads but do not support some pthread features (such as thread-local storage or thread cleanup handlers). Use `pthread_create()` for POSIX compatibility.
- **Stack allocation**: The stack is automatically allocated by the system (typically from the heap) and released when the task terminates. Use `task_create_with_stack()` to use a pre-allocated stack.
- **Argument passing**: The `argv` array and its strings are copied into the task's context, so the caller may free or modify the originals after the function returns. Note that arguments are shallow-copied (pointers are copied, but pointed-to data is not).
- **Task scheduling**: After creation, the task is immediately ready. If the new task's priority is higher than the current task, it preempts the current task (preemptive scheduling).
- **Task termination**: The task terminates automatically when the entry function returns. It can also be terminated by calling `exit()`, `task_delete()`, or receiving a signal (such as `SIGKILL`).
- **Resource management**: After termination, the system automatically reclaims core resources (stack, task control block), but other task-allocated resources (open files, allocated memory, etc.) must be cleaned up by the task itself.
- **Initial scheduling policy**: The new task's scheduling policy defaults to `SCHED_FIFO` (or the system default). Modify with `sched_setscheduler()` after creation.
- **Typical usage**:
  ```c
  char *argv[] = {"arg1", "arg2", NULL};
  int pid = task_create("my_task", 100, 4096, task_main, argv);
  if (pid < 0) {
      printf("Failed to create task: %d\n", pid);
  }
  ```
- **Difference from fork()**: Unlike `fork()`, `task_create()` does not duplicate the parent's address space; the new task starts at the specified entry function and does not share resources other than code with the parent.

**POSIX Compatibility**: openvela extension interface (non-POSIX standard).


### task_create_with_stack

```c
int task_create_with_stack(const char *name, int priority,
                           void *stack, int stack_size,
                           main_t entry, char * const argv[]);
```

Creates a new task using a pre-allocated stack. Similar to `task_create()`, but allows the caller to provide the stack memory instead of having the system allocate it automatically. Useful when precise control over memory layout is required, when using special memory regions (shared memory, DMA-accessible memory), or when optimizing startup performance.

A pre-allocated stack gives programmers more control but also more responsibility (stack size validation, memory alignment, lifecycle management).

**Parameters**:

- `name` Task name, used for debugging and identification. The string is copied, so it may be a temporary buffer. Maximum length is typically defined by `CONFIG_TASK_NAME_SIZE` (e.g., 31 characters + NULL). If `NULL`, the task will have an auto-generated name.
- `priority` Task priority; larger values are higher priority. Valid range is typically 1 to 255, and may be queried via `sched_get_priority_min()` / `sched_get_priority_max()`. Priority determines the scheduling order.
- `stack` Pointer to the pre-allocated stack memory. Must be:
  - **Non-NULL**: Cannot be NULL, otherwise an error is returned
  - **Large enough**: At least `stack_size` bytes
  - **Properly aligned**: Usually must be aligned to the architecture's required boundary (e.g., 8 or 16 bytes)
  - **Writable**: The stack memory must be readable and writable
  - **Lifecycle managed**: The caller is responsible for releasing the stack memory after task termination (if dynamically allocated)
- `stack_size` Stack size (bytes). Must satisfy:
  - **Minimum requirement**: At least `PTHREAD_STACK_MIN` (typically several hundred bytes)
  - **Task needs**: Large enough for local variables, function call depth, interrupt/exception handling
  - **Alignment**: Some architectures may require the size to be aligned (e.g., a multiple of 8 bytes)
- `entry` Task entry function with signature `int main(int argc, char *argv[])`. Cannot be `NULL`, otherwise an error is returned.
- `argv` Argument array passed to the task (similar to `main()`'s `argv`). The array must be `NULL`-terminated. Can be `NULL`, indicating no arguments (equivalent to an empty array).

**Returns**:

- Success: Returns the new task's PID (positive integer)
- Failure: Returns a negative error code:
  - `-EINVAL`: Invalid argument (e.g., `stack` is NULL, `entry` is NULL, `priority` out of range)
  - `-ENOMEM`: Insufficient memory (the stack is provided, but the task control block and other structures still need allocation)
  - `-EAGAIN`: System task count limit reached (`CONFIG_MAX_TASKS`)

**Notes**:

- **Difference from task_create**:
  - **task_create**: System automatically allocates and frees the stack
  - **task_create_with_stack**: Caller provides the stack and is responsible for freeing it
- **Stack lifecycle management**:
  - The stack memory must remain valid for the entire lifetime of the task
  - After termination, the caller is responsible for freeing the stack memory (if dynamically allocated)
  - If the stack is a static array or global variable, no explicit free is required
- **Typical usage (dynamically allocated stack)**:
  ```c
  void *stack = malloc(8192);
  if (stack == NULL) {
      perror("malloc");
      return -1;
  }
  
  int pid = task_create_with_stack("worker", 100, stack, 8192,
                                   worker_func, NULL);
  if (pid < 0) {
      perror("task_create_with_stack");
      free(stack);
      return -1;
  }
  
  // ... wait for the task to finish ...
  waitpid(pid, NULL, 0);
  free(stack);  // release the stack
  ```
- **Static stack example**:
  ```c
  static uint8_t worker_stack[4096] __attribute__((aligned(16)));
  
  int pid = task_create_with_stack("worker", 100, worker_stack,
                                   sizeof(worker_stack), worker_func, NULL);
  ```
- **Stack direction**: Some architectures grow the stack downward, others upward. openvela handles stack direction automatically; the caller only provides the start address and size.
- **Stack alignment**: Ensure the stack address is properly aligned (typically 8 or 16 bytes), otherwise it may cause undefined behavior or performance degradation:
  ```c
  void *stack = aligned_alloc(16, 8192);  // 16-byte aligned
  ```
- **Stack overflow protection**: A pre-allocated stack does not automatically provide overflow protection (guard page). If needed, allocate additional guard pages at the top/bottom of the stack and mark them inaccessible:
  ```c
  void *stack_with_guard = malloc(8192 + 4096);  // extra guard page
  mprotect(stack_with_guard, 4096, PROT_NONE);   // inaccessible
  void *usable_stack = (char*)stack_with_guard + 4096;
  task_create_with_stack("worker", 100, usable_stack, 8192, worker_func, NULL);
  ```
- **Shared-memory stack**: Shared memory can be used as a stack for cross-process stack sharing (advanced usage; requires careful synchronization):
  ```c
  int shm_fd = shm_open("/worker_stack", O_CREAT | O_RDWR, 0666);
  ftruncate(shm_fd, 8192);
  void *stack = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  task_create_with_stack("worker", 100, stack, 8192, worker_func, NULL);
  ```
- **Performance considerations**: Using a pre-allocated stack reduces the memory allocation overhead during task creation and improves startup performance. In scenarios with frequent task creation/destruction (such as task pools), you can maintain a stack cache pool.
- **Debugging suggestion**: During debugging, fill stack boundaries with magic numbers (such as 0xDEADBEEF) and periodically check them to detect stack overflow early:
  ```c
  uint32_t *stack_end = (uint32_t*)((char*)stack + stack_size - sizeof(uint32_t));
  *stack_end = 0xDEADBEEF;
  // ... task runs ...
  if (*stack_end != 0xDEADBEEF) {
      printf("Stack overflow detected!\n");
  }
  ```
- **Real-time system optimization**: In real-time systems, using a pre-allocated stack avoids the nondeterminism of dynamic allocation, improving the determinism and speed of task creation.
- **Pitfalls**:
  - Too-small stack leads to stack overflow, which is hard to debug (usually manifests as random crashes or data corruption)
  - Forgetting to free a dynamically allocated stack causes memory leaks
  - Unaligned stacks may cause undefined behavior (crash on some architectures, just performance degradation on others)
  - Freeing the stack while the task is still running causes serious errors

**POSIX Compatibility**: openvela extension interface (non-POSIX standard; similar to some RTOS interfaces).


### task_delete

```c
int task_delete(pid_t pid);
```

Deletes (terminates) the specified task, releasing its system resources. Similar to `pthread_cancel()` or `kill(pid, SIGKILL)`, but this is the openvela-native interface, more direct and efficient.

Task deletion is forced and does not go through normal cleanup (such as `atexit` handlers) and should be used with care. Cooperative termination mechanisms (such as setting an exit flag so the task can exit itself) are generally preferred.

**Parameters**:

- `pid` PID of the task to delete. Special value 0 means delete the calling task (equivalent to `exit()`). Must be a valid task PID.

**Returns**:

- Success: Returns 0 (`OK`)
- Failure: Returns a negative error code:
  - `-EINVAL`: `pid` is invalid (e.g., negative)
  - `-ESRCH`: The specified task does not exist (invalid PID or already terminated)
  - `-EPERM`: The caller has no permission to delete the target task (depends on system configuration)

**Notes**:

- **Forced termination**: The task is terminated immediately; cleanup code (such as handlers registered via `pthread_cleanup_push` or `atexit` callbacks) is not executed. This may lead to resource leaks (unfreed memory, unclosed files, unlocked mutexes).
- **Resource cleanup**: The kernel automatically reclaims core resources (stack memory, task control block), but application-level resources (heap memory, open files) may not be cleaned up automatically.
- **Deleting self**: If `pid` is 0, the task deletes itself and never returns (similar to calling `exit(0)`):
  ```c
  task_delete(0);
  // This line is never executed
  ```
- **Difference from pthread_cancel**:
  - `task_delete()` is an immediate forced termination without cancellation point semantics
  - `pthread_cancel()` takes effect at the next cancellation point, allowing cleanup
  - `task_delete()` is an openvela extension; `pthread_cancel()` is the POSIX standard
- **Typical usage**:
  ```c
  int pid = task_create("worker", 100, 2048, worker_func, NULL);
  // ... task running ...
  
  // Terminate the task
  if (task_delete(pid) == 0) {
      printf("Task %d deleted\n", pid);
  } else {
      perror("task_delete");
  }
  ```
- **Mutex pitfall**: If the deleted task was holding a mutex, other tasks waiting for the lock will block forever (deadlock). Ensure all locks are released before deletion.
- **Child task cleanup**: Deleting a parent task does not automatically delete its children. To clean up an entire task tree, delete all children explicitly.
- **Alternatives**:
  - **Cooperative exit**: Set an exit flag so the task checks it and exits
  - **pthread_cancel**: Use the POSIX cancellation mechanism, which allows cleanup handlers to run
  - **Signals**: Send `SIGTERM` so the task catches it and exits gracefully
- **Real-time note**: In real-time systems, forced deletion may affect determinism and predictability; use with care.
- **Debugging suggestion**: During debugging, add logs at task entry and exit to trace the task lifecycle.
- **Bulk deletion**: When deleting multiple tasks, delete them in dependency order (children first, parent later) to avoid dangling references.

**POSIX Compatibility**: openvela extension interface (non-POSIX standard).


### task_restart

```c
int task_restart(pid_t pid);
```

Restarts the specified task; the task re-executes from its original entry point with the original arguments, priority, and stack size. Equivalent to terminating the task and recreating it with the same arguments.

Task restart is a special recovery mechanism for handling task exceptions or scenarios requiring a reset of the task state. Compared with delete-and-recreate, restart preserves the original task configuration.

**Parameters**:

- `pid` PID of the task to restart. Must be a valid task PID (cannot be 0, since a task cannot restart itself).

**Returns**:

- Success: Returns 0 (`OK`)
- Failure: Returns a negative error code:
  - `-EINVAL`: `pid` is invalid (e.g., 0 or negative)
  - `-ESRCH`: The specified task does not exist (invalid PID or already terminated)
  - `-EPERM`: The caller has no permission to restart the target task (depends on system configuration)
  - `-ENOMEM`: Insufficient memory; unable to restart the task

**Notes**:

- **Cannot restart self**: Cannot restart oneself via `task_restart(0)` or `task_restart(getpid())`, because restart destroys the current execution context. Attempting to do so typically returns `-EINVAL`.
- **Task state reset**: After restart, all task state (local variables, stack content, registers) is reset, as if just created. The task starts execution from the entry function.
- **Configuration preserved**: The restarted task preserves its original configuration:
  - Task name (name)
  - Priority (priority)
  - Stack size (stack_size)
  - Entry function (entry)
  - Entry arguments (argv)
- **PID remains unchanged**: After restart, the task's PID remains unchanged, so references from other tasks remain valid.
- **Typical usage**:
  ```c
  int pid = task_create("monitor", 150, 2048, monitor_task, NULL);
  
  // ... task runs for a while, then an anomaly is detected ...
  
  // Restart the task
  if (task_restart(pid) == 0) {
      printf("Task %d restarted\n", pid);
  } else {
      perror("task_restart");
  }
  ```
- **Difference from task_delete + task_create**:
  - **task_restart**: PID unchanged, original configuration preserved, one-step
  - **task_delete + task_create**: PID changes, all arguments must be specified again, two-step
- **Resource cleanup**: Before restart, resources held by the task (open files, allocated memory, held locks) may not be released automatically, which may cause leaks. Consider exception recovery when designing tasks.
- **Mutex pitfall**: If the task was holding a mutex, restart causes the lock to never be released (deadlock). Ensure all locks are released before restart, or use robust mutexes.
- **Watchdog scenario**: Commonly used in watchdog systems. When a task is detected to be unresponsive or misbehaving, it is automatically restarted to recover service:
  ```c
  void watchdog_task(void *arg) {
      while (1) {
          if (check_task_health(worker_pid) == FAILED) {
              printf("Worker unhealthy, restarting...\n");
              task_restart(worker_pid);
          }
          sleep(5);
      }
  }
  ```
- **Restart count**: In production systems, the number of restarts should be limited to avoid infinite restart loops:
  ```c
  int restart_count = 0;
  const int MAX_RESTARTS = 3;
  
  if (task_restart(pid) == 0) {
      restart_count++;
      if (restart_count >= MAX_RESTARTS) {
          printf("Max restarts reached, giving up\n");
          task_delete(pid);
      }
  }
  ```
- **Asynchronous operation**: Restart is asynchronous; the task may still be initializing when the function returns. If waiting for initialization is required, use an additional synchronization mechanism (such as a semaphore).
- **Debugging suggestion**: Add logs in the task entry function to record each start time and reason, which helps analyze restart history.
- **Real-time system impact**: Frequent restarts may affect system real-time performance and predictability; improve task robustness to reduce the need for restarts.

**POSIX Compatibility**: openvela extension interface (non-POSIX standard).


## Task Cancellation


### task_setcancelstate

```c
int task_setcancelstate(int state, int *oldstate);
```

Sets the calling task's cancellation state, controlling whether the task can be canceled. Similar to `pthread_setcancelstate()` but applies to all tasks (not only pthreads).

The cancellation state is part of the task cancellation mechanism and allows a task to temporarily disable cancellation to protect critical sections from interruption.

**Parameters**:

- `state` The new cancellation state. Valid values:
  - `TASK_CANCEL_ENABLE` (0): Cancellation allowed (default); the task can respond to cancellation requests
  - `TASK_CANCEL_DISABLE` (1): Cancellation disabled; the task ignores cancellation requests (the request is deferred)
- `oldstate` If non-`NULL`, receives the previous cancellation state. Pass `NULL` if the old value is not needed.

**Returns**:

- Success: Returns 0 (`OK`)
- Failure: Returns a negative error code:
  - `-EINVAL`: `state` is invalid (not `TASK_CANCEL_ENABLE` or `TASK_CANCEL_DISABLE`)

**Notes**:

- **Protecting critical sections**: When performing uninterruptible critical operations (such as updating shared data structures or holding mutexes), cancellation should be disabled:
  ```c
  task_setcancelstate(TASK_CANCEL_DISABLE, NULL);
  // Critical operation, not interruptible by cancellation
  update_critical_data();
  task_setcancelstate(TASK_CANCEL_ENABLE, NULL);
  ```
- **Deferred cancellation**: When the cancel state is `TASK_CANCEL_DISABLE`, cancellation requests are not lost but deferred. When the state is set back to `TASK_CANCEL_ENABLE`, any pending cancellation request will cause the task to be canceled at the next cancellation point.
- **Interaction with cancel type**: The cancel state and cancel type (`task_setcanceltype()`) together determine cancellation behavior:
  - **state=ENABLE, type=DEFERRED**: Cancel only at cancellation points (default, safest)
  - **state=ENABLE, type=ASYNCHRONOUS**: Can cancel at any time (dangerous)
  - **state=DISABLE**: Never cancel, regardless of type
- **Typical usage (save old state)**:
  ```c
  int oldstate;
  task_setcancelstate(TASK_CANCEL_DISABLE, &oldstate);
  // Critical operation
  critical_section();
  task_setcancelstate(oldstate, NULL);  // Restore original state
  ```
- **With mutex**:
  ```c
  pthread_mutex_lock(&mutex);
  task_setcancelstate(TASK_CANCEL_DISABLE, NULL);
  
  // Protected operation
  modify_shared_data();
  
  task_setcancelstate(TASK_CANCEL_ENABLE, NULL);
  pthread_mutex_unlock(&mutex);
  ```
- **Default state**: Newly created tasks default to `TASK_CANCEL_ENABLE`, allowing cancellation.
- **Does not affect signals**: The cancel state affects only cancellation via `task_delete()` or similar mechanisms, not signal handling (such as `SIGTERM`).
- **Compatibility with pthread**: In pthread threads, `task_setcancelstate()` and `pthread_setcancelstate()` are usually equivalent and operate on the same underlying state.
- **Nested disable**: `TASK_CANCEL_DISABLE` can be called multiple times, but each call should be matched by a corresponding `TASK_CANCEL_ENABLE` (or restore the old state); otherwise cancellation may be permanently disabled:
  ```c
  int old1, old2;
  task_setcancelstate(TASK_CANCEL_DISABLE, &old1);  // First disable
  task_setcancelstate(TASK_CANCEL_DISABLE, &old2);  // Second disable (no effect)
  task_setcancelstate(old2, NULL);  // Restore to old2 (still disabled)
  task_setcancelstate(old1, NULL);  // Restore to old1 (possibly enabled)
  ```
  A simpler approach is to manipulate the cancel state only at the outermost scope.
- **Cleanup handlers**: Even when cancellation is disabled, cleanup handlers (`pthread_cleanup_push`) still run when the task exits normally.
- **Performance**: Setting the cancel state is lightweight, but avoid frequent switches in tight loops to prevent performance impact.

**POSIX Compatibility**: Similar to `pthread_setcancelstate()`, but applies to all task types.


### task_setcanceltype

```c
int task_setcanceltype(int type, int *oldtype);
```

Sets the calling task's cancellation type, controlling when cancellation requests take effect. Similar to `pthread_setcanceltype()` but applies to all tasks (not only pthreads).

The cancellation type determines when a task responds to cancellation requests, affecting cancellation safety and responsiveness.

**Parameters**:

- `type` The new cancellation type. Valid values:
  - `TASK_CANCEL_DEFERRED` (0): Deferred cancellation (default); responds to requests only at cancellation points, such as `pthread_testcancel()`, `sleep()`, `read()`, and other blocking calls
  - `TASK_CANCEL_ASYNCHRONOUS` (1): Asynchronous cancellation; the task can be canceled at any time (dangerous; may cause resource leaks or data inconsistency)
- `oldtype` If non-`NULL`, receives the previous cancellation type. Pass `NULL` if not needed.

**Returns**:

- Success: Returns 0 (`OK`)
- Failure: Returns a negative error code:
  - `-EINVAL`: `type` is invalid (not `TASK_CANCEL_DEFERRED` or `TASK_CANCEL_ASYNCHRONOUS`)

**Notes**:

- **Default type (DEFERRED) is safest**: Deferred cancellation is the default and recommended type; it only takes effect at well-defined cancellation points, allowing the task to complete the current operation and clean up resources before being canceled.
- **Danger of asynchronous cancellation**: `TASK_CANCEL_ASYNCHRONOUS` is extremely dangerous because the task may be canceled at any moment:
  - May be canceled while holding a mutex, causing deadlock
  - May be canceled mid-update of a data structure, causing data inconsistency
  - May be canceled after allocating memory but before saving the pointer, causing a memory leak
  - Only very specific code (such as pure computation tasks that do not access shared resources) should use asynchronous cancellation
- **Cancellation points**: Common cancellation points include:
  - `task_testcancel()` / `pthread_testcancel()`: Explicit cancellation points
  - Blocking system calls: `sleep()`, `usleep()`, `read()`, `write()`, `recv()`, `send()`
  - Synchronization primitives: `pthread_cond_wait()`, `sem_wait()`
  - Some library functions: `printf()` (possibly)
- **Typical usage (temporarily enable asynchronous cancel)**:
  ```c
  int oldtype;
  task_setcanceltype(TASK_CANCEL_ASYNCHRONOUS, &oldtype);
  // Pure computation task, no shared resources
  perform_long_computation();
  task_setcanceltype(oldtype, NULL);  // Restore original type
  ```
  But it is usually better to call `task_testcancel()` periodically inside a loop.
- **Recommended approach (add cancellation points in loops)**:
  ```c
  while (processing) {
      process_chunk();
      task_testcancel();  // Periodically check for cancellation requests
  }
  ```
  This is safer than asynchronous cancellation and still responds promptly.
- **Interaction with cancel state**: The cancel type and state together determine cancellation behavior:
  - **state=ENABLE, type=DEFERRED**: Cancel only at cancellation points (default, safest)
  - **state=ENABLE, type=ASYNCHRONOUS**: Can cancel at any time (dangerous)
  - **state=DISABLE**: Never cancel, regardless of type
- **Cleanup handlers**: Regardless of cancellation type, cleanup handlers (`pthread_cleanup_push`) run when the task is canceled. However, asynchronous cancellation may trigger cleanup in an inconsistent state, causing issues.
- **Real-time considerations**: In real-time systems, asynchronous cancellation may impact determinism; avoid using it. Prefer deferred cancellation or cooperative exit mechanisms.
- **Compatibility with pthread**: In pthread threads, `task_setcanceltype()` and `pthread_setcanceltype()` are usually equivalent.
- **Default value**: Newly created tasks default to `TASK_CANCEL_DEFERRED`.
- **Avoid mixing**: Do not mix deferred and asynchronous cancel types in the same task; it makes the code hard to understand and maintain. Pick one and stick with it.
- **Async-safe code**: If you must use asynchronous cancellation, ensure the task code is async-cancel-safe, similar to signal handler requirements:
  - Do not call non-async-safe functions (such as `malloc()`, `printf()`)
  - Do not access shared data (or use atomic operations)
  - Do not hold any locks

**POSIX Compatibility**: Similar to `pthread_setcanceltype()`, but applies to all task types.


### task_testcancel

```c
void task_testcancel(void);
```

Creates an explicit cancellation point. If there is a pending cancellation request and the task's cancel state is `TASK_CANCEL_ENABLE`, the task is canceled and terminates here.

This is the core of the deferred cancellation mechanism, allowing the task to respond to cancellation requests at safe locations, ensuring correct resource release and state consistency.

**Parameters**:

None.

**Returns**:

- No return value (if canceled, the function never returns)
- If there is no pending cancellation request, or if cancellation is disabled, the function returns normally

**Notes**:

- **Explicit cancellation point**: This function is a programmer-invoked cancellation point, distinct from implicit cancellation points (such as `sleep()`, `read()`). Explicit points provide precise control, allowing the task to check for cancellation at safe locations.
- **Typical usage (long-running loop)**:
  ```c
  while (processing) {
      process_data_chunk();
      task_testcancel();  // Periodically check for cancellation requests
  }
  ```
  This ensures timely response to cancellation while still completing each loop iteration.
- **Cancellation conditions**: The task is canceled only when all of the following are met:
  1. There is a pending cancellation request (via `task_delete()` or a similar mechanism)
  2. The cancel state is `TASK_CANCEL_ENABLE` (default)
  3. The cancel type is `TASK_CANCEL_DEFERRED` (default), or regardless of type (if the state is ENABLE)
- **Cleanup handlers**: If the task is canceled, cleanup handlers (registered via `pthread_cleanup_push`) run before termination. Register cleanup handlers for critical resources (such as mutexes):
  ```c
  pthread_mutex_lock(&mutex);
  pthread_cleanup_push((void(*)(void*))pthread_mutex_unlock, &mutex);
  
  // Possibly-canceled operation
  while (condition) {
      process_data();
      task_testcancel();
  }
  
  pthread_cleanup_pop(1);  // Unlock on normal exit too
  ```
- **Does not affect asynchronous cancellation**: If the cancel type is `TASK_CANCEL_ASYNCHRONOUS`, the task may be canceled at any moment, not only at cancellation points.
- **Compatibility with pthread_testcancel**: `task_testcancel()` and `pthread_testcancel()` are usually equivalent and interchangeable.
- **Performance**: Checking for a cancellation request is lightweight, but avoid calling it in extremely tight loops (e.g., microsecond-level iterations). Call once every N items processed:
  ```c
  for (int i = 0; i < items; i++) {
      process_item(i);
      if (i % 100 == 0) task_testcancel();  // Check every 100 items
  }
  ```
- **Cancel-safe locations**: Call `task_testcancel()` at locations where:
  - No mutex is held
  - All data structures are consistent
  - No unreleased temporary resources exist
- **Real-time system impact**: In real-time systems, cancellation points add (small) uncertainty to response time. For the highest determinism, avoid the cancellation mechanism and use cooperative exit.
- **Debugging suggestion**: When debugging cancellation issues, add logs before and after `task_testcancel()` to trace cancellation point execution:
  ```c
  printf("Before testcancel\n");
  task_testcancel();
  printf("After testcancel (not cancelled)\n");
  ```
- **Comparison with exit flag**:
  - **task_testcancel()**: Kernel mechanism, lighter-weight, integrates with cleanup handlers
  - **Exit flag**: User-space mechanism, more flexible, but cleanup must be managed manually
  ```c
  // Exit flag approach
  volatile bool should_exit = false;
  while (!should_exit) {
      process_data();
  }
  ```
- **No-op return**: If there is no cancellation request, the function returns immediately with almost no overhead (just a flag check).

**POSIX Compatibility**: Similar to `pthread_testcancel()`, but applies to all task types.


## Scheduling Policies and Parameters


### sched_setscheduler

```c
int sched_setscheduler(pid_t pid, int policy, const struct sched_param *param);
```

Sets the scheduling policy and parameters (such as priority) of the specified task. This is the primary interface to control task scheduling behavior, allowing dynamic adjustment of task scheduling characteristics at runtime.

The scheduling policy determines how the task competes for CPU time. Different policies suit different types of tasks (real-time, batch, interactive, etc.). Modifying the scheduling policy typically requires appropriate privileges.

**Parameters**:

- `pid` PID of the target task. Special value 0 means the calling task. Must be a valid task PID.
- `policy` The new scheduling policy. Valid values include:
  - `SCHED_FIFO` (0): First-in, first-out real-time scheduling, no time slice, suitable for hard real-time tasks
  - `SCHED_RR` (1): Round-robin real-time scheduling, with time slice, suitable for real-time tasks needing fairness
  - `SCHED_SPORADIC` (2): Sporadic scheduling, suitable for periodic real-time tasks (requires `CONFIG_SCHED_SPORADIC`)
  - `SCHED_OTHER` (3): Standard time-sharing scheduling, mapped to SCHED_FIFO or SCHED_RR
  - `SCHED_NORMAL` (3): Alias for SCHED_OTHER
  - `SCHED_BATCH` (4): Batch scheduling (if supported)
  - `SCHED_IDLE` (5): Idle scheduling, lowest priority (if supported)
- `param` Pointer to a `struct sched_param` containing scheduling parameters. At minimum, set the `sched_priority` field (basic priority). For SCHED_SPORADIC, also set the sporadic server parameters (low priority, replenish period, initial budget, maximum replenishments).

**Returns**:

- Success: Returns the task's previous scheduling policy (`SCHED_FIFO`, `SCHED_RR`, etc.)
- Failure: Returns -1 and sets `errno`:
  - `EINVAL`: `policy` is invalid, or the priority in `param` is out of range for the policy
  - `ESRCH`: The specified task does not exist (invalid PID or already terminated)
  - `EPERM`: The caller lacks permission to modify the target task's scheduling policy (typically requires superuser or same user)
  - `EFAULT`: `param` points to invalid memory

**Notes**:

- **Priority range**: Different scheduling policies have different valid priority ranges; query via `sched_get_priority_min(policy)` and `sched_get_priority_max(policy)`. Setting an out-of-range priority returns `EINVAL`.
- **Policy switch effects**:
  - Switching to a higher-priority policy may immediately preempt the current task
  - Switching to a lower priority may cause the task to be preempted by others
  - Policy changes do not affect task ready state; blocked tasks remain blocked
- **SCHED_SPORADIC parameters**: When using this policy, set in `param`:
  - `sched_ss_low_priority`: Low priority after budget is exhausted
  - `sched_ss_repl_period`: Replenish period
  - `sched_ss_init_budget`: Initial budget
  - `sched_ss_max_repl`: Maximum pending replenishments (<= `SS_REPL_MAX`)
- **Real-time policies**: SCHED_FIFO and SCHED_RR are real-time policies and typically require elevated privileges. Real-time tasks can affect system responsiveness; use with care.
- **Policy inheritance**: Child tasks (created via `fork()` or `task_create()`) typically inherit the parent's policy and priority unless specified at creation or modified later.
- **Typical usage**:
  ```c
  struct sched_param param;
  param.sched_priority = 150;  // Set high priority
  
  int old_policy = sched_setscheduler(0, SCHED_FIFO, &param);
  if (old_policy < 0) {
      perror("sched_setscheduler");
  } else {
      printf("Changed from policy %d to SCHED_FIFO\n", old_policy);
  }
  ```
- **Querying current policy**: Use `sched_getscheduler(pid)` to query the task's current policy.
- **Modify priority only**: To modify only the priority without changing the policy, use `sched_setparam()`; it is more efficient and its semantics are clearer.
- **Atomicity**: Policy and parameter modifications are atomic; no intermediate state is observable.
- **Effect on running task**: Modifying the currently running task (pid=0) causes the scheduler to reevaluate priorities immediately, which may preempt the task.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sched_getscheduler

```c
int sched_getscheduler(pid_t pid);
```

Queries the current scheduling policy of the specified task. A lightweight query interface used to obtain the task's scheduling policy type (such as SCHED_FIFO or SCHED_RR).

The scheduling policy determines the task's scheduling behavior. Understanding the current policy helps debugging and monitoring task real-time characteristics.

**Parameters**:

- `pid` PID of the target task. Special value 0 queries the calling task. Must be a valid task PID.

**Returns**:

- Success: Returns the task's current scheduling policy (non-negative integer):
  - `SCHED_FIFO` (0): First-in, first-out real-time scheduling
  - `SCHED_RR` (1): Round-robin real-time scheduling
  - `SCHED_SPORADIC` (2): Sporadic scheduling (if supported)
  - `SCHED_OTHER` (3): Standard time-sharing scheduling
  - `SCHED_BATCH` (4): Batch scheduling (if supported)
  - `SCHED_IDLE` (5): Idle scheduling (if supported)
- Failure: Returns -1 and sets `errno`:
  - `ESRCH`: The specified task does not exist (invalid PID or already terminated)
  - `EINVAL`: `pid` is negative

**Notes**:

- **Read-only query**: Does not modify task state; a pure query with very low overhead.
- **Use with others**: Typically used with `sched_getparam()` to obtain complete scheduling info (policy + parameters).
- **Typical usage**:
  ```c
  int policy = sched_getscheduler(0);  // Query own policy
  if (policy >= 0) {
      const char *policy_names[] = {"SCHED_FIFO", "SCHED_RR", "SCHED_SPORADIC", "SCHED_OTHER"};
      printf("Current policy: %s\n", policy_names[policy]);
  } else {
      perror("sched_getscheduler");
  }
  ```
- **Task diagnostics**: When debugging real-time systems, use this function to verify that a task runs under the expected scheduling policy.
- **Monitoring tools**: System monitoring tools often use this function to display task scheduling policy to aid in analyzing scheduling behavior.
- **Modifying policy**: To modify the scheduling policy, use `sched_setscheduler()`.
- **Policy name mapping**: Use a switch or array to map returned integer values to policy names for readability.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sched_setparam

```c
int sched_setparam(pid_t pid, const struct sched_param *param);
```

Modifies the scheduling parameters (primarily priority) of the specified task, without changing the scheduling policy. This is the standard interface for adjusting task priority; lighter and more clearly semantic than `sched_setscheduler()`.

Priority is the most important parameter in the scheduler; it determines execution order among tasks under the same policy. Dynamically adjusting priority is common in real-time systems (for example, implementing priority inheritance or priority ceiling protocols).

**Parameters**:

- `pid` PID of the target task. Special value 0 modifies the calling task. Must be a valid task PID.
- `param` Pointer to a `struct sched_param` containing the new scheduling parameters. Main fields:
  - `sched_priority`: New priority value (required); must be within the current policy's valid range
  - For SCHED_SPORADIC, includes `sched_ss_low_priority`, `sched_ss_repl_period`, `sched_ss_init_budget`, `sched_ss_max_repl`, and other sporadic server parameters

**Returns**:

- Success: Returns 0
- Failure: Returns -1 and sets `errno`:
  - `EINVAL`: Priority in `param` out of range for the current policy, or invalid SCHED_SPORADIC parameters
  - `ESRCH`: The specified task does not exist (invalid PID or already terminated)
  - `EPERM`: Caller lacks permission to modify the target task's scheduling parameters (typically requires superuser or same user)
  - `EFAULT`: `param` points to invalid memory

**Notes**:

- **Policy unchanged**: Modifies scheduling parameters only; does not change the policy. To change both, use `sched_setscheduler()`.
- **Priority range**: Each policy has its valid priority range; query via `sched_get_priority_min()` and `sched_get_priority_max()`. Out-of-range values return `EINVAL`.
- **Takes effect immediately**: Priority modifications take effect immediately; the scheduler reevaluates priorities:
  - If the new priority is higher, the task may preempt the current task immediately
  - If lower, the task may be preempted by higher-priority tasks
  - For blocked tasks, the new priority takes effect upon resumption
- **Priority inheritance**: When implementing priority inheritance for mutexes, this function is often used to temporarily raise a low-priority task's priority to avoid priority inversion.
- **Typical usage**:
  ```c
  struct sched_param param;
  param.sched_priority = 100;  // Set new priority
  
  if (sched_setparam(0, &param) == 0) {
      printf("Priority changed to %d\n", param.sched_priority);
  } else {
      perror("sched_setparam");
  }
  ```
- **Query first**: Use `sched_getparam()` to get current parameters, then modify only the fields you need.
- **Real-time tuning**: In real-time systems, dynamically adjusting priorities based on actual execution can improve responsiveness and throughput.
- **Permission requirement**: Modifying another task's priority typically requires privilege. In embedded systems, where all tasks often run at the same privilege level, this restriction may be looser.
- **Effect on running task**: Modifying the currently running task's priority (pid=0) may trigger immediate rescheduling if a higher-priority task is ready.
- **SCHED_SPORADIC parameters**: For sporadic tasks, other fields in `param` (such as `sched_ss_low_priority`) can also be modified via this function, allowing dynamic adjustment of sporadic server behavior.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sched_getparam

```c
int sched_getparam(pid_t pid, struct sched_param *param);
```

Queries the current scheduling parameters (primarily priority) of the specified task. The standard interface to obtain task priority and other scheduling parameters, commonly used for monitoring, debugging, and dynamic scheduling decisions.

Scheduling parameters include the basic priority and policy-specific parameters (such as the replenish period for sporadic scheduling). Understanding these parameters helps explain task scheduling behavior.

**Parameters**:

- `pid` PID of the target task. Special value 0 queries the calling task. Must be a valid task PID.
- `param` Pointer to a `struct sched_param` that receives the query results. The function fills this structure:
  - `sched_priority`: Basic priority (always set)
  - For SCHED_SPORADIC, also includes `sched_ss_low_priority`, `sched_ss_repl_period`, `sched_ss_init_budget`, `sched_ss_max_repl`, and other sporadic server parameters
  - For other policies (SCHED_FIFO, SCHED_RR, SCHED_OTHER), typically only `sched_priority` is set

**Returns**:

- Success: Returns 0 and fills `param` with the task's scheduling parameters
- Failure: Returns -1 and sets `errno`:
  - `ESRCH`: The specified task does not exist (invalid PID or already terminated)
  - `EINVAL`: `pid` is negative
  - `EFAULT`: `param` points to invalid memory (NULL or unwritable)

**Notes**:

- **Read-only query**: Does not modify task state; lightweight.
- **Full scheduling info**: Typically used with `sched_getscheduler()` for complete scheduling info (policy + parameters):
  ```c
  int policy = sched_getscheduler(0);
  struct sched_param param;
  sched_getparam(0, &param);
  printf("Policy: %d, Priority: %d\n", policy, param.sched_priority);
  ```
- **Typical usage**:
  ```c
  struct sched_param param;
  if (sched_getparam(0, &param) == 0) {
      printf("Current priority: %d\n", param.sched_priority);
  } else {
      perror("sched_getparam");
  }
  ```
- **Dynamic adjustment reference**: Before dynamically adjusting priority, use this function to read current parameters, then modify specific fields, then apply with `sched_setparam()`.
- **Monitoring tools**: System monitoring tools often use this function to show task priorities, aiding scheduling analysis and diagnosis of issues such as priority inversion.
- **Sporadic parameters**: For SCHED_SPORADIC tasks, the function returns the full sporadic server configuration (budget, replenish period, etc.).
- **Parameter initialization**: No need to initialize `param` before calling; the function fully overwrites its contents. Just ensure `param` points to valid memory.
- **Task diagnostics**: When debugging real-time systems, periodically query the priorities of key tasks to verify mechanisms like priority inheritance.
- **Atomicity**: The query is atomic; the returned parameters form a consistent snapshot without partial updates.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Scheduling Control


### sched_yield

```c
int sched_yield(void);
```

Voluntarily yields the CPU, putting the calling task back on the ready queue so the scheduler can select other tasks of equal or higher priority to run. This is a cooperative scheduling mechanism for fairness and responsiveness between tasks.

For SCHED_FIFO, the task is moved to the end of its priority queue; for SCHED_RR, the effect is similar to time slice expiration. This allows tasks of the same priority to get a chance to run.

**Parameters**:

None.

**Returns**:

- Success: Returns 0 (`OK`)
- Failure: Returns -1 and sets `errno` (typically always succeeds)

**Notes**:

- **Cooperative scheduling**: Key to cooperative multitasking; allows tasks to yield the CPU voluntarily, improving overall system responsiveness.
- **Does not lower priority**: `sched_yield()` does not change the task's priority; it only temporarily yields execution. The task remains in the ready queue at the same priority.
- **Policy-specific behavior**:
  - **SCHED_FIFO**: Task moves to the end of its priority queue; if other tasks at the same priority are ready, they run first
  - **SCHED_RR**: Similar to SCHED_FIFO, and the time slice counter is reset
  - **Single-task case**: If no other equal- or higher-priority tasks are ready, the calling task continues immediately (yield has no effect)
- **Typical usage**:
  ```c
  while (processing) {
      // Process a batch of data
      process_data_chunk();
      
      // Voluntarily yield CPU to other tasks
      sched_yield();
  }
  ```
- **Improving responsiveness**: Calling `sched_yield()` periodically in long-running loops avoids starving low-priority tasks and improves interactivity.
- **Polling optimization**: Using `sched_yield()` in polling loops reduces CPU usage and gives other tasks more chances to run:
  ```c
  while (!flag_is_set()) {
      sched_yield();  // Avoid busy-waiting
  }
  ```
  Usually a better solution is blocking waits (semaphore, condition variable).
- **Difference from sleep**:
  - `sched_yield()` does not guarantee the task is blocked; if no other ready task exists, it continues immediately
  - `sleep()` / `usleep()` puts the task to sleep for at least the given time, not consuming CPU during that period
- **Real-time note**: In real-time systems, overuse of `sched_yield()` can cause unpredictability; use sparingly. Prefer explicit synchronization (mutexes, condition variables, semaphores).
- **Highest-priority task**: If the calling task is the highest-priority task in the system, `sched_yield()` typically has no practical effect (returns immediately and continues).
- **Time slice reset**: For SCHED_RR, `sched_yield()` resets the time slice counter, effectively giving up the current slice.
- **Non-preemptive check**: Even if `sched_yield()` returns immediately, a context-switch check occurs, and the scheduler re-evaluates decisions.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sched_get_priority_max

```c
int sched_get_priority_max(int policy);
```

Queries the maximum (highest) priority allowed for the specified scheduling policy. Different policies have different priority ranges; this function returns the valid upper bound to ensure configured priorities fall within the legal range.

In openvela, larger numeric values indicate higher priority (same as Linux, but opposite some RTOSes). Knowing the priority range is essential for correctly configuring real-time tasks.

**Parameters**:

- `policy` Scheduling policy. Valid values include:
  - `SCHED_FIFO` (0)
  - `SCHED_RR` (1)
  - `SCHED_SPORADIC` (2) (if supported)
  - `SCHED_OTHER` (3)
  - Other system-supported policies

**Returns**:

- Success: Returns the maximum priority value for the specified policy (positive integer, typically 255)
- Failure: Returns -1 and sets `errno`:
  - `EINVAL`: `policy` is invalid or unsupported

**Notes**:

- **Use together with the min version**: Typically used with `sched_get_priority_min()` to get the full priority range:
  ```c
  int min_prio = sched_get_priority_min(SCHED_FIFO);
  int max_prio = sched_get_priority_max(SCHED_FIFO);
  printf("SCHED_FIFO priority range: %d to %d\n", min_prio, max_prio);
  ```
- **openvela default range**: In openvela, most policies have a priority range of 1 to 255:
  - 1: Lowest priority (typically the IDLE task)
  - 255: Highest priority (urgent real-time tasks)
  - 100–200: Typical application task priority range
- **Policy independence**: In openvela, all real-time policies (SCHED_FIFO, SCHED_RR, SCHED_SPORADIC) usually share the same priority space, so the max value is the same.
- **Parameter validation**: Before calling `sched_setparam()` or `sched_setscheduler()`, use this function to validate priority to avoid `EINVAL`:
  ```c
  int new_priority = 200;
  if (new_priority <= sched_get_priority_max(SCHED_FIFO)) {
      struct sched_param param = {.sched_priority = new_priority};
      sched_setscheduler(0, SCHED_FIFO, &param);
  }
  ```
- **Portability**: Priority ranges differ between operating systems; using this function produces portable code without hardcoded priority values.
- **Typical usage**:
  ```c
  int max = sched_get_priority_max(SCHED_FIFO);
  if (max < 0) {
      perror("sched_get_priority_max");
  } else {
      printf("Max priority for SCHED_FIFO: %d\n", max);
  }
  ```
- **Real-time configuration**: Key real-time tasks are usually configured near the max priority so they preempt other tasks.
- **Priority layering**: In complex systems, priorities can be partitioned into layers (system, driver, application), each using a distinct subrange.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sched_get_priority_min

```c
int sched_get_priority_min(int policy);
```

Queries the minimum (lowest) priority allowed for the specified scheduling policy. Different policies have different priority ranges; this function returns the valid lower bound to ensure configured priorities fall within the legal range.

In openvela, smaller numeric values indicate lower priority. The minimum priority is typically reserved for background or idle tasks.

**Parameters**:

- `policy` Scheduling policy (same values as `sched_get_priority_max`).

**Returns**:

- Success: Returns the minimum priority for the specified policy (positive integer, typically 1)
- Failure: Returns -1 and sets `errno`:
  - `EINVAL`: `policy` is invalid or unsupported

**Notes**:

- **Use with the max version**:
  ```c
  int min_prio = sched_get_priority_min(SCHED_RR);
  int max_prio = sched_get_priority_max(SCHED_RR);
  printf("SCHED_RR priority range: [%d, %d]\n", min_prio, max_prio);
  ```
- **openvela default value**: In openvela, the minimum priority is typically 1 (priority 0 is sometimes reserved for the system or special uses).
- **IDLE task**: The system idle task typically runs at the minimum priority and only runs when no other tasks are ready.
- **Background tasks**: Low-priority background tasks (logging, statistics) typically use a near-minimum priority to avoid impacting foreground tasks.
- **Parameter validation**: Before setting priority, validate with this function:
  ```c
  int new_priority = 5;
  if (new_priority >= sched_get_priority_min(SCHED_FIFO) &&
      new_priority <= sched_get_priority_max(SCHED_FIFO)) {
      struct sched_param param = {.sched_priority = new_priority};
      sched_setparam(0, &param);
  }
  ```
- **Portability**: Priority ranges differ across operating systems; use this function for portable code.
- **Typical usage**:
  ```c
  int min = sched_get_priority_min(SCHED_FIFO);
  if (min < 0) {
      perror("sched_get_priority_min");
  } else {
      printf("Min priority for SCHED_FIFO: %d\n", min);
  }
  ```
- **Priority assignment strategy**: Avoid using the minimum priority (unless for truly background tasks) to prevent CPU starvation.
- **Policy independence**: In openvela, all real-time policies typically share the same priority space, so the minimum is the same.
- **Debugging tool**: These functions can power generic priority-checking tools to validate system configuration.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sched_rr_get_interval

```c
int sched_rr_get_interval(pid_t pid, struct timespec *interval);
```

Queries the time slice length for a task using the `SCHED_RR` scheduling policy. The time slice is the maximum continuous runtime before the task is forced to yield under SCHED_RR.

This function helps understand the system's scheduling granularity and tune real-time application performance and responsiveness.

**Parameters**:

- `pid` PID of the target task. Special value 0 queries the calling task. Must be a valid task PID.
- `interval` Pointer to a `struct timespec` that receives the time slice length. The function fills this structure:
  - `tv_sec`: Seconds part (typically 0, since time slices are usually less than 1 second)
  - `tv_nsec`: Nanoseconds part (e.g., 10,000,000 ns = 10 ms)

**Returns**:

- Success: Returns 0 (`OK`) and fills `interval`
- Failure: Returns -1 and sets `errno`:
  - `ESRCH`: The specified task does not exist
  - `EINVAL`: `pid` is negative
  - `EFAULT`: `interval` points to invalid memory (NULL or unwritable)
  - `ENOSYS`: System does not support this feature (SCHED_RR not enabled)

**Notes**:

- **SCHED_RR only**: The time slice concept applies only to SCHED_RR. Under SCHED_FIFO, tasks run until they block or are preempted by a higher-priority task; there is no time slice.
- **System-level configuration**: Time slice length is typically a system-wide configuration (compile-time or startup). Querying different tasks typically returns the same value.
- **Typical value**: In openvela, the default time slice is usually 10 ms (10,000,000 ns), adjustable via configuration (such as `CONFIG_RR_INTERVAL`).
- **Typical usage**:
  ```c
  struct timespec ts;
  if (sched_rr_get_interval(0, &ts) == 0) {
      long ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
      printf("Time slice: %ld ms\n", ms);
  } else {
      perror("sched_rr_get_interval");
  }
  ```
- **Performance tuning**: Knowing the time slice helps tune task design:
  - If a task's critical operations approach the time slice, consider optimizing the algorithm or using SCHED_FIFO
  - If multiple same-priority SCHED_RR tasks need to share CPU fairly, ensure work units are smaller than the time slice
- **Real-time analysis**: The time slice is an important parameter in response-time analysis and affects worst-case response time.
- **Slice exhaustion**: When a SCHED_RR task's time slice is exhausted, the task moves to the end of its priority queue and the time slice is reset.
- **sched_yield()**: For SCHED_RR tasks, calling `sched_yield()` resets the time slice counter.
- **Non-SCHED_RR tasks**: Even if the task is not currently using SCHED_RR, this function typically returns the system default time slice, but the value is not meaningful for non-SCHED_RR tasks.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sched_lock

```c
void sched_lock(void);
```

Disables task scheduling (preemption). After calling, the current task cannot be preempted by other tasks of equal or higher priority until `sched_unlock()` is called to restore scheduling. Supports nested calls; each `sched_lock()` must be paired with a corresponding `sched_unlock()`.

**Parameters**:

None.

**Returns**:

No return value.

**Notes**:

- **Nested support**: `sched_lock()` maintains a lock counter that increments on each call; `sched_unlock()` decrements it. Scheduling only truly resumes when the counter reaches zero.
- **Does not affect interrupts**: `sched_lock()` only disables task-level preemption, not interrupts. Interrupt handlers may still execute.
- **Difference from disabling interrupts**:
  - `sched_lock()`: Disables task switching; interrupts still respond
  - `enter_critical_section()`: Disables interrupts; stronger protection but longer latency
- **Typical usage**:
  ```c
  sched_lock();
  // Critical section: not preempted by other tasks
  update_shared_data();
  sched_lock();  // Nested call
  do_more_work();
  sched_unlock(); // Counter decrements; still locked
  sched_unlock(); // Counter reaches 0; scheduling resumes
  ```
- **SMP note**: On multi-core systems, `sched_lock()` only protects scheduling on the current CPU; tasks on other CPUs still run. For cross-core protection, use spinlocks or other SMP synchronization.
- **Avoid long holds**: Disabling scheduling for long periods impacts real-time performance; keep critical sections short.

**POSIX Compatibility**: openvela/NuttX extension interface (non-POSIX standard).


### sched_unlock

```c
void sched_unlock(void);
```

Resumes task scheduling (preemption). Decrements the scheduling lock counter; when it reaches zero, normal scheduling resumes. Must be paired with `sched_lock()`.

**Parameters**:

None.

**Returns**:

No return value.

**Notes**:

- Each `sched_unlock()` corresponds to one `sched_lock()`; do not call extra.
- When the counter reaches zero, if a higher-priority task is ready, a context switch occurs immediately.

**POSIX Compatibility**: openvela/NuttX extension interface (non-POSIX standard).


### sched_lockcount

```c
int sched_lockcount(void);
```

Queries the current task's scheduling lock nesting count. The return value indicates how many `sched_lock()` calls have not yet been matched by a corresponding `sched_unlock()`.

**Parameters**:

None.

**Returns**:

Returns the current task's scheduling lock count (non-negative integer). 0 means scheduling is not locked.

**Notes**:

- Mainly used for debugging to verify that `sched_lock()` / `sched_unlock()` are correctly paired.
- Typical usage:
  ```c
  int count = sched_lockcount();
  if (count > 0) {
      printf("Scheduler locked, count=%d\n", count);
  }
  ```

**POSIX Compatibility**: openvela/NuttX extension interface (non-POSIX standard).


## CPU Affinity


### sched_getcpu

```c
int sched_getcpu(void);
```

Gets the CPU core number on which the calling task is currently running. Only meaningful in SMP (symmetric multiprocessing) systems; used to query task-to-CPU binding.

On multi-core systems, knowing which CPU a task runs on helps with performance analysis, debugging, and optimizing CPU affinity strategies.

**Parameters**:

None.

**Returns**:

- Success: Returns the current CPU number (non-negative integer, starting from 0)
  - 0: First CPU core
  - 1: Second CPU core
  - ...and so on
- Failure: Returns -1 and sets `errno` (typically does not fail)
  - `ENOSYS`: System does not support SMP (`CONFIG_SMP` not enabled)

**Notes**:

- **SMP only**: Valid only in multi-core systems with `CONFIG_SMP`. On single-core systems, typically returns 0.
- **Instantaneous value**: The returned CPU number is the value at query time. In preemptive multitasking, the task may migrate to another CPU immediately after the function returns.
- **Typical usage**:
  ```c
  int cpu = sched_getcpu();
  if (cpu >= 0) {
      printf("Running on CPU %d\n", cpu);
  } else {
      perror("sched_getcpu");
  }
  ```
- **Performance analysis**: Recording which CPU a task runs on helps analyze CPU load distribution and migration frequency.
- **Debugging**: When debugging CPU affinity issues, periodically query the CPU number to verify a task runs on the expected CPU.
- **Used with affinity**: Typically combined with `sched_setaffinity()` and `sched_getaffinity()`:
  ```c
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(2, &set);  // Bind to CPU 2
  sched_setaffinity(0, sizeof(set), &set);
  
  int cpu = sched_getcpu();
  assert(cpu == 2);  // Verify binding succeeded
  ```
- **Not reliable for synchronization**: Do not use this for synchronization; the return value may become stale before use.
- **NUMA systems**: On NUMA architectures, knowing which CPU a task is on helps optimize memory access patterns and reduce cross-node memory access.
- **Hotspot analysis**: If multiple tasks frequently run on the same CPU, this may indicate load imbalance, needing affinity or priority tuning.
- **Interrupt context**: This function can also be called in interrupt handlers to query on which CPU the interrupt is handled.
- **Thread binding**: In multi-threaded apps, different threads can be bound to different CPUs, then verified using this function.

**POSIX Compatibility**: Compatible with the Linux extension interface (non-POSIX standard, but widely supported).


### sched_setaffinity

```c
int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);
```

Sets the CPU affinity mask of a task, specifying the set of CPU cores on which the task may run. Valid only in SMP systems; requires `CONFIG_SMP`.

CPU affinity allows binding a task to specific CPU cores, useful for optimizing cache locality, reducing context-switch overhead, and isolating critical tasks.

**Parameters**:

- `pid` PID of the target task. Special value 0 sets the calling task's affinity. Must be a valid task PID.
- `cpusetsize` Size (in bytes) of the CPU set `mask` points to. Typically `sizeof(cpu_set_t)`. Allows future expansion to support more CPUs.
- `mask` Pointer to the CPU affinity mask (`cpu_set_t`). Each bit corresponds to a CPU:
  - Bit 1: Task allowed to run on that CPU
  - Bit 0: Not allowed
  Manipulate the mask with `CPU_ZERO()`, `CPU_SET()`, `CPU_CLR()`, etc.

**Returns**:

- Success: Returns 0 (`OK`)
- Failure: Returns -1 and sets `errno`:
  - `ESRCH`: The specified task does not exist
  - `EINVAL`: The CPU set in `mask` is invalid (for example, all zeros or contains non-existent CPUs)
  - `EFAULT`: `mask` points to invalid memory (NULL or unreadable)
  - `EPERM`: Caller lacks permission to modify the target task's affinity (typically requires superuser or same user)
  - `ENOSYS`: System does not support SMP (`CONFIG_SMP` not enabled)

**Notes**:

- **SMP only**: Valid only in multi-core systems with `CONFIG_SMP`. Single-core systems typically return `ENOSYS`.
- **Takes effect immediately**: Affinity changes take effect immediately. If the task currently runs on a CPU not in the new mask, the scheduler migrates it to an allowed CPU.
- **Typical usage**:
  ```c
  cpu_set_t set;
  CPU_ZERO(&set);           // Clear mask
  CPU_SET(0, &set);         // Allow CPU 0
  CPU_SET(1, &set);         // Allow CPU 1
  
  if (sched_setaffinity(0, sizeof(set), &set) == 0) {
      printf("Affinity set to CPU 0 and 1\n");
  } else {
      perror("sched_setaffinity");
  }
  ```
- **Bind to a single CPU**:
  ```c
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(2, &set);  // Only allow CPU 2
  sched_setaffinity(0, sizeof(set), &set);
  ```
- **Performance optimization**:
  - **Cache locality**: Binding to a specific CPU improves cache hit rate and reduces cache invalidation
  - **Less migration overhead**: Avoids frequent task migration between CPUs and reduces context-switch cost
  - **Load isolation**: Binding critical real-time tasks to dedicated CPUs avoids interference from other tasks
- **NUMA systems**: On NUMA architectures, bind tasks to CPUs near their accessed memory to reduce cross-node latency.
- **Permission**: Modifying another task's affinity typically requires privileges. In embedded systems, this restriction may be looser.
- **Child inheritance**: Children (created via `fork()` or `task_create()`) typically inherit the parent's CPU affinity.
- **Used with getaffinity**: Typically combined with `sched_getaffinity()` to read, modify, then set back:
  ```c
  cpu_set_t set;
  sched_getaffinity(0, sizeof(set), &set);
  CPU_CLR(3, &set);  // Remove CPU 3
  sched_setaffinity(0, sizeof(set), &set);
  ```
- **Verify the setting**: After setting, use `sched_getcpu()` to verify the task runs on the expected CPU.
- **Pitfalls**:
  - If `mask` is all zeros (no CPU allowed), returns `EINVAL`
  - If the mask specifies a CPU outside system range (e.g., CPU 5 on a 4-core system), returns `EINVAL`
  - Over-binding may cause load imbalance with some CPUs overloaded and others idle
- **Dynamic adjustment**: Adjusting affinity at runtime based on load enables flexible load balancing.

**POSIX Compatibility**: Compatible with the Linux extension interface (non-POSIX standard, but widely supported).


### sched_getaffinity

```c
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
```

Queries the CPU affinity mask of a task. Valid only in SMP systems; requires `CONFIG_SMP`.

Querying CPU affinity helps understand the task's CPU binding strategy for monitoring, debugging, and dynamically adjusting affinity.

**Parameters**:

- `pid` PID of the target task. Special value 0 queries the calling task. Must be a valid task PID.
- `cpusetsize` Size (in bytes) of the CPU set `mask` points to. Typically `sizeof(cpu_set_t)`.
- `mask` Pointer to the CPU affinity mask that receives the query results. The function fills this `cpu_set_t`, where:
  - Bit 1: Task allowed to run on that CPU
  - Bit 0: Not allowed

**Returns**:

- Success: Returns 0 (`OK`) and fills `mask`
- Failure: Returns -1 and sets `errno`:
  - `ESRCH`: The specified task does not exist
  - `EINVAL`: `cpusetsize` is too small to hold the system's CPU count
  - `EFAULT`: `mask` points to invalid memory (NULL or unwritable)
  - `ENOSYS`: System does not support SMP

**Notes**:

- **SMP only**: Valid only in multi-core systems with `CONFIG_SMP`. Single-core systems typically return `ENOSYS`.
- **Read-only query**: Does not modify task state; lightweight.
- **Typical usage**:
  ```c
  cpu_set_t set;
  if (sched_getaffinity(0, sizeof(set), &set) == 0) {
      printf("Task can run on CPUs: ");
      for (int i = 0; i < CPU_SETSIZE; i++) {
          if (CPU_ISSET(i, &set)) {
              printf("%d ", i);
          }
      }
      printf("\n");
  } else {
      perror("sched_getaffinity");
  }
  ```
- **Check a specific CPU**:
  ```c
  cpu_set_t set;
  sched_getaffinity(0, sizeof(set), &set);
  if (CPU_ISSET(2, &set)) {
      printf("Task can run on CPU 2\n");
  }
  ```
- **Query before modify**: When modifying affinity, typically query first and then modify relative to the current value:
  ```c
  cpu_set_t set;
  sched_getaffinity(0, sizeof(set), &set);
  CPU_CLR(1, &set);  // Remove CPU 1
  sched_setaffinity(0, sizeof(set), &set);
  ```
- **Monitoring tools**: System monitoring tools often use this function to show task CPU binding and help analyze load distribution.
- **Debugging affinity issues**: If a task's performance is anomalous, query affinity to check whether it is bound to an overloaded CPU.
- **Count CPUs**:
  ```c
  cpu_set_t set;
  sched_getaffinity(0, sizeof(set), &set);
  int count = CPU_COUNT(&set);
  printf("Task can run on %d CPUs\n", count);
  ```
- **Default affinity**: Newly created tasks default to all CPUs allowed (unless the parent has restrictions). All CPU bits will be 1.
- **CPU_SETSIZE constant**: `CPU_SETSIZE` defines the maximum CPU count supported by `cpu_set_t` (typically 1024), but the actual number of system CPUs may be smaller.
- **CPU set macros**:
  - `CPU_ZERO(&set)`: Clear the set
  - `CPU_SET(cpu, &set)`: Add a CPU
  - `CPU_CLR(cpu, &set)`: Remove a CPU
  - `CPU_ISSET(cpu, &set)`: Check whether a CPU is in the set
  - `CPU_COUNT(&set)`: Count CPUs in the set
  - `CPU_EQUAL(&set1, &set2)`: Compare two sets for equality
- **Portability**: `cpu_set_t` sizes differ across systems; always use `sizeof(cpu_set_t)` rather than hardcoding.
- **Used with getcpu**:
  ```c
  int cpu = sched_getcpu();
  cpu_set_t set;
  sched_getaffinity(0, sizeof(set), &set);
  assert(CPU_ISSET(cpu, &set));  // Current CPU should be in the affinity set
  ```

**POSIX Compatibility**: Compatible with the Linux extension interface (non-POSIX standard, but widely supported).


### sched_cpucount

```c
int sched_cpucount(const cpu_set_t *set);
```

Counts the number of CPUs in a CPU set. Equivalent to Linux's `CPU_COUNT()` macro.

**Parameters**:

- `set` Pointer to a CPU set.

**Returns**:

Returns the number of CPUs set in the set.

**Notes**:

- On non-SMP systems, the macro is defined to always return 1.
- Typical usage:
  ```c
  cpu_set_t set;
  sched_getaffinity(0, sizeof(set), &set);
  printf("Can run on %d CPUs\n", sched_cpucount(&set));
  ```

**POSIX Compatibility**: Compatible with the Linux extension interface (non-POSIX standard).


## Debugging and Diagnostics


### sched_backtrace

```c
int sched_backtrace(pid_t tid, void **buffer, int size, int skip);
```

Gets the call stack backtrace of the specified task. Stack frame addresses are stored into the `buffer` array for debugging and crash analysis.

**Parameters**:

- `tid` PID of the target task. 0 means the current task.
- `buffer` Pointer to an array of pointers that stores stack frame addresses.
- `size` Maximum capacity of `buffer` (number of elements).
- `skip` Number of stack frames to skip (from the top), used to filter the debug framework's own frames.

**Returns**:

Returns the number of frames actually obtained (non-negative). If the returned value equals `size`, more frames may exist.

**Notes**:

- Requires `CONFIG_SCHED_BACKTRACE` to be enabled. When disabled, the macro expands to return 0.
- When getting another task's call stack, the target task should be blocked; otherwise results may be inaccurate.

**POSIX Compatibility**: openvela/NuttX extension interface (non-POSIX standard).


### sched_dumpstack

```c
void sched_dumpstack(pid_t tid);
```

Prints the call stack of the specified task to the system log. Internally calls `sched_backtrace()` to get frames and then formats the output.

**Parameters**:

- `tid` PID of the target task. 0 means the current task.

**Returns**:

No return value.

**Notes**:

- Mainly used for debugging and crash analysis; output goes to the system log (syslog).
- Requires `CONFIG_SCHED_BACKTRACE` to be enabled.

**POSIX Compatibility**: openvela/NuttX extension interface (non-POSIX standard).
