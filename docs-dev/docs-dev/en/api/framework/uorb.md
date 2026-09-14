\[ English | [简体中文](../../../zh-cn/api/framework/uorb.md) \]

# uORB API

uORB is the publish/subscribe message bus of openvela, used for asynchronous data communication between processes or threads.

Header: `#include <uORB/uORB.h>`

## openvela Implementation Notes

- **Configuration Dependencies**: Requires `CONFIG_USENSOR` and `CONFIG_UORB` to be enabled
- **Cross-core Communication**: Supports cross-core topic transport when `CONFIG_SENSORS_RPMSG` is enabled
- **Built-in Sensors**: Provides 34 predefined sensor topics (accelerometer, gyroscope, GPS, etc.)
- **Custom Topics**: Defined via `ORB_DECLARE`, `ORB_DEFINE`, and `ORB_ID` macros
- **Debug Tools**: The `uorb listener` tool can monitor topic data (requires `CONFIG_DEBUG_UORB`)

## Device Management

### orb_open

```c
int orb_open(const char *name, int instance, int flags);
```

Opens the topic device node with the specified name and instance.


### orb_close

```c
int orb_close(int fd);
```

Closes a file descriptor.


### orb_unlink_multi

```c
int orb_unlink_multi(const struct orb_metadata *meta, int instance);
```

Removes a topic node.


## Publish Interface


### orb_advertise_multi_queue_info

```c
int orb_advertise_multi_queue_info(const struct orb_metadata *meta, const void *data, int *instance, unsigned int queue_size, orb_info_t *info);
```

Performs the initial advertisement (publisher registration) of a topic, creating the corresponding topic node under `/dev/uorb` and publishing initial data.


### orb_advertise_multi_queue_persist

```c
int orb_advertise_multi_queue_persist_info(const struct orb_metadata *meta, const void *data, int *instance, unsigned int queue_size, orb_info_t *info);
```

`orb_advertise_multi_queue_persist` is similar to `orb_advertise_multi_queue`, but guarantees that all subscribers (including future ones) can access the current and subsequent published data.


### orb_unadvertise

```c
static inline int orb_unadvertise(int fd) { return orb_close(fd);
```

Cancels a topic advertisement.


### orb_publish_multi

```c
ssize_t orb_publish_multi(int fd, const void *data, size_t len);
```

Publishes new data of a specified length to a topic. After publication, all waiting subscribers are woken up; non-waiting subscribers can use `orb_check` to check whether the topic has been updated.

### orb_advertise

```c
static inline int orb_advertise(const struct orb_metadata *meta, const void *data);
```

Advertises a topic. Equivalent to `orb_advertise_multi(meta, data, NULL)`, creating default instance 0.

**Parameters**:

- `meta` Pointer to topic metadata.
- `data` Initial data to publish (may be `NULL`).

**Returns**:

Returns the advertiser file descriptor on success, or a negative value on failure with `errno` set.

### orb_advertise_multi

```c
static inline int orb_advertise_multi(const struct orb_metadata *meta,
                                      const void *data, int *instance);
```

Advertises a topic with an instance ID, used for multi-instance topic scenarios.

**Parameters**:

- `meta` Pointer to topic metadata.
- `data` Initial data to publish.
- `instance` Input/output parameter that returns the newly created instance ID.

**Returns**:

Returns the file descriptor on success, or a negative value on failure.

### orb_advertise_queue

```c
static inline int orb_advertise_queue(const struct orb_metadata *meta,
                                      const void *data, unsigned int queue_size);
```

Advertises a topic with a queue. When the queue size is greater than 1, multiple historical data entries are cached.

**Parameters**:

- `meta` Pointer to topic metadata.
- `data` Initial data.
- `queue_size` Queue depth.

**Returns**:

Returns the file descriptor on success, or a negative value on failure.

### orb_advertise_multi_queue

```c
static inline int orb_advertise_multi_queue(const struct orb_metadata *meta,
                                            const void *data, int *instance,
                                            unsigned int queue_size);
```

Advertises a topic with both an instance ID and a queue depth.

**Parameters**:

- `meta` Pointer to topic metadata.
- `data` Initial data.
- `instance` Input/output parameter that returns the instance ID.
- `queue_size` Queue depth.

**Returns**:

Returns the file descriptor on success, or a negative value on failure.

### orb_advertise_multi_queue_persist_info

```c
int orb_advertise_multi_queue_persist_info(const struct orb_metadata *meta,
                                           const void *data, int *instance,
                                           unsigned int queue_size,
                                           orb_info_t *info);
```

Advertises a topic with persistent info fields. The `info` parameter is used to pass additional topic metadata.

**Parameters**:

- `meta` Pointer to topic metadata.
- `data` Initial data.
- `instance` Input/output parameter that returns the instance ID.
- `queue_size` Queue depth.
- `info` Pointer to additional topic information.

**Returns**:

Returns the file descriptor on success, or a negative value on failure.

### orb_publish

```c
static inline int orb_publish(const struct orb_metadata *meta, int fd, const void *data);
```

Publishes topic data (default length obtained from meta).

**Parameters**:

- `meta` Pointer to topic metadata.
- `fd` Advertiser file descriptor.
- `data` Data to publish.

**Returns**:

Returns the number of bytes published on success, or a negative value on failure.

### orb_publish_auto

```c
static inline int orb_publish_auto(const struct orb_metadata *meta, int *fd,
                                   const void *data, int *instance);
```

Automatically advertises and publishes. If `*fd` is less than 0, an advertiser is created automatically. Convenient for simple publish scenarios.

**Parameters**:

- `meta` Pointer to topic metadata.
- `fd` Input/output parameter, advertiser file descriptor.
- `data` Data to publish.
- `instance` Input/output parameter that returns the instance ID.

**Returns**:

Returns `0` on success, or a negative value on failure.

## Subscribe Interface


### orb_subscribe_multi

```c
int orb_subscribe_multi(const struct orb_metadata *meta, unsigned instance);
```

Subscribes to a topic in non-wakeup mode. After data is published, waiting subscribers are woken up; non-waiting subscribers can use `orb_check` to check for updates.

If the subscription occurs after a publish, calling `orb_check` immediately after a successful subscription will return `true`. Even if the topic has not been advertised yet, the subscription will succeed — in this case the topic timestamp is 0, no poll events are triggered, `orb_check` always returns `false`, and data cannot be copied until the topic is subsequently advertised.


### orb_unsubscribe

```c
static inline int orb_unsubscribe(int fd) { return orb_close(fd);
```

Unsubscribes from a topic.


### orb_copy_multi

```c
ssize_t orb_copy_multi(int fd, void *buffer, size_t len);
```

Retrieves data of a specified length from a topic. This is the **only** interface that resets the "subscriber received new data" flag — once `poll` or `orb_check` indicates an update, this interface must be called to consume the data.


### orb_check

```c
int orb_check(int fd, bool *updated);
```

Checks whether a topic has had new data published since the last `orb_copy`. Used in scenarios that do not use `poll()` to determine whether `orb_copy` needs to be called; also avoids the overhead of calling `poll()` when the topic has very likely been updated. The update state is tracked per fd: after returning `true`, `orb_copy` must be called on the same fd to reset the update flag, otherwise this interface will continue to return `true`.

### orb_subscribe

```c
static inline int orb_subscribe(const struct orb_metadata *meta);
```

Subscribes to the default instance of a topic. Equivalent to `orb_subscribe_multi(meta, 0)`.

**Parameters**:

- `meta` Pointer to topic metadata.

**Returns**:

Returns the subscriber file descriptor on success, or a negative value on failure.

### orb_subscribe_wakeup

```c
static inline int orb_subscribe_wakeup(const struct orb_metadata *meta);
```

Subscribes to the default instance with wakeup mode enabled (asynchronously wakes the caller when new data arrives). Equivalent to `orb_subscribe_multi_wakeup(meta, 0)`.

**Parameters**:

- `meta` Pointer to topic metadata.

**Returns**:

Returns the file descriptor on success, or a negative value on failure.

### orb_subscribe_multi_wakeup

```c
int orb_subscribe_multi_wakeup(const struct orb_metadata *meta, unsigned instance);
```

Subscribes to a topic with a specified instance ID and enables wakeup mode.

**Parameters**:

- `meta` Pointer to topic metadata.
- `instance` Instance ID.

**Returns**:

Returns the file descriptor on success, or a negative value on failure.

### orb_copy

```c
static inline int orb_copy(const struct orb_metadata *meta, int fd, void *buffer);
```

Copies the latest topic data from the subscriber fd to the buffer, using the default length (obtained from meta).

**Parameters**:

- `meta` Pointer to topic metadata.
- `fd` Subscriber file descriptor.
- `buffer` Buffer to receive the data.

**Returns**:

Returns the number of bytes copied on success, or a negative value on failure.

### orb_unlink

```c
static inline int orb_unlink(const struct orb_metadata *meta);
```

Removes the default instance of a topic. Equivalent to `orb_unlink_multi(meta, 0)`.

**Parameters**:

- `meta` Pointer to topic metadata.

**Returns**:

Returns `0` on success, or a negative value on failure.

## Control Interface


### orb_get_state

```c
int orb_get_state(int fd, struct orb_state *state);
```

Retrieves the state information of all subscribers for a topic. The state includes the maximum frequency and minimum batch interval among all subscribers, as well as the `enable` field (indicating whether the current node is subscribed or active). If there are no subscribers, the state fields are set to: `max_frequency=0`, `min_batch_interval=0`, `enable=false`.


### orb_get_events

```c
int orb_get_events(int fd, unsigned int *events);
```

Retrieves event information for the specified subscriber.


### orb_ioctl

```c
int orb_ioctl(int fd, int cmd, unsigned long arg);
```

Performs ioctl control on a subscriber, identical to ioctl().


### orb_flush

```c
int orb_flush(int fd);
```

Flushes accumulated topic data from the hardware buffer. When the hardware FIFO has not reached the watermark but an immediate read is desired, calling this interface forces the FIFO data to be output; there is no restriction on when to call it. After calling, you can monitor the `POLLPRI` event on the fd and use `orb_get_events` to determine whether the flush is complete.


