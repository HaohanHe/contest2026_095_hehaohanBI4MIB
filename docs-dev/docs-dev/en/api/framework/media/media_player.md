\[ English | [简体中文](../../../../zh-cn/api/framework/media/media_player.md) \]

# Multimedia Player API

Audio and video playback supporting local files and network streaming.

Header: `#include <media_player.h>`

## openvela Implementation Notes

- **Synchronous/Asynchronous dual model**: Two parallel sets of interfaces are provided
    - Synchronous: `media_player_*` series, calls return in the current thread
    - Asynchronous: `media_uv_player_*` series, based on libuv event loop, requires `CONFIG_LIBUV`
- **Lifecycle**: `open` creates the player → `prepare` sets the source → `start` begins playback → `stop`/`close` releases resources
- **Data sources**: Two input methods are supported
    - Local/network URL: Specify directly via `prepare(url)`
    - Byte stream buffer: Push via `write_data`, or obtain the underlying socket via `get_socket`
- **Event callback**: Register an event listener via `set_event_callback` to receive playback state changes, errors, and other notifications
- **Parameter configuration**: Common parameters are read/written via `set_property` / `get_property` (e.g., sample rate, channel count)

## Synchronous Interface - Lifecycle

### media_player_open

```c
void* media_player_open(const char* stream);
```

Opens a player for the specified stream type.

**Parameters**:

- `stream` Stream type constant. Different stream types have different routing logic.

**Returns**:

Returns the player handle on success, or `NULL` on failure.


### media_player_close

```c
int media_player_close(void* handle, int pending_stop);
```

Closes the player.

**Parameters**:

- `handle` Player handle.
- `pending_stop` Whether to wait for stop to complete before closing: 0 means stop immediately and close, 1 means wait for the current track to finish before closing. This parameter is only effective for audio players; setting it to 1 for video players has no waiting effect.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_player_set_event_callback

```c
int media_player_set_event_callback(void* handle, void* event_cookie, media_event_callback on_event);
```

Sets the event callback to monitor stream state changes.

**Parameters**:

- `handle` Player handle.
- `event_cookie` Callback context parameter.
- `on_event` Event callback function for receiving stream state change notifications.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_player_prepare

```c
int media_player_prepare(void* handle, const char* url, const char* options);
```

Prepares the playback resource.

**Parameters**:

- `handle` Player handle.
- `url` Resource path, supporting two modes: 1. URL mode: `url` is a local file path or network address, the framework reads and plays it; 2. BUFFER mode: `url` is `NULL`, the caller must continuously push data via `media_player_write_data()` or `media_player_get_socket()` + `write()`.
- `options` Additional configuration parameters for the resource, typically key-value pairs describing the resource format (e.g., `"format=s16le,sample_rate=44100,channels=2"`).

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_player_reset

```c
int media_player_reset(void* handle);
```

Resets the player.

**Parameters**:

- `handle` Player handle returned by `media_player_open`.

**Returns**:

Returns 0 on success, or a negated error code on failure.


## Synchronous Interface - Data Stream

### media_player_write_data

```c
ssize_t media_player_write_data(void* handle, const void* data, size_t len);
```

Writes data to the player for playback.

**Parameters**:

- `handle` Player handle.
- `data` Buffer address.
- `len` Buffer length to write.

**Returns**:

Returns the number of bytes sent on success, or a negated error code on failure.


### media_player_get_sockaddr

```c
int media_player_get_sockaddr(void* handle, struct sockaddr_storage* addr);
```

Gets the socket address information for buffer mode.

**Parameters**:

- `handle` Player handle.
- `addr` Socket address information.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_player_get_socket

```c
int media_player_get_socket(void* handle);
```

Gets the socket file descriptor for writing.

**Parameters**:

- `handle` Player handle.

**Returns**:

Returns the socket file descriptor on success, or a negated error code on failure.


### media_player_close_socket

```c
void media_player_close_socket(void* handle);
```

Closes the socket file descriptor.

**Parameters**:

- `handle` Player handle.


## Synchronous Interface - Playback Control

### media_player_start

```c
int media_player_start(void* handle);
```

Starts or resumes playback of the audio source.

**Parameters**:

- `handle` Player handle.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_player_stop

```c
int media_player_stop(void* handle);
```

Stops playback and clears the audio source.

**Parameters**:

- `handle` Player handle.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_player_pause

```c
int media_player_pause(void* handle);
```

Pauses playback.

**Parameters**:

- `handle` Player handle.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_player_seek

```c
int media_player_seek(void* handle, unsigned int position);
```

Seeks to the specified position from the beginning.

**Parameters**:

- `handle` Player handle.
- `position` Position in milliseconds.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_player_set_looping

```c
int media_player_set_looping(void* handle, int loop);
```

