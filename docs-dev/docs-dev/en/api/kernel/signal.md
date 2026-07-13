\[ English | [简体中文](../../../zh-cn/api/kernel/signal.md) \]

# Signal API

openvela provides a complete POSIX signal mechanism for asynchronous communication and event notification between processes and threads.

Header: `#include <signal.h>`

## openvela Implementation Notes

- **Signal range**: Standard signals 1~31, real-time signals `SIGRTMIN`(32) ~ `SIGRTMAX`(63)
- **Default actions**: In openvela, the default action for most signals is to ignore (unlike Linux), unless the corresponding configuration is enabled
- **Real-time signal features**: Supports queuing, carrying additional data (`sigqueue`), and FIFO delivery order
- **`SIGKILL`/`SIGSTOP`**: Cannot be caught, blocked, or ignored
- **Signal stack**: `sigaltstack()` does not currently support `SS_ONSTACK`, only `SS_DISABLE` is supported
- **Deprecated interfaces**: `signal()`, `sighold()`, `sigrelse()`, `sigignore()`, `sigset()`, `sigpause()` are legacy interfaces; it is recommended to use `sigaction()` and `sigprocmask()` instead

## Signal Overview

A signal is a software interrupt mechanism that allows the kernel or other processes to send asynchronous notifications to a target process.

### Signal Types

1. **Standard signals** (1~31): Not queued, do not carry extra data, most have predefined default actions
2. **Real-time signals** (`SIGRTMIN`~`SIGRTMAX`): Support queuing, can carry additional data, delivered in FIFO order

### Signal Handling Modes

1. **Ignore** (`SIG_IGN`): The signal is discarded
2. **Default handling** (`SIG_DFL`): Execute the default action
3. **Custom handling**: Register a signal handler function

### Signal Mask

Each thread has an independent signal mask; blocked signals remain pending until unblocked.


## Signal Sending


### kill

```c
int kill(pid_t pid, int signo);
```

Send a signal to the specified process or process group. This is the most basic and commonly used signal-sending function, used for inter-process communication, process control, and event notification.

The name `kill()` is historical, but it is not only used to "kill" processes; it can also send any signal for communication. Signal 0 is a special "null signal" that is not actually delivered but performs error checking and can be used to test if a process exists.

**Parameters**:

- `pid` Target process or process group identifier. Values:
  - `> 0`: Send the signal to the process with the specified PID.
  - `0`: Send the signal to all processes in the calling process's process group (broadcast).
  - `-1`: Send the signal to all processes to which the caller has permission (except init and itself); requires superuser privileges. This is a system-wide broadcast.
  - `< -1`: Send the signal to all processes in the process group whose ID is `|pid|` (absolute value). For example, `kill(-100, SIGTERM)` sends SIGTERM to process group 100.
- `signo` Signal number to send (1-63). Valid signals are defined in `<signal.h>` (such as `SIGTERM`, `SIGKILL`). The special value 0 means the "null signal"; no actual signal is sent, only error checking is performed (used to detect whether a process exists).

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set:

- `EINVAL` Invalid signal number (less than 0 or greater than `MAX_SIGNO`).
- `ESRCH` The specified process or process group does not exist, or the process has terminated.
- `EPERM` The calling process does not have permission to send the signal to the target process. Usually you can only send signals to processes of the same user or to child processes.

**Note**:

- `SIGKILL` (signal 9) and `SIGSTOP` (signal 19) are special signals that cannot be caught, blocked, or ignored, guaranteeing that the target process can be terminated or stopped.
- Sending a signal is only a request; the target process may ignore the signal (if the signal handler is set to `SIG_IGN`) or block the signal.
- Signals may not be delivered immediately; if the target process blocks the signal, it will remain pending until unblocked.
- In a multithreaded program, a signal is delivered to one of the threads in the process that has not blocked the signal (chosen by the system). To send a signal to a specific thread, use `pthread_kill()` or `tgkill()`.
- Using `kill(pid, 0)` allows you to test whether a process exists: returns 0 if it exists; returns -1 with `errno` set to `ESRCH` if it does not exist.
- When sending a signal to a process group, if the group is empty or the caller has no access to any process, `ESRCH` is returned.
- Some signals have special semantics; for example, `SIGCHLD` notifies a parent process of a child's state change, and `SIGPIPE` is produced when writing to a closed pipe.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### killpg

```c
int killpg(pid_t pgrp, int signo);
```

Send a signal to all processes in the specified process group. Equivalent to `kill(-pgrp, signo)`.

**Parameters**:

- `pgrp` Target process group ID. If 0, the signal is sent to the process group of the calling process.
- `signo` Signal number to send.

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set:

- `EINVAL` Invalid signal number.
- `ESRCH` The specified process group does not exist.
- `EPERM` No permission to send the signal to the target process group.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### tgkill

```c
int tgkill(pid_t pid, pid_t tid, int signo);
```

Send a signal to a specific thread in the specified thread group (process). This is the safest way to send a signal to a specific thread in a multithreaded program.

**Parameters**:

- `pid` Target process (thread group) ID. If -1, this parameter is ignored and the target thread is located by `tid` alone.
- `tid` Target thread ID.
- `signo` Signal number to send.

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set:

- `EINVAL` Invalid signal number.
- `ESRCH` The thread does not exist or does not belong to the specified process.
- `EPERM` No permission to send the signal to the target thread.

**Note**:

- Safer than `pthread_kill()` because it verifies that the thread belongs to the expected process.
- Avoids sending a signal to a wrong thread whose thread ID has been recycled.

**POSIX Compatibility**: Compatible with the Linux extension interface.


### raise

```c
int raise(int signo);
```

