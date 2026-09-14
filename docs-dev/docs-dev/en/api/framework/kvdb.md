\[ English | [简体中文](../../../zh-cn/api/framework/kvdb.md) \]

# KVDB API

KVDB provides lightweight key-value persistent storage, built on the UnQLite embedded database, with an API design inspired by the Android properties specification.

Headers: `#include <kvdb.h>`, `#include <cutils/properties.h>`

## openvela Implementation Notes

- **Storage Backend**: Built on the UnQLite embedded database
- **Value Types**: Supports string, boolean, int32, int64, and binary data
- **Sync/Async Writes**: `property_set()` performs synchronous writes; `property_set_oneway()` performs asynchronous writes (does not wait for persistence to complete)
- **Monitoring**: Supports key-value change monitoring via `property_wait()` or `property_monitor_*` interfaces
- **CLI Tools**: Provides `setprop`/`getprop` command-line tools for debugging

## Read Interfaces

### property_get

```c
int property_get(const char *key, char *value, const char *default_value);
```

Retrieves a string property value. Returns the default value if the key does not exist.

**Parameters**:

- `key` Property key name.
- `value` Buffer to store the property value.
- `default_value` Default value when the key does not exist; may be `NULL`.

**Returns**:

Returns the length of the property value.

### property_get_bool

```c
int8_t property_get_bool(const char *key, int8_t default_value);
```

Retrieves a boolean property value.

**Parameters**:

- `key` Property key name.
- `default_value` Default value when the key does not exist.

**Returns**:

Returns the boolean value of the property.

### property_get_int32

```c
int32_t property_get_int32(const char *key, int32_t default_value);
```

Retrieves a 32-bit integer property value.

**Parameters**:

- `key` Property key name.
- `default_value` Default value when the key does not exist.

**Returns**:

Returns the int32 value of the property.

### property_get_int64

```c
int64_t property_get_int64(const char *key, int64_t default_value);
```

Retrieves a 64-bit integer property value.

**Parameters**:

- `key` Property key name.
- `default_value` Default value when the key does not exist.

**Returns**:

Returns the int64 value of the property.

### property_get_buffer

```c
ssize_t property_get_buffer(const char *key, void *value, size_t size);
```

Retrieves a binary buffer property value.

**Parameters**:

- `key` Property key name.
- `value` Buffer to store the data.
- `size` Buffer size.

**Returns**:

Returns the number of bytes read on success, or a negative error code on failure.

### property_get_binary

```c
ssize_t property_get_binary(const char *key, void *value, size_t val_len);
```

Retrieves a binary property value.

**Parameters**:

- `key` Property key name.
- `value` Buffer to store the data.
- `val_len` Buffer size.

**Returns**:

Returns the number of bytes read on success, or a negative error code on failure.

### property_get_with_err

```c
int property_get_with_err(const char *key, char *value);
```

Retrieves a string property value, distinguishing between "key does not exist" and "value is empty" via the return value.

**Parameters**:

- `key` Property key name.
- `value` Buffer to store the property value.

**Returns**:

Returns the property value length on success, or a negative error code when the key does not exist.

### property_get_bool_with_err

```c
int property_get_bool_with_err(const char *key, int8_t *value);
```

Reads a boolean value with error detection.

**Parameters**:

- `key` Property key name.
- `value` Pointer to store the boolean result.

**Returns**:

Returns `0` on success, or a negative error code when the key does not exist or the type does not match.

### property_get_int32_with_err

```c
int property_get_int32_with_err(const char *key, int32_t *value);
```

Reads a 32-bit signed integer with error detection.

**Parameters**:

- `key` Property key name.
- `value` Pointer to store the int32 result.

**Returns**:

Returns `0` on success, or a negative error code when the key does not exist or the type does not match.

### property_get_int64_with_err

```c
int property_get_int64_with_err(const char *key, int64_t *value);
```

Reads a 64-bit signed integer with error detection.

**Parameters**:

- `key` Property key name.
- `value` Pointer to store the int64 result.

**Returns**:

Returns `0` on success, or a negative error code when the key does not exist or the type does not match.

## Write Interfaces

### property_set

```c
int property_set(const char *key, const char *value);
```

Sets a string property value (synchronous write; waits for persistence to complete).

**Parameters**:

- `key` Property key name.
- `value` Property value.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### property_set_oneway

```c
int property_set_oneway(const char *key, const char *value);
```

Sets a string property value (asynchronous write; does not wait for persistence to complete). Offers better performance but does not guarantee immediate persistence.

**Parameters**:

- `key` Property key name.
- `value` Property value.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### property_set_bool

```c
int property_set_bool(const char *key, int8_t value);
```

Sets a boolean property value (synchronous write).

**Parameters**:

- `key` Property key name.
- `value` Boolean value (`0` or non-`0`).

**Returns**:

Returns `0` on success, or a negative error code on failure.

### property_set_int32

```c
int property_set_int32(const char *key, int32_t value);
```

