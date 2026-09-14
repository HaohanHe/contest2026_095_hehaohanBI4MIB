\[ English | [简体中文](../../../../zh-cn/api/framework/media/media_focus.md) \]

# Audio Focus Management API

Audio Focus is used to coordinate playback priority among multiple audio applications. When multiple applications request audio focus simultaneously, the system determines which should play, stop, or lower volume based on the scenario, and notifies each application via callbacks.

Header: `#include <media_focus.h>`

## openvela Implementation Notes

- **Scenario priority**: The request type is identified by a `scenario` string (e.g. `MEDIA_SCENARIO_MUSIC`, `MEDIA_SCENARIO_NOTIFICATION`). The framework returns a focus suggestion based on scenario priority.
- **Synchronous / Asynchronous dual model**: Two sets of interfaces are provided:
    - Synchronous: `media_focus_*` series, suitable for simple scenarios.
    - Asynchronous (libuv-based): `media_uv_focus_*` series, requires `CONFIG_LIBUV`, suitable for applications based on uv_loop.
- **Auto-reply**: The `request2` interface adds an `auto_reply` parameter. When set to `1`, the framework automatically replies to received suggestions without the application manually calling `reply`.
- **Callback disconnection protection**: Even if `initial_suggestion` returns `MEDIA_FOCUS_STOP` (meaning the focus is immediately preempted by another holder), the framework still returns a valid handle. You must call `abandon` to release it; otherwise a resource leak will occur.
- **Legacy vs. new interfaces**: `media_focus_request` / `media_uv_focus_request` are marked deprecated. Use the versions with the `2` suffix instead.

## Focus Request (Synchronous)

### media_focus_request

```c
void* media_focus_request(int* initial_suggestion, const char* scenario,
                          media_focus_callback on_suggestion, void* cookie)
    __attribute__((deprecated));
```

Request audio focus (**deprecated**, use `media_focus_request2` instead).

**Parameters**:

- `initial_suggestion` Output parameter that returns the initial focus suggestion, valued as a `MEDIA_FOCUS_*` constant.
- `scenario` Scenario identifier string, valued as a `MEDIA_SCENARIO_*` constant.
- `on_suggestion` Callback function invoked when the focus suggestion changes.
- `cookie` User data passed to the callback.

**Returns**:

Returns a focus handle on success, or `NULL` on failure.

**Notes**:

- If `initial_suggestion` is `MEDIA_FOCUS_STOP`, `on_suggestion` will not be called, but a valid handle is still returned. The caller must call `media_focus_abandon` to release it; otherwise a leak will occur.

### media_focus_request2

```c
void* media_focus_request2(int* initial_suggestion, const char* scenario,
                           media_focus_callback2 on_suggestion,
                           int auto_reply, void* cookie);
```

Request audio focus (recommended, replaces `media_focus_request`).

**Parameters**:

- `initial_suggestion` Output parameter that returns the initial focus suggestion, valued as a `MEDIA_FOCUS_*` constant.
- `scenario` Scenario identifier string, valued as a `MEDIA_SCENARIO_*` constant.
- `on_suggestion` Callback function invoked when the focus suggestion changes (new signature includes `req_id`).
- `auto_reply` Whether to enable auto-reply: `1` means the framework automatically replies to focus suggestions, `0` means the application manually calls `media_focus_reply`.
- `cookie` User data passed to the callback.

**Returns**:

Returns a focus handle on success, or `NULL` on failure.

**Notes**:

- If `initial_suggestion` is `MEDIA_FOCUS_STOP`, `on_suggestion` will not be called, but a valid handle is still returned. The caller must call `media_focus_abandon` to release it.

**Example**:

```c
int initial_suggestion;

context->handle = media_focus_request2(&initial_suggestion,
    MEDIA_SCENARIO_MUSIC, demo_focus_callback, 1, context);
if (!context->handle) {
    // handle error
}

if (initial_suggestion == MEDIA_FOCUS_STOP)
    media_focus_abandon(context->handle);
```

### media_focus_abandon

```c
int media_focus_abandon(void* handle);
```

Release audio focus.

**Parameters**:

- `handle` The focus handle to release.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### media_focus_reply

```c
int media_focus_reply(void* handle, int req_id);
```

Reply to a focus request. Used when `request2` has `auto_reply` set to `0`, to manually acknowledge the focus suggestion.

**Parameters**:

- `handle` The focus handle.
- `req_id` Request ID, passed in by the focus suggestion callback.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Focus Request (Asynchronous, libuv-based)

The following interfaces are only available when `CONFIG_LIBUV` is enabled.

### media_uv_focus_request

```c
void* media_uv_focus_request(void* loop, const char* scenario,
                             media_focus_callback on_suggestion, void* cookie)
    __attribute__((deprecated));
```

Asynchronously request audio focus (**deprecated**, use `media_uv_focus_request2` instead).

**Parameters**:

- `loop` The `uv_loop_t*` event loop of the current thread.
- `scenario` Scenario identifier string, valued as a `MEDIA_SCENARIO_*` constant.
- `on_suggestion` Callback function invoked when the focus suggestion changes.
- `cookie` User data passed to the callback.

**Returns**:

Returns an asynchronous focus handle on success, or `NULL` on failure.

**Notes**:

- When the `on_suggestion` callback receives `MEDIA_FOCUS_STOP`, the application should call `media_uv_focus_abandon` to release the handle.

### media_uv_focus_request2

```c
void* media_uv_focus_request2(void* loop, const char* scenario,
                              media_focus_callback2 on_suggestion,
                              int auto_reply, void* cookie);
```

Asynchronously request audio focus (recommended, replaces `media_uv_focus_request`).

**Parameters**:

- `loop` The `uv_loop_t*` event loop of the current thread.
- `scenario` Scenario identifier string, valued as a `MEDIA_SCENARIO_*` constant.
- `on_suggestion` Callback function invoked when the focus suggestion changes (new signature includes `req_id`).
- `auto_reply` Whether to enable auto-reply: `1` means the framework automatically replies to focus suggestions.
- `cookie` User data passed to the callback.

**Returns**:

Returns an asynchronous focus handle on success, or `NULL` on failure.

**Example**:

```c
void user_on_suggestion(int suggestion, int req_id, void* cookie) {
    UserContext* ctx = cookie;
    switch (suggestion) {
    case MEDIA_FOCUS_STOP:
        media_uv_focus_abandon(ctx->handle, NULL);
        break;
    }
}

ctx->handle = media_uv_focus_request2(loop, MEDIA_SCENARIO_MUSIC,
    user_on_suggestion, 1, ctx);
```

### media_uv_focus_abandon

```c
int media_uv_focus_abandon(void* handle, media_uv_callback on_abandon);
```

Asynchronously release audio focus.

**Parameters**:

- `handle` The asynchronous focus handle.
- `on_abandon` Callback invoked after the release is complete, typically used to free the cookie.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_uv_focus_reply

```c
int media_uv_focus_reply(void* handle, int req_id);
```

Asynchronously reply to a focus request. Used when `request2` has `auto_reply` set to `0`.

**Parameters**:

- `handle` The asynchronous focus handle.
- `req_id` Request ID.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Debug Interface

### media_focus_dump

```c
void media_focus_dump(const char* options);
```

Dump focus stack information for debugging.

**Parameters**:

- `options` Dump options (currently unused).

**Notes**:

- This interface will be merged into the unified `media_dump()` in the future.