Sets the loop count.

**Parameters**:

- `handle` Player handle.
- `loop` Loop count; `-1` means infinite looping.

**Returns**:

Returns `0` on success, or a negated errno on failure.


## Synchronous Interface - State Query

### media_player_is_playing

```c
int media_player_is_playing(void* handle);
```

Checks the current playing status.

**Parameters**:

- `handle` Player handle.

**Returns**:

Returns a positive value if playing, zero if inactive, or a negative value on error.


### media_player_get_position

```c
int media_player_get_position(void* handle, unsigned int* position);
```

Gets the current playback position of the audio source.

**Parameters**:

- `handle` Player handle.
- `position` Output position in milliseconds.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_player_get_duration

```c
int media_player_get_duration(void* handle, unsigned int* duration);
```

Gets the total duration of the current audio source.

**Parameters**:

- `handle` Player handle.
- `duration` Output duration in milliseconds.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_player_get_latency

```c
int media_player_get_latency(void* handle, unsigned int* latency);
```

Gets the latency of the current audio source.

**Parameters**:

- `handle` Player handle.
- `latency` Output latency in frames.

**Returns**:

Returns `0` on success, or a negated errno on failure.


## Synchronous Interface - Volume and Properties

### media_player_set_volume

```c
int media_player_set_volume(void* handle, float volume);
```

Sets the volume.

**Parameters**:

- `handle` Player handle.
- `volume` Volume value in the range `[0.0, 1.0]`.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_player_get_volume

```c
int media_player_get_volume(void* handle, float* volume);
```

Gets the volume.

**Parameters**:

- `handle` Player handle.
- `volume` Output volume value in the range `[0.0, 1.0]`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_player_set_property

```c
int media_player_set_property(void* handle, const char* target, const char* key, const char* value);
```

Sets a property on the player.

**Parameters**:

- `handle` Player handle.
- `target` Target filter name.
- `key` Property key.
- `value` Property value.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_player_get_property

```c
int media_player_get_property(void* handle, const char* target, const char* key, char* value, int value_len);
```

Gets a property from the player.

**Parameters**:

- `handle` Player handle.
- `target` Target filter name.
- `key` Property key.
- `value` Output buffer.
- `value_len` Length of the output buffer.

**Returns**:

Returns `0` on success, or a negated errno on failure.


## Asynchronous Interface (libuv-based)

The following interfaces are only available when `CONFIG_LIBUV` is enabled. Callbacks execute on the `uv_loop`, avoiding blocking the calling thread.

### media_uv_player_open

```c
void* media_uv_player_open(void* loop, const char* stream, media_uv_callback on_open, void* cookie);
```

Opens an asynchronous player with the given stream type.

**Parameters**:

- `loop` The `uv_loop_t*` event loop handle for the current thread.
- `stream` Stream type constant. Different stream types have different routing logic.
- `on_open` Callback function triggered when the open operation completes.
- `cookie` Callback context shared by `on_open`, `on_event`, `on_connection`, and `on_close`.

**Returns**:

Returns the player handle on success, or `NULL` on failure.


### media_uv_player_listen

```c
int media_uv_player_listen(void* handle, media_event_callback on_event);
```

Listens to status change events by setting a callback.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_event` Event callback function invoked upon notification.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_close

```c
int media_uv_player_close(void* handle, int pending, media_uv_callback on_close);
```

Closes the asynchronous player.

**Parameters**:

- `handle` Asynchronous player handle.
- `pending` Whether to close in pending mode.
- `on_close` Callback function triggered when resource release completes.

**Returns**:

Returns 0 on success, or a negated error code for an invalid handle.


### media_uv_player_prepare

```c
int media_uv_player_prepare(void* handle, const char* url, const char* options, media_uv_object_callback on_connection, media_uv_callback on_prepare, void* cookie);
```

Prepares the audio source for playback.

**Parameters**:

- `handle` Asynchronous player handle.
- `url` Resource path, supporting two modes: 1. URL mode: `url` is a local file path or network address, the framework reads and plays it; 2. BUFFER mode: `url` is `NULL`, the caller must continuously push data via `media_player_write_data()` or `media_player_get_socket()` + `write()`.
- `options` Additional configuration parameters for the resource, typically key-value pairs describing the resource format (e.g., `"format=s16le,sample_rate=44100,channels=2"`).
- `on_connection` Callback function that receives the `uv_pipe_t` in BUFFER mode.
- `on_prepare` Result callback function.
- `cookie` Callback context for `on_prepare`.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_uv_player_reset

```c
int media_uv_player_reset(void* handle, media_uv_callback on_reset, void* cookie);
```

Resets the player.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_reset` Result callback function.
- `cookie` Callback context for `on_reset`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_start_auto