Sets a 32-bit signed integer property value (synchronous write).

**Parameters**:

- `key` Property key name.
- `value` int32 value.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### property_set_int64

```c
int property_set_int64(const char *key, int64_t value);
```

Sets a 64-bit signed integer property value (synchronous write).

**Parameters**:

- `key` Property key name.
- `value` int64 value.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### property_set_bool_oneway

```c
int property_set_bool_oneway(const char *key, int8_t value);
```

Asynchronously sets a boolean property value (does not wait for persistence to complete).

**Parameters**:

- `key` Property key name.
- `value` Boolean value.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### property_set_int32_oneway

```c
int property_set_int32_oneway(const char *key, int32_t value);
```

Asynchronously sets an int32 property value (does not wait for persistence to complete).

**Parameters**:

- `key` Property key name.
- `value` int32 value.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### property_set_int64_oneway

```c
int property_set_int64_oneway(const char *key, int64_t value);
```

Asynchronously sets an int64 property value (does not wait for persistence to complete).

**Parameters**:

- `key` Property key name.
- `value` int64 value.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### property_set_buffer

```c
int property_set_buffer(const char *key, const void *value, size_t size);
```

Sets a binary buffer property value (synchronous write).

**Parameters**:

- `key` Property key name.
- `value` Pointer to the data buffer.
- `size` Buffer size in bytes.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### property_set_buffer_oneway

```c
int property_set_buffer_oneway(const char *key, const void *value, size_t size);
```

Asynchronously sets a binary buffer property value (does not wait for persistence to complete).

**Parameters**:

- `key` Property key name.
- `value` Pointer to the data buffer.
- `size` Buffer size in bytes.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### property_set_binary

```c
int property_set_binary(const char *key, const void *value, size_t val_len, bool oneway);
```

Sets a binary property value.

**Parameters**:

- `key` Property key name.
- `value` Binary data.
- `val_len` Data length.
- `oneway` Whether to write asynchronously.

### property_delete

```c
int property_delete(const char *key);
```

Deletes the property with the specified key.

**Parameters**:

- `key` Property key name to delete.

**Returns**:

Returns 0 on success, or a negative error code on failure.

## Management Interfaces

### property_commit

```c
int property_commit(void);
```

Forces all pending property writes to be persisted to storage.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### property_load

```c
int property_load(const char *path);
```

Loads properties from a file into the database.

**Parameters**:

- `path` Path to the property file.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### property_exit

```c
int property_exit(void);
```

Closes KVDB and releases resources.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### property_list

```c
int property_list(void (*propfn)(const char *key, const char *value, void *cookie), void *cookie);
```

Iterates over all properties, invoking the callback function for each one.

**Parameters**:

- `propfn` Callback function that receives the key, value, and user data.
- `cookie` User data passed to the callback function.

### property_list_binary

```c
int property_list_binary(void (*propfn)(const char *key, const void *value, size_t val_len, void *cookie), void *cookie);
```

Iterates over all binary properties, invoking the callback function for each one. Unlike `property_list`, the callback includes a `val_len` parameter, making it suitable for non-string values.

**Parameters**:

- `propfn` Callback function with signature `void (*)(const char *key, const void *value, size_t val_len, void *cookie)`, receiving the key, binary value, value length, and user data.
- `cookie` User data passed to the callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Monitoring Interfaces

### property_wait

```c
ssize_t property_wait(const char *key, char *newkey, void *newvalue, size_t val_len, int timeout);
```

Waits for a property change notification. Blocks until the specified key (or any key) changes or the timeout expires.

**Parameters**:

- `key` Key name to monitor; `NULL` to monitor all keys.
- `newkey` Buffer to store the changed key name.
- `newvalue` Buffer to store the new value.
- `val_len` Value buffer size.
- `timeout` Timeout in milliseconds; -1 for infinite wait.

**Returns**:

Returns the value length on success, 0 on timeout, or a negative error code on failure.

### property_monitor_open

```c
int property_monitor_open(const char *key);
```

Opens a property monitoring file descriptor, which can be used with `poll()`.

**Parameters**:

- `key` Key name to monitor; `NULL` to monitor all keys.

**Returns**:

Returns a file descriptor on success, or a negative error code on failure.

### property_monitor_read

```c
ssize_t property_monitor_read(int fd, char *newkey, void *newvalue, size_t val_len);
```

Reads a change event from the monitoring file descriptor.

**Parameters**:

- `fd` File descriptor returned by `property_monitor_open()`.
- `newkey` Buffer to store the changed key name.
- `newvalue` Buffer to store the new value.
- `val_len` Value buffer size.

**Returns**:

Returns the value length on success, or a negative error code on failure.

### property_monitor_close

```c
int property_monitor_close(int fd);
```

Closes a property monitoring file descriptor.

**Parameters**:

- `fd` File descriptor to close.

**Returns**:

Returns 0 on success, or a negative error code on failure.