Send a signal to the calling thread itself. This is the standard way for a process or thread to send a signal to itself, equivalent to:
- Single-threaded program: `kill(getpid(), signo)`
- Multithreaded program: `pthread_kill(pthread_self(), signo)`

`raise()` is often used to trigger signal handling on purpose, such as self-termination, triggering a breakpoint, or testing a signal handler.

**Parameters**:

- `signo` Signal number to send (1-63). Common values include:
  - `SIGABRT`: Abnormal termination (for example, as used by `abort()`)
  - `SIGTERM`: Request termination
  - `SIGUSR1`/`SIGUSR2`: User-defined signals
  - `SIGTRAP`: Trigger a debugger breakpoint

**Returns**:

Returns 0 on success, or a non-zero value on failure.

**Note**:

- If the signal's handling action is to terminate the process (such as the default action of `SIGTERM` when enabled), `raise()` does not return and the process terminates directly.
- The signal is handled immediately (if not blocked) or pended (if blocked), after which the function returns.
- If the signal is blocked, it remains pending until unblocked, at which point `raise()` has already returned.
- Calling `raise()` within a signal handler to send the current signal may cause recursion unless the `SA_NODEFER` flag is set.
- `raise()` is thread-safe; in a multithreaded program it only affects the calling thread.
- Compared to `kill(getpid(), signo)`, `raise()` is more efficient and semantically clearer.
- Common usage:
  - `raise(SIGABRT)` - Terminate the program abnormally (equivalent to `abort()`)
  - `raise(SIGTERM)` - Self-termination
  - `raise(SIGUSR1)` - Trigger user-defined signal handling

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigqueue

```c
int sigqueue(int pid, int signo, const union sigval value);
```

Send a signal with data to the specified process. This is an enhanced version of `kill()` that supports passing additional data (an integer or pointer) along with the signal. It is mainly used for real-time signals and communication scenarios that need to carry data.

Key differences from `kill()`:
1. **Carries data**: An integer or pointer can be passed to the receiving process via `value`.
2. **Queuing supported**: Real-time signals are queued; sending multiple times queues them all for delivery.
3. **More detailed information**: The receiver can obtain the signal source and carried data through `siginfo_t`.

`sigqueue()` is often used for real-time signal communication, event notifications, asynchronous I/O completion notifications, and other scenarios that need to carry additional information.

**Parameters**:

- `pid` Target process ID. Must be > 0; does not support process groups (0 or negative values cannot be used).
- `signo` Signal number to send (1-63). Although standard signals can be used, real-time signals (`SIGRTMIN` to `SIGRTMAX`) are recommended because:
  - Real-time signals support queuing (multiple sends are all delivered)
  - Standard signals are not queued (multiple sends may only be delivered once)
  - Real-time signals are delivered in FIFO order
- `value` Data passed with the signal. The `union sigval` type contains two members (only one can be used):
  - `sival_int`: Pass an integer value, such as an error code, sequence number, or counter.
  - `sival_ptr`: Pass a pointer value. Note that a pointer may be invalid in the receiving process (unless it is a shared memory address).

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set:

- `EINVAL` Invalid signal number (<= 0 or > `MAX_SIGNO`).
- `ESRCH` The specified process does not exist or has terminated.
- `EPERM` The calling process has no permission to send the signal to the target process (different user, different session, etc.).
- `EAGAIN` Signal queue resources are exhausted. The system limits the number of pending signals per process (typically several hundred); exceeding the limit returns this error. This mainly affects real-time signals.

**Note**:

- **Real-time signal queuing**: Real-time signals (`SIGRTMIN`-`SIGRTMAX`) support queuing; sending the same signal multiple times queues them for delivery without loss. Each signal instance carries its own independent `value`.
- **Standard signals do not queue**: Standard signals (such as `SIGUSR1`, `SIGTERM`) are not queued; multiple sends may only be delivered once, and subsequent `value`s may be lost.
- **Receiving data**: The receiver must use one of the following methods to obtain `value`:
  - When setting a handler with `sigaction()`, set the `SA_SIGINFO` flag and use a three-parameter handler:
    ```c
    void handler(int sig, siginfo_t *info, void *context) {
        int data = info->si_value.sival_int;
        // or void *ptr = info->si_value.sival_ptr;
    }
    ```
  - Or use `sigwaitinfo()`/`sigtimedwait()` to wait synchronously for a signal and obtain `siginfo_t`.
- **Pointer parameter caveat**: `sival_ptr` is usually invalid across processes because each process has its own address space. It is only meaningful when:
  - It is a shared-memory address
  - It is passed to another thread in the same process (using `pthread_sigqueue()`, if available)
  - The "pointer" is actually an encoded integer value rather than a real pointer
- **Queuing limit**: The system imposes a limit on pending signals (`SIGQUEUE_MAX`), which can be queried via `sysconf(_SC_SIGQUEUE_MAX)`. Exceeding the limit returns `EAGAIN`.
- **Signal priority**: Real-time signals have an implicit priority; signals with smaller numbers are delivered first. `SIGRTMIN` has the highest priority and `SIGRTMAX` the lowest.
- **Typical usage**:
  ```c
  // Sender
  union sigval val;
  val.sival_int = 42;  // or any data
  sigqueue(target_pid, SIGRTMIN, val);
  
  // Receiver
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGRTMIN, &sa, NULL);
  ```
- **Choosing between `kill()` and `sigqueue()`**:
  - If you don't need to carry data and are using standard signals, `kill()` is simpler.
  - If you need to carry data or use real-time signals, use `sigqueue()`.
- `sigqueue()` sets `si_code` in `siginfo_t` to `SI_QUEUE`, which can be used to distinguish the signal source.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.



