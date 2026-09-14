\[ English | [简体中文](../../../zh-cn/api/kernel/msgqueue.md) \]

# Message Queue API

openvela provides POSIX-compliant message queue interfaces for asynchronous message passing between tasks. Message queues support priority ordering, where higher-priority messages are received first.

Header file: `#include <mqueue.h>`

## openvela Implementation Notes

- **Sending from interrupts**: `mq_send()` can be called from interrupt context, but behaves differently: it does not check queue size, uses pre-allocated messages (quantity configured by `PREALLOC_MQ_IRQ_MSGS`)
- **Notification behavior difference**: `mq_notify()` sends the notification signal even when a task is waiting to receive a message, which is not fully consistent with the POSIX specification
- **Timeout values**: `mq_timedsend()` and `mq_timedreceive()` use Epoch-based absolute time

## Queue Management

### mq_open

```c
mqd_t mq_open(const char *mqName, int oflags, ...);
```

Establishes a connection between the calling task and a message queue. After a successful call, the returned message queue descriptor can be used for subsequent operations until `mq_close()` is called.

**Parameters**:

- `mqName` Name of the message queue.
- `oflags` Open flags, which can be any combination of:
  - `O_RDONLY` Read-only.
  - `O_WRONLY` Write-only.
  - `O_RDWR` Read-write.
  - `O_CREAT` Create the message queue if it does not exist.
  - `O_EXCL` Used with `O_CREAT`, fails if the queue already exists.
  - `O_NONBLOCK` Non-blocking mode.
- `...` Optional arguments required when `O_CREAT` is used:
  - `mode` (`mode_t`) File permission bits. Not used in the current implementation, but required by POSIX.
  - `attr` (`struct mq_attr *`) Queue attributes. If `NULL`, default values are used. `mq_maxmsg` sets the maximum number of messages, `mq_msgsize` sets the maximum message size.

**Returns**:

Returns a message queue descriptor (`mqd_t`) on success, or -1 (`ERROR`) on failure and sets `errno`:

- `ENOENT` `O_CREAT` was not set and the specified queue does not exist.
- `EEXIST` Both `O_CREAT` and `O_EXCL` were set, but the queue already exists.
- `EINVAL` Invalid argument (e.g., `mqName` is `NULL`).
- `ENOMEM` Insufficient memory to create the queue.
- `ENFILE` System message queue limit reached.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

### mq_close

```c
int mq_close(mqd_t mqdes);
```

Closes a message queue descriptor, releasing system resources allocated to the task. If the task has registered a notification request on this queue, the notification is cancelled.

**Parameters**:

- `mqdes` Message queue descriptor.

**Returns**:

Returns 0 on success, or -1 (`ERROR`) on failure and sets `errno`:

- `EBADF` `mqdes` is not a valid message queue descriptor.

**Notes**:

- Calling `mq_close()` while `mq_send()` or `mq_receive()` is blocked is undefined behavior.
- Using the same `mqdes` after closing is undefined behavior.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

### mq_unlink

```c
int mq_unlink(const char *mqName);
```

Removes the named message queue. If any tasks still have the queue open, the removal is deferred until all descriptors are closed.

**Parameters**:

- `mqName` Name of the message queue.

**Returns**:

Returns 0 on success, or -1 (`ERROR`) on failure and sets `errno`:

- `ENOENT` The specified queue does not exist.
- `EINVAL` `mqName` is `NULL`.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

## Sending Messages

### mq_send

```c
int mq_send(mqd_t mqdes, const void *msg, size_t msglen, int prio);
```

Sends a message to a message queue. Messages are ordered by priority, with higher-priority messages placed before lower-priority ones. `prio` must not exceed `MQ_PRIO_MAX`.

If the queue is full and `O_NONBLOCK` is not set, the call blocks until space becomes available. If `O_NONBLOCK` is set, it returns an error immediately.

**Parameters**:

- `mqdes` Message queue descriptor.
- `msg` Message to send.
- `msglen` Length of the message in bytes, must not exceed `mq_msgsize`.
- `prio` Message priority.

**Returns**:

Returns 0 on success, or -1 (`ERROR`) on failure and sets `errno`:

- `EAGAIN` Queue is full and `O_NONBLOCK` is set.
- `EINVAL` `msg` or `mqdes` is `NULL`, or `prio` is invalid.
- `EPERM` Message queue was not opened for writing.
- `EMSGSIZE` `msglen` exceeds the queue's `mq_msgsize`.
- `EINTR` Interrupted by a signal.

**Notes**:

- `mq_send()` can be called from interrupt context, but behaves differently: it does not check queue size (always sends), uses pre-allocated messages (quantity configured by `PREALLOC_MQ_IRQ_MSGS`), and does not allocate new memory.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

### mq_timedsend

```c
int mq_timedsend(mqd_t mqdes, const void *msg, size_t msglen, int prio,
                 const struct timespec *abstime);
```

Sends a message with a timeout. Behaves the same as `mq_send()`, but when the queue is full, blocks at most until the absolute time specified by `abstime`.

**Parameters**:

- `mqdes` Message queue descriptor.
- `msg` Message to send.
- `msglen` Length of the message in bytes.
- `prio` Message priority.
- `abstime` Absolute timeout (Epoch-based).

**Returns**:

Returns 0 on success, or -1 (`ERROR`) on failure and sets `errno`:

- `EAGAIN` Queue is full and `O_NONBLOCK` is set.
- `EINVAL` `msg` or `mqdes` is `NULL`, or `prio` is invalid.
- `EPERM` Message queue was not opened for writing.
- `EMSGSIZE` `msglen` exceeds the queue's `mq_msgsize`.
- `EINTR` Interrupted by a signal.
- `ETIMEDOUT` Timed out.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

## Receiving Messages

### mq_receive

```c
ssize_t mq_receive(mqd_t mqdes, void *msg, size_t msglen, int *prio);
```

Receives the oldest message with the highest priority from the message queue. `msglen` must not be less than the queue's `mq_msgsize`, otherwise an error is returned.

If the queue is empty and `O_NONBLOCK` is not set, the call blocks until a message is available. Among multiple waiting tasks, the one with the highest priority and longest wait time receives first.

**Parameters**:

- `mqdes` Message queue descriptor.
- `msg` Buffer to receive the message.
- `msglen` Buffer size in bytes.
- `prio` If not `NULL`, stores the priority of the received message.

**Returns**:

Returns the message length in bytes on success, or -1 (`ERROR`) on failure and sets `errno`:

- `EAGAIN` Queue is empty and `O_NONBLOCK` is set.
- `EPERM` Message queue was not opened for reading.
- `EMSGSIZE` `msglen` is less than the queue's `mq_msgsize`.
- `EINTR` Interrupted by a signal.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

### mq_timedreceive

```c
ssize_t mq_timedreceive(mqd_t mqdes, char *msg, size_t msglen,
                        unsigned int *prio, const struct timespec *abstime);
```

Receives a message with a timeout. Behaves the same as `mq_receive()`, but when the queue is empty, blocks at most until the absolute time specified by `abstime`.

**Parameters**:

- `mqdes` Message queue descriptor.
- `msg` Buffer to receive the message.
- `msglen` Buffer size in bytes.
- `prio` If not `NULL`, stores the priority of the received message.
- `abstime` Absolute timeout (Epoch-based).

**Returns**:

Returns the message length in bytes on success, or -1 (`ERROR`) on failure and sets `errno`:

- `EAGAIN` Queue is empty and `O_NONBLOCK` is set.
- `EPERM` Message queue was not opened for reading.
- `EMSGSIZE` `msglen` is less than the queue's `mq_msgsize`.
- `EINTR` Interrupted by a signal.
- `ETIMEDOUT` Timed out.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

## Queue Attributes

### mq_setattr

```c
int mq_setattr(mqd_t mqdes, const struct mq_attr *mqStat, struct mq_attr *oldMqStat);
```

Sets message queue attributes. Only the `O_NONBLOCK` bit in `mq_flags` can be modified.

**Parameters**:

- `mqdes` Message queue descriptor.
- `mqStat` New attributes.
- `oldMqStat` If not `NULL`, stores the attributes before modification.

**Returns**:

Returns 0 on success, or -1 (`ERROR`) on failure and sets `errno`:

- `EBADF` `mqdes` is not a valid descriptor.
- `EINVAL` `mqStat` is `NULL`.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

### mq_getattr

```c
int mq_getattr(mqd_t mqdes, struct mq_attr *mqStat);
```

Gets the current attributes of a message queue.

**Parameters**:

- `mqdes` Message queue descriptor.
- `mqStat` Returns the attribute structure:
  - `mq_maxmsg` Maximum number of messages in the queue.
  - `mq_msgsize` Maximum message size in bytes.
  - `mq_flags` Message queue flags.
  - `mq_curmsgs` Current number of messages in the queue.

**Returns**:

Returns 0 on success, or -1 (`ERROR`) on failure and sets `errno`:

- `EBADF` `mqdes` is not a valid descriptor.
- `EINVAL` `mqStat` is `NULL`.

**POSIX Compatibility**: Compatible with the POSIX interface of the same name.

## Notification

### mq_notify

```c
int mq_notify(mqd_t mqdes, const struct sigevent *notification);
```

Registers or cancels message arrival notification. When a message arrives at a previously empty queue, a signal notification is sent to the registered task.

If `notification` is `NULL`, the current registration is cancelled. The registration is automatically cancelled after the notification is sent and must be re-registered.

**Parameters**:

- `mqdes` Message queue descriptor.
- `notification` Notification configuration, including:
  - `sigev_notify` Notification method (should be `SIGEV_SIGNAL`).
  - `sigev_signo` Signal number for notification.
  - `sigev_value` Value passed with the signal.

**Returns**:

Returns 0 on success, or -1 (`ERROR`) on failure and sets `errno`:

- `EBADF` `mqdes` is not a valid descriptor.
- `EBUSY` Another task has already registered notification for this queue.
- `EINVAL` `sigev_notify` is not a valid value, or the signal number is invalid.
- `ENOMEM` Insufficient memory.

**Notes**:

- **Behavior difference from POSIX**: In openvela, the notification signal is still sent to the registered task even when a task is blocked on `mq_receive()` waiting for a message. The POSIX specification requires that no notification be sent in this case (the message directly satisfies the waiting `mq_receive()`).

**POSIX Compatibility**: Partially compatible with the POSIX interface of the same name (notification timing differs from the POSIX specification).