### orb_set_batch_interval

```c
int orb_set_batch_interval(int fd, unsigned batch_interval);
```

Sets the desired batch interval for the user. The actual effective value depends on the hardware FIFO capability. This interface triggers a `POLLPRI` event to notify the publisher, which then decides the final batch interval. Only applicable to topics with hardware FIFO (e.g., sensors with hardware FIFO); otherwise the call has no effect.


### orb_get_batch_interval

```c
int orb_get_batch_interval(int fd, unsigned *batch_interval);
```

Retrieves the currently effective batch interval in batch mode. Only applicable to topics with hardware FIFO (e.g., sensors with hardware FIFO); otherwise the call has no effect. See `orb_set_batch_interval`.

### orb_set_interval

```c
int orb_set_interval(int fd, unsigned interval);
```

Sets the minimum reporting interval for a subscriber (in microseconds).

**Parameters**:

- `fd` Subscriber file descriptor.
- `interval` Reporting interval in microseconds; `0` means no limit.

**Returns**:

Returns `0` on success, or a negative value on failure.

### orb_get_interval

```c
int orb_get_interval(int fd, unsigned *interval);
```

Retrieves the current reporting interval for a subscriber (in microseconds).

**Parameters**:

- `fd` Subscriber file descriptor.
- `interval` Output parameter that returns the current interval value.

**Returns**:

Returns `0` on success, or a negative value on failure.

### orb_set_frequency

```c
static inline int orb_set_frequency(int fd, unsigned frequency);
```

Sets the reporting interval for a subscriber by frequency (Hz). Equivalent to `orb_set_interval(fd, frequency ? 1000000/frequency : 0)`.

**Parameters**:

- `fd` Subscriber file descriptor.
- `frequency` Target frequency (Hz); `0` means no limit.

**Returns**:

Returns `0` on success, or a negative value on failure.

### orb_get_frequency

```c
static inline int orb_get_frequency(int fd, unsigned *frequency);
```

Retrieves the current reporting rate for a subscriber by frequency (Hz).

**Parameters**:

- `fd` Subscriber file descriptor.
- `frequency` Output parameter that returns the current frequency.

**Returns**:

Returns `0` on success, or a negative value on failure.


### orb_get_info

```c
int orb_get_info(int fd, orb_info_t *info);
```

Retrieves topic information.


### orb_info

```c
void orb_info(const char *format, const char *name, const void *data);
```

Prints sensor data.


## Query Interface


### orb_elapsed_time

```c
orb_abstime orb_absolute_time(void);
```

Retrieves the current system time (in microseconds).


### orb_exists

```c
int orb_exists(const struct orb_metadata *meta, int instance);
```

Checks whether a topic instance has been advertised.


### orb_group_count

```c
int orb_group_count(const struct orb_metadata *meta);
```

Retrieves the number of advertised topic instances.

### orb_absolute_time

```c
orb_abstime orb_absolute_time(void);
```

Retrieves a monotonically increasing absolute timestamp (in microseconds), used for timestamping topic data.

**Returns**:

Returns the current timestamp.


### orb_get_meta

```c
const struct orb_metadata *orb_get_meta(const char *name);
```

Retrieves topic metadata by name string.


## Formatted I/O


### orb_sscanf

```c
int orb_sscanf(const char *buf, const char *format, void *data);
```

Converts a string value into a struct buffer.


### orb_fprintf

```c
int orb_fprintf(FILE *stream, const char *format, const void *data);
```

Prints sensor data to a file.


## Event Loop


### orb_loop_init

```c
int orb_loop_init(struct orb_loop_s *loop, enum orb_loop_type_e type);
```

Initializes an orb event loop. Use orb_loop_deinit to release resources.


### orb_loop_run

```c
int orb_loop_run(struct orb_loop_s *loop);
```

Starts the event loop. After the loop starts, users can dynamically add new file descriptors via `orb_handle_start` or close existing ones via `orb_handle_stop`. The loop enters a blocking state after starting.


### orb_loop_deinit

```c
int orb_loop_deinit(struct orb_loop_s *loop);
```

Deinitializes the current event loop. After deinitialization, the loop must be reinitialized before reuse. Handles added to the loop via `orb_handle_init` are the user's responsibility to close.


### orb_loop_exit_async

```c
int orb_loop_exit_async(struct orb_loop_s *loop);
```

Sends an exit event to the current event loop (non-blocking).


### orb_handle_init

```c
int orb_handle_init(struct orb_handle_s *handle, int fd, int events, void *arg, orb_datain_cb_t datain_cb, orb_dataout_cb_t dataout_cb, orb_eventpri_cb_t pri_cb, orb_eventerr_cb_t err_cb);
```

Initializes an orb handle.


### orb_handle_start

```c
int orb_handle_start(struct orb_loop_s *loop, struct orb_handle_s *handle);
```

Starts a handle in the event loop.


### orb_handle_stop

```c
int orb_handle_stop(struct orb_loop_s *loop, struct orb_handle_s *handle);
```

Stops a handle in the event loop.