## Signal Handler Setup


### sigaction

```c
int sigaction(int signo, const struct sigaction *act, struct sigaction *oact);
```

Set or query the handling action for a signal. This is the preferred method for setting a signal handler, offering more control and more predictable behavior than `signal()`. `sigaction()` is the POSIX-recommended interface for signal handling.

The `struct sigaction` structure allows precise control over signal handling behavior, including the handler function, signal mask, and various flags. It is more powerful and flexible than the simple `signal()` interface.

**Parameters**:

- `signo` Signal number to configure (1-63). Cannot be `SIGKILL` (9) or `SIGSTOP` (19) because the handling of these two signals cannot be modified.
- `act` Pointer to the new signal handling action structure. If `NULL`, the current handling action is not modified; only `oact` is used for querying. Structure fields:
  - `sa_handler` or `sa_sigaction`: Signal handler function.
    - `SIG_DFL`: Restore the default action.
    - `SIG_IGN`: Ignore the signal.
    - Function pointer: Custom handler function.
  - `sa_mask`: Signal mask specifying signals to be additionally blocked during the handler's execution. The signal being handled is automatically blocked (unless `SA_NODEFER` is set).
  - `sa_flags`: Flags controlling signal handling behavior (see below).
- `oact` Pointer used to save the previous handling action. If `NULL`, the previous action is not returned. Can be used to save and later restore the original handling action.

**`sa_flags` description**:

- `SA_SIGINFO` (0x02): Use the extended three-parameter handler `sa_sigaction(int sig, siginfo_t *info, void *context)` instead of the simple `sa_handler(int sig)`. This allows obtaining detailed signal information (sender PID, signal value, etc.).
- `SA_RESTART` (0x10): System calls interrupted by the signal are automatically restarted instead of returning `EINTR`. This simplifies error handling and avoids the need to manually retry interrupted system calls.
- `SA_NODEFER` (0x20): Do not automatically block the signal being handled. By default, signal X is blocked while handling signal X to prevent recursion. Setting this flag allows recursive invocation of the signal handler.
- `SA_RESETHAND` (0x40): Reset the handling action to `SIG_DFL` automatically after the signal is delivered (one-shot handler). Similar to the old, unreliable signal semantics.
- `SA_ONSTACK` (0x08): Execute the handler on the alternate signal stack (first configured with `sigaltstack()`). Used to prevent the stack-overflow signal handler from itself overflowing the stack.
- `SA_NOCLDSTOP` (0x01): If `signo` is `SIGCHLD`, no signal is generated when a child stops (`SIGSTOP`) or resumes (`SIGCONT`); only termination produces a signal.
- `SA_NOCLDWAIT` (0x04): If `signo` is `SIGCHLD`, terminating child processes are reaped automatically, zombie processes are not created, and the parent does not need to call `wait()`.

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set:

- `EINVAL` `signo` is invalid, or an attempt is made to modify the handling action of `SIGKILL`/`SIGSTOP`.
- `EFAULT` `act` or `oact` points to an invalid memory address (segfault).

**Note**:

- Signal handlers should be kept as short and efficient as possible and must avoid calling non-reentrant functions (such as `malloc()` or `printf()`). Only call async-signal-safe functions.
- `sa_mask` takes effect during handler execution and is automatically restored after the handler returns.
- It is safe to call `sigaction()` multiple times to modify the handling of the same signal; the new action replaces the old one.
- To temporarily change and restore signal handling, the pattern is:
  ```c
  struct sigaction old_act;
  sigaction(SIGINT, &new_act, &old_act);  // set new handler
  // ... do something ...
  sigaction(SIGINT, &old_act, NULL);       // restore old handler
  ```
- When modifying global variables in a signal handler, declare them as `volatile sig_atomic_t` to ensure atomicity and visibility.
- Using the `SA_SIGINFO` flag allows obtaining detailed signal information, such as the sender PID and data carried by the signal, which is useful for debugging and advanced signal handling.
- `sa_mask` should include all signals that need to be blocked during handler execution to prevent race conditions caused by the handler being interrupted by other signals.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### signal

```c
sighandler_t signal(int signo, sighandler_t handler);
```

Set the signal handler function. This is a simplified version of `sigaction()`, but its behavior may vary across systems. `sigaction()` is recommended.

**Parameters**:

- `signo` Signal number. Cannot be `SIGKILL` or `SIGSTOP`.
- `handler` Signal handler:
  - `SIG_IGN` Ignore the signal.
  - `SIG_DFL` Use the default action.
  - A user-defined function pointer with prototype `void handler(int signo)`.

**Returns**:

Returns the previous signal handler on success, or `SIG_ERR` on failure with `errno` set.

**Note**:

- The handler should be kept short and must only call async-signal-safe functions.
- In openvela, `signal()` behaves consistently with BSD semantics: the handler is not reset to the default after handling, and interrupted system calls are automatically restarted.
- For scenarios requiring precise control over signal behavior, use `sigaction()`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigset

```c
sighandler_t sigset(int signo, sighandler_t handler);
```

Set the signal handler (similar to `signal`, but supports `SIG_HOLD`).

**Parameters**:

- `signo` Signal number.
- `handler` Signal handler; can be `SIG_IGN`, `SIG_DFL`, `SIG_HOLD`, or a user-defined function.

**Returns**:

Returns the previous signal handler on success, or `SIG_ERR` on failure.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name (obsolete).


### sigignore

```c
int sigignore(int signo);
```

Set the handling of the specified signal to ignore.

**Parameters**:

- `signo` Signal number to ignore.

**Returns**:

Returns 0 on success, or -1 on failure.