```c
int media_uv_player_start_auto(void* handle, const char* scenario, media_uv_callback on_start, void* cookie);
```

Plays or resumes the prepared audio source with automatic focus request.

**Parameters**:

- `handle` Asynchronous player handle.
- `scenario` Scenario constant; different scenarios correspond to different focus priorities.
- `on_start` Result confirmation callback (for request/start operations).
- `cookie` Callback context for `on_start`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_start

```c
int media_uv_player_start(void* handle, media_uv_callback on_start, void* cookie);
```

Plays or resumes the prepared resource.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_start` Result callback function.
- `cookie` Callback context for `on_start`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_pause

```c
int media_uv_player_pause(void* handle, media_uv_callback on_pause, void* cookie);
```

Pauses playback.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_pause` Result callback function.
- `cookie` Callback context for `on_pause`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_stop

```c
int media_uv_player_stop(void* handle, media_uv_callback on_stop, void* cookie);
```

Stops playback and clears the prepared audio source.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_stop` Result callback function.
- `cookie` Callback context for `on_stop`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_set_volume

```c
int media_uv_player_set_volume(void* handle, float volume, media_uv_callback on_volume, void* cookie);
```

Sets the player volume.

**Parameters**:

- `handle` Asynchronous player handle.
- `volume` Volume value in the range `[0.0, 1.0]`.
- `on_volume` Result callback function.
- `cookie` Callback context for `on_volume`.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_uv_player_get_volume

```c
int media_uv_player_get_volume(void* handle, media_uv_float_callback on_volume, void* cookie);
```

Gets the player volume.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_volume` Result callback function.
- `cookie` Callback context for `on_volume`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_get_playing

```c
int media_uv_player_get_playing(void* handle, media_uv_int_callback on_playing, void* cookie);
```

Gets the current playing status.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_playing` Result callback function.
- `cookie` Callback context for `on_playing`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_get_position

```c
int media_uv_player_get_position(void* handle, media_uv_unsigned_callback on_position, void* cookie);
```

Gets the current playback position.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_position` Result callback function.
- `cookie` Callback context for `on_position`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_get_duration

```c
int media_uv_player_get_duration(void* handle, media_uv_unsigned_callback on_duration, void* cookie);
```

Gets the duration of the current audio source.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_duration` Result callback function.
- `cookie` Callback context for `on_duration`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_get_latency

```c
int media_uv_player_get_latency(void* handle, media_uv_unsigned_callback cb, void* cookie);
```

Gets the latency of the current audio source.

**Parameters**:

- `handle` Asynchronous player handle.
- `cb` Result callback function.
- `cookie` Callback context for `cb`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_set_looping

```c
int media_uv_player_set_looping(void* handle, int loop, media_uv_callback on_looping, void* cookie);
```

Sets the loop count.

**Parameters**:

- `handle` Asynchronous player handle.
- `loop` Loop count; `-1` means infinite looping.
- `on_looping` Result callback function.
- `cookie` Callback context for `on_looping`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_seek

```c
int media_uv_player_seek(void* handle, unsigned int position, media_uv_callback on_seek, void* cookie);
```

Seeks to the specified position from the beginning.

**Parameters**:

- `handle` Asynchronous player handle.
- `position` Target position in milliseconds.
- `on_seek` Result callback function.
- `cookie` Callback context for `on_seek`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_set_property

```c
int media_uv_player_set_property(void* handle, const char* target, const char* key, const char* value, media_uv_callback on_setprop, void* cookie);
```

Sets a property on the player.

**Parameters**:

- `handle` Asynchronous player handle.
- `target` Target filter name.
- `key` Property key.
- `value` Property value.
- `on_setprop` Result callback function.
- `cookie` Callback context for `on_setprop`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_get_property

```c
int media_uv_player_get_property(void* handle, const char* target, const char* key, media_uv_string_callback on_getprop, void* cookie);
```

Gets a property from the player.

**Parameters**:

- `handle` Asynchronous player handle.
- `target` Target filter name.
- `key` Property key.
- `on_getprop` Result callback function.
- `cookie` Callback context for `on_getprop`.

**Returns**:

Returns `0` on success, or a negated errno on failure.


### media_uv_player_query

```c
int media_uv_player_query(void* handle, media_uv_object_callback on_query, void* cookie);
```

Queries metadata of the player.

**Parameters**:

- `handle` Asynchronous player handle.
- `on_query` Callback function that receives the metadata pointer.
- `cookie` Callback context for `on_query`.

**Returns**:

Returns 0 on success, or a negated error code on failure.


### media_uv_player_close_socket

```c
int media_uv_player_close_socket(void* handle);
```

Closes the socket file descriptor.

**Parameters**:

- `handle` Player handle.

**Returns**:

Returns 0 on success, or a negated error code on failure.
