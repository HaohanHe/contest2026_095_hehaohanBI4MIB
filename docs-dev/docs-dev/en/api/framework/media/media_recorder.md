\[ English | [简体中文](../../../../zh-cn/api/framework/media/media_recorder.md) \]

# Media Recorder API

Audio/video recording functionality, supporting file recording and buffer mode.

Header: `#include <media_recorder.h>`

## openvela Implementation Notes

- **Synchronous/Asynchronous dual model**: `media_recorder_*` (synchronous) and `media_uv_recorder_*` (asynchronous, based on libuv)
- **Output methods**: Two destination types are supported
    - Local file: Specify the path via `prepare(url)`
    - Byte stream buffer: Read via `read_data`, or obtain the underlying socket via `get_socket`
- **Photo capture**: In addition to audio/video recording, provides `take_picture` / `start_picture` / `finish_picture` image capture interfaces
- **Event callback**: Register an event listener via `set_event_callback`

## Synchronous Interface - Lifecycle

### media_recorder_open

```c
void* media_recorder_open(const char* params);
```

Opens a recorder with the specified source type.

**Parameters**:

- `params` Source type constant, typically `MEDIA_SOURCE_MIC`.

**Returns**:

Returns the recorder handle on success; returns `NULL` on failure.

### media_recorder_close

```c
int media_recorder_close(void* handle);
```

Closes the recorder.

**Parameters**:

- `handle` Recorder handle.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_recorder_set_event_callback

```c
int media_recorder_set_event_callback(void* handle, void* cookie, media_event_callback event_cb);
```

Sets the recorder event callback, which is invoked on state changes or other events of interest.

**Parameters**:

- `handle` Recorder handle.
- `cookie` User data passed back when `event_cb` is triggered.
- `event_cb` Event callback function.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_recorder_prepare

```c
int media_recorder_prepare(void* handle, const char* url, const char* options);
```

Prepares the recorder.

**Parameters**:

- `handle` Recorder handle.
- `url` Resource path, supporting two modes: 1. URL mode: `url` is a local file path; the framework opens and records to that path. 2. BUFFER mode: `url` is `NULL`; the caller must continuously receive data via `media_recorder_read_data()` or `media_recorder_get_socket()` + `read()`.
- `options` Additional configuration parameters, including: format (container format, e.g. opus/wav), sample_rate (sample rate), ch_layout (channel layout), b (bitrate, e.g. `"23900"`), vbr (0=constant bitrate, 1=variable bitrate), level (encoding complexity, 0-10, default 10). Example: `"format=opusraw:sample_rate=16000:ch_layout=mono:b=32000:vbr=0:level=1"`.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_recorder_reset

```c
int media_recorder_reset(void* handle);
```

Resets the recorder.

**Parameters**:

- `handle` Recorder handle.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Synchronous Interface - Data Stream

### media_recorder_read_data

```c
ssize_t media_recorder_read_data(void* handle, void* data, size_t len);
```

Reads recorded data from the recorder.

**Parameters**:

- `handle` Recorder handle.
- `data` Buffer address.
- `len` Buffer length.

**Returns**:

Returns the number of bytes read on success, or a negative errno on failure.

### media_recorder_get_sockaddr

```c
int media_recorder_get_sockaddr(void* handle, struct sockaddr_storage* addr);
```

Gets the socket address information for buffer mode.

**Parameters**:

- `handle` Recorder handle.
- `addr` Socket address information output.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_recorder_get_socket

```c
int media_recorder_get_socket(void* handle);
```

Gets the socket file descriptor for reading.

**Parameters**:

- `handle` Recorder handle.

**Returns**:

Returns the socket file descriptor on success, or a negative errno on failure.

### media_recorder_close_socket

```c
void media_recorder_close_socket(void* handle);
```

Closes the socket file descriptor when the recorder finishes receiving data.

**Parameters**:

- `handle` Recorder handle.

## Synchronous Interface - Recording Control

### media_recorder_start

```c
int media_recorder_start(void* handle);
```

Starts or resumes recording from the audio source.

**Parameters**:

- `handle` Recorder handle.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_recorder_pause

```c
int media_recorder_pause(void* handle);
```

Pauses the recorder after capture has started.

**Parameters**:

- `handle` Recorder handle.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_recorder_stop

```c
int media_recorder_stop(void* handle);
```

Stops recording.

**Parameters**:

- `handle` Recorder handle.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Synchronous Interface - Properties

### media_recorder_set_property

```c
int media_recorder_set_property(void* handle, const char* target, const char* key, const char* value);
```

Sets a property on the recorder path.

**Parameters**:

- `handle` Recorder handle.
- `target` Target filter name.
- `key` Property key to set.
- `value` Property value to set.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_recorder_get_property

```c
int media_recorder_get_property(void* handle, const char* target, const char* key, char* value, int value_len);
```

Gets a property from the recorder path.

**Parameters**:

- `handle` Recorder handle.
- `target` Target filter name.
- `key` Property key to query.
- `value` Output buffer.
- `value_len` Length of the output buffer.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Synchronous Interface - Image Capture

### media_recorder_take_picture

```c
int media_recorder_take_picture(char* params, char* filename, size_t number);
```

Takes a picture from the camera.

**Parameters**:

- `params` Camera open path parameters.
- `filename` Storage path for the new image.
- `number` Number of pictures to take.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_recorder_start_picture

```c
void* media_recorder_start_picture(char* params, char* filename, size_t number, media_event_callback event_cb, void* cookie);
```

Starts taking a picture, including open, set_event_callback, prepare, and start operations.

**Parameters**:

- `params` Open parameters.
- `filename` Storage path for the new image.
- `number` Number of pictures to take.
- `event_cb` Callback function for handling status feedback.
- `cookie` User private data.

**Returns**:

Returns a valid handle on success, or `NULL` on failure.

### media_recorder_finish_picture

```c
int media_recorder_finish_picture(void* handle);
```

Closes the recorder when picture taking is finished.

**Parameters**:

- `handle` Recorder handle returned by `media_recorder_start_picture()`.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Asynchronous Interface (libuv-based)

The following interfaces are only available when `CONFIG_LIBUV` is enabled.

### media_uv_recorder_open

```c
void* media_uv_recorder_open(void* loop, const char* source, media_uv_callback on_open, void* cookie);
```

Opens an asynchronous recorder.

**Parameters**:

- `loop` The `uv_loop_t*` event loop handle of the current thread.
- `source` Source type.
- `on_open` Callback function triggered after open completes.
- `cookie` Callback context shared by `on_open`, `on_event`, and `on_close`.

**Returns**:

Returns the recorder handle on success.

### media_uv_recorder_listen

```c
int media_uv_recorder_listen(void* handle, media_event_callback on_event);
```

Listens for status change events by setting a callback.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `on_event` Event callback function, invoked upon notification.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_close

```c
int media_uv_recorder_close(void* handle, media_uv_callback on_close);
```

Closes the asynchronous recorder.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `on_close` Callback function triggered after resource release completes.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_prepare

```c
int media_uv_recorder_prepare(void* handle, const char* url, const char* options, media_uv_object_callback on_connection, media_uv_callback on_prepare, void* cookie);
```

Prepares the destination file.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `url` Destination path.
- `options` Destination configuration parameters; see `media_recorder_prepare` for details.
- `on_connection` Connection callback; in BUFFER mode, carries a writable `uv_pipe_t`.
- `on_prepare` Result callback function.
- `cookie` One-time callback context.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_start_auto

```c
int media_uv_recorder_start_auto(void* handle, const char* stream, media_uv_callback on_start, void* cookie);
```

Starts or resumes capturing with an automatic focus request.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `stream` Scenario constant defined in `media_defs.h`.
- `on_start` Result callback function.
- `cookie` One-time callback context.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_start

```c
int media_uv_recorder_start(void* handle, media_uv_callback on_start, void* cookie);
```

Starts or resumes capturing.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `on_start` Result callback function.
- `cookie` One-time callback context.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_pause

```c
int media_uv_recorder_pause(void* handle, media_uv_callback on_pause, void* cookie);
```

Pauses capturing.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `on_pause` Result callback function.
- `cookie` One-time callback context.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_stop

```c
int media_uv_recorder_stop(void* handle, media_uv_callback on_stop, void* cookie);
```

Stops capturing and finishes the destination file.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `on_stop` Result callback function.
- `cookie` One-time callback context.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_set_property

```c
int media_uv_recorder_set_property(void* handle, const char* target, const char* key, const char* value, media_uv_callback cb, void* cookie);
```

Sets a property on the recorder.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `target` Target filter name.
- `key` Property key.
- `value` Property value.
- `cb` Result callback function.
- `cookie` One-time callback context.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_get_property

```c
int media_uv_recorder_get_property(void* handle, const char* target, const char* key, media_uv_string_callback cb, void* cookie);
```

Gets a property from the recorder.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `target` Target filter name.
- `key` Property key.
- `cb` Result callback function.
- `cookie` One-time callback context.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_reset

```c
int media_uv_recorder_reset(void* handle, media_uv_callback on_reset, void* cookie);
```

Resets the recorder, clearing the current recording to start a new one.

**Parameters**:

- `handle` Asynchronous recorder handle.
- `on_reset` Result callback function.
- `cookie` One-time callback context.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_recorder_take_picture

```c
int media_uv_recorder_take_picture(void* loop, char* params, char* filename, size_t number, media_uv_callback on_complete, void* cookie);
```

Takes a picture from the camera.

**Parameters**:

- `loop` The `uv_loop_t*` event loop handle of the current thread.
- `params` Camera open path parameters.
- `filename` Storage path for the new image.
- `number` Number of pictures to take.
- `on_complete` Result callback function.
- `cookie` User private data.

**Returns**:

Returns `0` on success, or a negative errno on failure.