**Note**:

- Deprecated interface, equivalent to setting `SIG_IGN` via `sigaction()`. Use `sigaction()` instead.
- `SIGKILL` and `SIGSTOP` cannot be ignored.
**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name (obsolete).


### siginterrupt

```c
int siginterrupt(int signo, int flag);
```

Set whether the signal interrupts system calls.

**Parameters**:

- `signo` Signal number.
- `flag` If non-zero, the signal interrupts system calls; otherwise, system calls are automatically restarted.

**Returns**:

Returns 0 on success, or -1 on failure.


**Note**:

- Non-zero `flag`: Clears `SA_RESTART`; interrupted system calls return `EINTR`.
- Zero `flag`: Sets `SA_RESTART`; interrupted system calls are automatically restarted.
**POSIX Compatibility**: Compatible with the BSD extension interface.


## Signal Set Operations


### sigemptyset

```c
int sigemptyset(sigset_t *set);
```

Initialize a signal set to be empty (contains no signals). This initialization is required before using a signal set. The contents of an uninitialized signal set are indeterminate and using it directly leads to undefined behavior.

Once initialized as empty, individual signals can be added with `sigaddset()` to build a custom signal set.

**Parameters**:

- `set` Pointer to the signal set to initialize. Must be an allocated `sigset_t` variable (automatic, static, or dynamically allocated).

**Returns**:

Returns 0 on success. According to POSIX, this function always succeeds, but some implementations may return -1 if the argument is invalid.

**Note**:

- **Must be initialized**: Every signal set must be initialized with `sigemptyset()` or `sigfillset()` before use. Do not assume that a newly allocated signal set is empty; the initial value of an automatic variable is indeterminate.
- An initialized empty set contains no signals, and membership tests (`sigismember()`) return 0 for every signal.
- Typical usage pattern:
  ```c
  sigset_t set;
  sigemptyset(&set);           // initialize to empty
  sigaddset(&set, SIGINT);     // add SIGINT
  sigaddset(&set, SIGTERM);    // add SIGTERM
  // set now contains SIGINT and SIGTERM
  ```
- An empty set is commonly used with `sigprocmask(SIG_SETMASK, &empty_set, ...)` to unblock all signals.
- Even if a signal set has been initialized, `sigemptyset()` can be called again to clear it.
- Pairing with `sigfillset()`: use `sigemptyset()` + `sigaddset()` to build sparse signal sets (containing a few signals); use `sigfillset()` + `sigdelset()` to build dense signal sets (excluding a few signals).

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigfillset

```c
int sigfillset(sigset_t *set);
```

Initialize a signal set to be full (contains all valid signals). After initialization, the set contains every signal supported by the system (1 to `MAX_SIGNO`, i.e., 1-63).

A full set is often used when all signals need to be blocked, or as a starting point from which unwanted signals are removed with `sigdelset()`.

**Parameters**:

- `set` Pointer to the signal set to initialize.

**Returns**:

Returns 0 on success. According to POSIX, this function always succeeds, but some implementations may return -1 if the argument is invalid.

**Note**:

- The full set contains all signals (including `SIGKILL` and `SIGSTOP`), but `SIGKILL` and `SIGSTOP` cannot actually be blocked even if they are in a signal set.
- The number of signals included in the full set depends on the system; in openvela it is 63 (`MIN_SIGNO=1` to `MAX_SIGNO=63`).
- Typical usage pattern:
  ```c
  sigset_t set;
  sigfillset(&set);             // initialize to full
  sigdelset(&set, SIGUSR1);     // remove SIGUSR1
  sigdelset(&set, SIGUSR2);     // remove SIGUSR2
  sigprocmask(SIG_SETMASK, &set, NULL);  // block all except SIGUSR1/SIGUSR2
  ```
- Common applications:
  - Block all signals to protect a critical section: `sigfillset(&set); sigprocmask(SIG_BLOCK, &set, ...);`
  - Use in `sa_mask` of `sigaction()` to block all other signals while a signal is being handled
  - Create a "reverse" signal set: fill first, then remove unwanted signals
- Choosing between `sigfillset()` and `sigemptyset()`:
  - Use `sigemptyset()` + `sigaddset()` if only a few signals are needed.
  - Use `sigfillset()` + `sigdelset()` if a few signals should be excluded.
- Even if a signal set has been initialized, `sigfillset()` can be called again to refill it.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigaddset

```c
int sigaddset(sigset_t *set, int signo);
```

Add the specified signal to a signal set. If the signal is already in the set, the operation has no effect (idempotent).

This is the fundamental operation for building a custom signal set, typically used after `sigemptyset()` initialization to add needed signals one by one.

**Parameters**:

- `set` Pointer to the signal set to modify. Must have been initialized with `sigemptyset()` or `sigfillset()`.
- `signo` Signal number to add (1-63). Must be a valid signal number; invalid values lead to undefined behavior or return an error.

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set:

- `EINVAL` `signo` is not a valid signal number (<= 0 or > `MAX_SIGNO`).

**Note**:

- Must operate on an initialized signal set; otherwise behavior is undefined.
- Adding a signal that already exists produces no error or side effect.
- Multiple calls can be made to add multiple signals:
  ```c
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGUSR1);
  ```
- `SIGKILL` and `SIGSTOP` can be added to a signal set, but they are ignored when actually used (such as with `sigprocmask()`).
- Common uses:
  - Build a signal mask to block signals
  - Specify the signal set for `sigwait()` to wait on
  - Set `sa_mask` in `sigaction()`
- Paired with `sigdelset()` for flexible signal set manipulation.
- Use `sigismember()` to check whether a signal is in the set.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigdelset

```c
int sigdelset(sigset_t *set, int signo);
```

Remove the specified signal from a signal set. If the signal is not in the set, the operation has no effect (idempotent).

This is typically paired with `sigfillset()`: fill all signals first, then remove unwanted ones to build an "exclude-type" signal set.

**Parameters**:

- `set` Pointer to the signal set to modify. Must have been initialized with `sigemptyset()` or `sigfillset()`.
- `signo` Signal number to remove (1-63). Must be a valid signal number; invalid values lead to undefined behavior or return an error.

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set:

- `EINVAL` `signo` is not a valid signal number (<= 0 or > `MAX_SIGNO`).

**Note**:

- Must operate on an initialized signal set; otherwise behavior is undefined.
- Removing a signal that is not present produces no error or side effect.
- Multiple calls can be made to remove multiple signals:
  ```c
  sigset_t set;
  sigfillset(&set);              // contains all signals
  sigdelset(&set, SIGINT);       // remove SIGINT
  sigdelset(&set, SIGTERM);      // remove SIGTERM
  // set now contains all signals except SIGINT and SIGTERM
  ```
- Common uses:
  - Exclude certain signals from a full set
  - Modify an existing signal set to drop certain signals
  - Fine-grained control of the signal mask
- The inverse of `sigaddset()`; the two can be combined for flexible signal set manipulation.
- Removing `SIGKILL` or `SIGSTOP` has no practical effect since they cannot be blocked anyway.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigismember

```c
int sigismember(const sigset_t *set, int signo);
```

Check whether the specified signal is in a signal set. This is the standard way to query signal set membership.

**Parameters**:

- `set` Pointer to the signal set to query. Must be initialized.
- `signo` Signal number to check (1-63).

**Returns**:

- Returns 1: The signal is in the set (is a member).
- Returns 0: The signal is not in the set (is not a member).
- Returns -1: Failure; `signo` is invalid. `errno` is set to `EINVAL`.

**Note**:

- The return value is 1 or 0, not non-zero or zero. This differs from some boolean functions; explicitly check for 1.
- Correct usage:
  ```c
  if (sigismember(&set, SIGINT) == 1) {
      // SIGINT is in the set
  } else if (sigismember(&set, SIGINT) == 0) {
      // SIGINT is not in the set
  } else {
      // error
  }
  ```
- Simplified usage (as a boolean):
  ```c
  if (sigismember(&set, SIGINT)) {
      // SIGINT is in the set (returns 1, truthy)
  }
  ```
- Must operate on an initialized signal set; results on an uninitialized signal set are indeterminate.
- Common uses:
  - Check whether a signal is blocked (by querying the mask returned by `sigprocmask()`)
  - Check whether a signal is pending (by querying the set returned by `sigpending()`)
  - Verify the contents of a signal set
- With set operations:
  ```c
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  assert(sigismember(&set, SIGINT) == 1);
  assert(sigismember(&set, SIGTERM) == 0);
  ```

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigisemptyset

```c
int sigisemptyset(sigset_t *set);
```

Check whether a signal set is empty.

**Parameters**:

- `set` Signal set.

**Returns**:

Returns 1 if the signal set is empty, otherwise 0.


**Note**:

- glibc extension (not POSIX-standard); used for a quick check whether a signal set is empty.
**POSIX Compatibility**: Compatible with the glibc extension interface.


### sigandset

```c
int sigandset(sigset_t *dest, const sigset_t *left, const sigset_t *right);
```

Compute the intersection of two signal sets.

**Parameters**:

- `dest` Signal set that stores the result.
- `left` First signal set.
- `right` Second signal set.

**Returns**:

Returns 0 on success, or -1 on failure.


**Note**:

- glibc extension, used to compute the intersection of two signal sets. `dest` may be the same as `left` or `right`.
**POSIX Compatibility**: Compatible with the glibc extension interface.


### sigorset

```c
int sigorset(sigset_t *dest, const sigset_t *left, const sigset_t *right);
```

Compute the union of two signal sets.

**Parameters**:

- `dest` Signal set that stores the result.
- `left` First signal set.
- `right` Second signal set.

**Returns**:

Returns 0 on success, or -1 on failure.


**Note**:

- glibc extension, used to compute the union of two signal sets. `dest` may be the same as `left` or `right`.
**POSIX Compatibility**: Compatible with the glibc extension interface.



## Signal Waiting


### sigwait

```c
int sigwait(const sigset_t *set, int *sig);
```

Synchronously wait for any signal in `set` to arrive. This is the key function for handling signals synchronously, allowing them to be treated as ordinary events rather than asynchronous interrupts.

Unlike asynchronous signal handling, `sigwait()` turns signals into synchronous events: a thread actively waits for a signal, and when it arrives the function returns the signal number instead of invoking a handler. This greatly simplifies signal handling and avoids the complexities of asynchronous handling (such as reentrancy and race conditions).

A typical usage is to create a dedicated signal-handling thread that blocks signals of interest and then calls `sigwait()` in a loop to receive and handle them.

**Parameters**:

- `set` Signal set to wait for. These signals should normally be blocked first with `sigprocmask()` or `pthread_sigmask()`; otherwise the signals may be caught by an asynchronous handler instead of being received by `sigwait()`. Must contain at least one valid signal.
- `sig` Returns the received signal number (output parameter). When waiting for multiple signals, it is impossible to predict which arrives first; dispatch on the returned number.

**Returns**:

Returns 0 on success, or an error code on failure (note: the error code is returned directly and `errno` is not set):

- `EINVAL` `set` contains an invalid signal number (<= 0 or > `MAX_SIGNO`).
- `EINTR` Interrupted by a signal not in `set` (in some implementations; openvela generally does not return this).

**Note**:

- **Important**: The signals being waited for must be blocked. Otherwise, signals may be caught by a handler before or during the `sigwait()` call, causing `sigwait()` to miss them. Recommended pattern:
  ```c
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGUSR2);
  
  // block signals first
  pthread_sigmask(SIG_BLOCK, &set, NULL);
  
  // then wait
  int sig;
  while (1) {
      sigwait(&set, &sig);
      switch (sig) {
          case SIGUSR1: /* handle SIGUSR1 */ break;
          case SIGUSR2: /* handle SIGUSR2 */ break;
      }
  }
  ```
- `sigwait()` removes the signal from the pending queue and does not trigger a signal handler. Even if a handler is registered, a signal received via `sigwait()` does not invoke it.
- If multiple threads wait on the same signal, only one thread receives it (the system chooses).
- If a signal is already pending before `sigwait()` is called, the function returns immediately without blocking.
- `sigwait()` is a cancellation point: if the thread is cancelled (`pthread_cancel()`), the function returns immediately.
- Advantages of synchronous waiting over asynchronous handling:
  - No need to consider function reentrancy
  - Ordinary C library functions can be used (malloc, printf, etc.)
  - No need to use `volatile sig_atomic_t` variables
  - Easier to reason about and debug
- Often used to implement the signal-handling-thread pattern: main and worker threads block all signals, and a dedicated signal thread calls `sigwait()` in a loop to process signals.
- Standard signals are not queued; if the same signal is sent multiple times, only one instance may be received by `sigwait()`. Real-time signals support queuing.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigwaitinfo

```c
int sigwaitinfo(const sigset_t *set, siginfo_t *info);
```

Wait for any signal in `set` and obtain detailed information about the signal. Similar to `sigwait()`, but provides more signal information.

**Parameters**:

- `set` Signal set to wait for.
- `info` If non-`NULL`, receives detailed signal information, including:
  - `si_signo` Signal number.
  - `si_code` Signal source code (such as `SI_USER`, `SI_QUEUE`, `SI_TIMER`).
  - `si_pid` PID of the sending process.
  - `si_value` Data passed with the signal (for signals sent via `sigqueue()`).

**Returns**:

Returns the signal number on success, or -1 on failure with `errno` set:

- `EINTR` Interrupted by another signal.
- `EINVAL` `set` contains an invalid signal number.

**Note**:

- Equivalent to `sigtimedwait(set, info, NULL)`, i.e., an infinite wait without timeout.
- Differs from `sigwait()` in that it returns more detailed `siginfo_t` information.


**Note**:

- Equivalent to `sigtimedwait(set, info, NULL)`, i.e., an infinite wait without timeout.
- Differs from `sigwait()` in that it returns more detailed `siginfo_t` information.
- On failure, `errno` may be `EINTR` (interrupted) or `EINVAL` (invalid signal set).
**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigtimedwait

```c
int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout);
```

Wait for any signal in `set` with a timeout. If no signal arrives within the specified time, the function returns an error.

**Parameters**:

- `set` Signal set to wait for.
- `info` If non-`NULL`, receives detailed signal information.
- `timeout` Timeout:
  - `tv_sec` Seconds.
  - `tv_nsec` Nanoseconds.
  - If `NULL`, wait indefinitely (equivalent to `sigwaitinfo()`).
  - If `{0, 0}`, return immediately (polling mode).

**Returns**:

Returns the signal number on success, or -1 on failure with `errno` set:

- `EAGAIN` No signal arrived within the timeout.
- `EINTR` Interrupted by another signal (not in `set`).
- `EINVAL` Invalid `timeout` argument (such as a negative value).

**Note**:

- Commonly used to implement signal waiting logic with a timeout.
- Combined with real-time signals, it can implement a reliable event notification mechanism.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigsuspend

```c
int sigsuspend(const sigset_t *mask);
```

Temporarily replace the signal mask and suspend the task until an unblocked signal arrives. This is an atomic operation that avoids the race condition between separate calls to `sigprocmask()` and `pause()`.

**Parameters**:

- `mask` Temporary signal mask. During the wait, the process's signal mask is replaced with this value.

**Returns**:

Always returns -1, with `errno` set to `EINTR` (interrupted by a signal).

**Note**:

- After the function returns, the signal mask is automatically restored to the value before the call.
- Often used to wait for a specific signal while keeping it unblocked.
- Example: unblock `SIGUSR1` and wait for it:
  ```c
  sigset_t mask;
  sigemptyset(&mask);  // unblock only SIGUSR1
  sigsuspend(&mask);   // wait for any signal
  ```

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigpause

```c
int sigpause(int signo);
```

Remove the specified signal from the signal mask and suspend the task until the signal arrives.

**Parameters**:

- `signo` Signal number to unblock.

**Returns**:

Always returns -1, with `errno` set to `EINTR`.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name (obsolete).


## Signal Mask


### sigprocmask

```c
int sigprocmask(int how, const sigset_t *set, sigset_t *oset);
```

Set or query the signal mask of the calling thread. The signal mask determines which signals are currently blocked. Blocked signals are not delivered to the process but remain pending until removed from the mask (unblocked).

The signal mask is thread-local; each thread maintains its own signal mask. A newly created thread inherits the creator's signal mask. Blocking signals can protect critical sections from being interrupted and is an important tool for writing robust signal-handling code.

**Parameters**:

- `how` Specifies how to modify the signal mask; must be one of:
  - `SIG_BLOCK` (1): Add signals in `set` to the current mask. New mask = old mask ∪ `set`. Used to block more signals.
  - `SIG_UNBLOCK` (2): Remove signals in `set` from the current mask. New mask = old mask - `set`. Used to unblock signals.
  - `SIG_SETMASK` (3): Replace the current mask entirely with `set`. New mask = `set`. Used to set a precise signal mask.
- `set` Signal set to operate on. If `NULL`, the current mask is not modified; only queried through `oset` (in this case `how` is ignored). The signal set should be initialized and configured with `sigemptyset()`, `sigfillset()`, `sigaddset()`, etc.
- `oset` If non-`NULL`, returns the signal mask before the operation (old value). Can be used to save the current mask for later restoration. To only query without modifying, pass `set=NULL`.

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set:

- `EINVAL` The `how` value is invalid (not `SIG_BLOCK`, `SIG_UNBLOCK`, or `SIG_SETMASK`).
- `EFAULT` `set` or `oset` points to an invalid memory address.

**Note**:

- `SIGKILL` (9) and `SIGSTOP` (19) cannot be blocked; attempts to block them are silently ignored (no error is returned). This guarantees that a process can always be terminated or stopped.
- In a multithreaded program, each thread has its own signal mask; `sigprocmask()` only affects the calling thread. For multithreaded programs, POSIX recommends using `pthread_sigmask()` (which has the same functionality but returns an error code instead of setting `errno`).
- After a signal is unblocked, if that signal is pending, it is delivered immediately (before `sigprocmask()` returns).
- While a signal is blocked, sending the same signal multiple times queues only one instance (standard signals do not queue). Real-time signals (`SIGRTMIN`-`SIGRTMAX`) support queuing.
- Typical usage pattern - critical section protection:
  ```c
  sigset_t new_mask, old_mask;
  sigemptyset(&new_mask);
  sigaddset(&new_mask, SIGINT);
  sigaddset(&new_mask, SIGTERM);
  
  // enter critical section, block signals
  sigprocmask(SIG_BLOCK, &new_mask, &old_mask);
  
  // critical section code, not interrupted by SIGINT/SIGTERM
  // ...
  
  // leave critical section, restore signal mask
  sigprocmask(SIG_SETMASK, &old_mask, NULL);
  ```
- The signal mask is inherited by child processes after `fork()`, but reset to empty after `exec()` (all signals unblocked).
- The signal mask does not affect the signal handling action; only signal delivery. Even if a signal is blocked, its handling action can still be modified with `sigaction()`.
- Use `sigpending()` to query which signals are currently blocked and pending.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sigpending

```c
int sigpending(sigset_t *set);
```

Get the set of signals that are currently blocked and pending (waiting to be delivered). These signals have been sent to the process but have not been delivered because they are blocked.

**Parameters**:

- `set` Pointer used to return the pending signal set.

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set.

**Note**:

- Can be used to check whether any signals are waiting before unblocking them.
- Combine with `sigismember()` to check whether a specific signal is pending.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


### sighold

```c
int sighold(int signo);
```

Add the specified signal to the signal mask (block the signal).

**Parameters**:

- `signo` Signal number to block.

**Returns**:

Returns 0 on success, or -1 on failure.


**Note**:

- Deprecated interface, equivalent to `sigprocmask(SIG_BLOCK, ...)`. Use `sigprocmask()` instead.
- On failure, `errno` is set to `EINVAL` (invalid signal number).
**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name (obsolete).


### sigrelse

```c
int sigrelse(int signo);
```

Remove the specified signal from the signal mask (unblock).

**Parameters**:

- `signo` Signal number to unblock.

**Returns**:

Returns 0 on success, or -1 on failure.


**Note**:

- Deprecated interface, equivalent to `sigprocmask(SIG_UNBLOCK, ...)`. Use `sigprocmask()` instead.
- On failure, `errno` is set to `EINVAL` (invalid signal number).
**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name (obsolete).


### pthread_sigmask

```c
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oset);
```

Set or query the signal mask of the calling thread. Functionally identical to `sigprocmask()`, but this is the recommended interface for POSIX multithreaded programs.

The main difference is how errors are reported: `pthread_sigmask()` returns an error code directly (does not set `errno`), whereas `sigprocmask()` returns -1 and sets `errno`. This follows the general convention for pthread functions.

**Parameters**:

- `how` Specifies how to modify the signal mask:
  - `SIG_BLOCK` (1): Block more signals. New mask = old mask ∪ `set`.
  - `SIG_UNBLOCK` (2): Unblock signals. New mask = old mask - `set`.
  - `SIG_SETMASK` (3): Replace the mask. New mask = `set`.
- `set` Signal set to operate on. If `NULL`, the mask is not modified; only queried (returned through `oset`).
- `oset` If non-`NULL`, returns the signal mask before the operation.

**Returns**:

Returns 0 on success, or an error code on failure (does not set `errno`):

- `EINVAL` Invalid `how` argument.
- `EFAULT` `set` or `oset` points to invalid memory (in some implementations).

**Note**:

- **Multithreading-dedicated**: In multithreaded programs, use `pthread_sigmask()` instead of `sigprocmask()`. The functionality is the same, but `pthread_sigmask()` has clearer semantics.
- **Thread-local mask**: Each thread has its own signal mask; modifications only affect the calling thread.
- **New thread inheritance**: Threads created via `pthread_create()` inherit the creator's signal mask.
- `SIGKILL` and `SIGSTOP` cannot be blocked; attempts to block them are silently ignored.
- After a signal is unblocked, pending signals are delivered immediately (before the function returns).
- Typical usage - dedicated signal-handling thread pattern:
  ```c
  // main thread: block all signals
  sigset_t mask;
  sigfillset(&mask);
  pthread_sigmask(SIG_SETMASK, &mask, NULL);
  
  // create worker threads (inherit the mask that blocks all signals)
  pthread_create(&worker, NULL, worker_func, NULL);
  
  // create signal-handling thread
  pthread_create(&sig_thread, NULL, signal_handler_thread, NULL);
  
  // signal handling thread: unblock and synchronously wait for signals
  void *signal_handler_thread(void *arg) {
      sigset_t wait_mask;
      sigemptyset(&wait_mask);
      sigaddset(&wait_mask, SIGINT);
      sigaddset(&wait_mask, SIGTERM);
      
      int sig;
      while (1) {
          sigwait(&wait_mask, &sig);
          // handle signal...
      }
  }
  ```
- **Error handling difference**:
  ```c
  // sigprocmask() style
  if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
      perror("sigprocmask");  // errno already set
  }
  
  // pthread_sigmask() style
  int err = pthread_sigmask(SIG_BLOCK, &set, NULL);
  if (err != 0) {
      errno = err;
      perror("pthread_sigmask");  // must set errno manually
  }
  ```
- Combined with `pthread_kill()`, precise inter-thread signal communication can be implemented.
- In single-threaded programs, `pthread_sigmask()` and `sigprocmask()` are fully equivalent.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Signal Stack


### sigaltstack

```c
int sigaltstack(const stack_t *ss, stack_t *oss);
```

Set or get the alternate stack for signal handling.

**Parameters**:

- `ss` If non-`NULL`, points to the new alternate stack configuration:
  - `ss_sp` Pointer to the stack memory.
  - `ss_size` Stack size.
  - `ss_flags` Flags (`SS_DISABLE` disables the alternate stack).
- `oss` If non-`NULL`, returns the previous alternate stack configuration.

**Returns**:

Returns 0 on success, or -1 on failure with `errno` set.


**Note**:

- openvela does not currently support `SS_ONSTACK`; setting a stack with flags other than `SS_DISABLE` returns `EINVAL`.
- `EINVAL` is also returned when `ss->ss_size` is less than `MINSIGSTKSZ` (returns `ENOMEM`).
**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Thread Signals


### pthread_kill

```c
int pthread_kill(pthread_t thread, int signo);
```

Send a signal to a specified thread. This is the standard way to send a signal to a specific thread in a multithreaded program. It is more precise than `kill()` and allows specifying which thread receives the signal.

In a multithreaded program, a signal can be delivered to any thread in the process that has not blocked the signal. Using `pthread_kill()` explicitly specifies the receiving thread, ensuring the signal is handled by the expected thread.

**Parameters**:

- `thread` Thread ID of the target thread (returned from `pthread_create()` or obtained via `pthread_self()`).
- `signo` Signal number to send (1-63). The special value 0 is the "null signal" and does not send an actual signal; it only checks whether the thread exists (liveness test).

**Returns**:

Returns 0 on success, or an error code on failure (note: the error code is returned directly and `errno` is not set):

- `EINVAL` Invalid `signo` (<= 0 or > `MAX_SIGNO`).
- `ESRCH` The thread does not exist or has terminated. The thread ID may have been recycled and refer to a new thread, causing the signal to be delivered to the wrong thread.

**Note**:

- **Thread specificity**: The signal is delivered to the specified thread; even if the thread blocks the signal, the signal remains pending on that thread (not on another thread).
- **Liveness test**: Use `pthread_kill(thread, 0)` to test whether a thread exists:
  ```c
  if (pthread_kill(thread, 0) == 0) {
      // thread exists
  } else {
      // thread does not exist (ESRCH)
  }
  ```
- **Signal handling**: Even when a signal is sent to a specific thread, the handler may still execute in another thread (depending on signal masks). If only the target thread has the signal unblocked, the handler will run in that thread.
- **Thread ID reuse**: After a thread terminates, its ID may be reused. Sending a signal after termination may send it to a new thread. Use `tgkill()` to avoid this (it verifies that the thread belongs to the expected process).
- **Difference from `kill()`**:
  - `kill()` sends to the entire process; any thread may handle the signal.
  - `pthread_kill()` sends to a specific thread, which is more precise.
- **Relationship to `raise()`**: In a multithreaded program, `raise(sig)` is equivalent to `pthread_kill(pthread_self(), sig)`.
- **Signal mask inheritance**: A new thread inherits the creator's signal mask. If a different mask is needed, call `pthread_sigmask()` when the thread starts.
- **Typical uses**:
  - Cancel or terminate a specific thread (sending `SIGTERM`, `SIGUSR1`, etc.)
  - Send notification signals to worker threads
  - Send signals to signal-handling threads
  - Test thread liveness
- **Thread safety**: `pthread_kill()` itself is thread-safe and can be called concurrently from multiple threads.
- **POSIX consistency**: In openvela, `pthread_t` is the same as `pid_t`; the thread ID is the task ID.

**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.


## Debugging and Diagnostics


### psignal

```c
void psignal(int signo, const char *message);
```

Print the description of a signal.

**Parameters**:

- `signo` Signal number.
- `message` Prefix message.

**Returns**:

No return value.


**Note**:

- The output format is `message: signal_description`, mainly used for debugging and error logging.
**POSIX Compatibility**: Compatible with the BSD extension interface.


### psiginfo

```c
void psiginfo(const siginfo_t *info, const char *message);
```

Print the description of a signal information structure.

**Parameters**:

- `info` Signal information structure.
- `message` Prefix message.

**Returns**:

No return value.


**Note**:

- Provides more detailed information than `psignal()`, including the signal source code.
**POSIX Compatibility**: Compatible with the `POSIX` interface of the same name.

