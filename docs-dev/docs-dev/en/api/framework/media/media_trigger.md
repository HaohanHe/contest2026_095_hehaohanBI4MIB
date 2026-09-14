\[ English | [简体中文](../../../../zh-cn/api/framework/media/media_trigger.md) \]

# Media Trigger API

Media Trigger is used for Voice Trigger scenarios, enabling keyword detection and recognition through loading acoustic models.

Header: `#include <media_trigger.h>`

## openvela Implementation Notes

- **Typical Scenarios**: Voice wake-up on smart speakers, smartwatches, etc.
- **Workflow**:
    1. `open` to open a trigger handle
    2. `set_event_callback` to register an event callback
    3. `load_sound_model` to load an acoustic model
    4. `start_recognition` to start recognition
    5. Listen for callbacks and process when a keyword is detected
    6. `stop_recognition` → `unload_sound_model` → `close` for cleanup
- **Parameter Configuration**: The `params` passed to `open` selects the microphone configuration (e.g. `"default"` / `"Dual Mic"`)
- **Underlying Implementation**: Interfaces with the DSP-side acoustic model processor

## Trigger Lifecycle

### media_trigger_open

```c
void* media_trigger_open(const char* params);
```

Opens a media trigger handle.

**Parameters**:

- `params` Trigger parameter string, e.g. `"default"` or `"Dual Mic"`, used to select the microphone configuration.

**Returns**:

Returns the trigger handle on success, or `NULL` on failure.

**Example**:

```c
// 1. Create instance
void* handle = media_trigger_open("default");

// 2. Set event callback
ret = media_trigger_set_event_callback(handle, cookie, callback);

// 3. Load acoustic model
ret = media_trigger_load_sound_model(handle, model, model_size);

// 4. Start recognition
ret = media_trigger_start_recognition(handle);

// 5. Stop recognition
ret = media_trigger_stop_recognition(handle);

// 6. Unload model
ret = media_trigger_unload_sound_model(handle);

// 7. Close handle
ret = media_trigger_close(handle);
```

### media_trigger_close

```c
int media_trigger_close(void* handle);
```

Closes the trigger handle and releases associated resources.

**Parameters**:

- `handle` The trigger handle to close.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Events and Callbacks

### media_trigger_set_event_callback

```c
int media_trigger_set_event_callback(void* handle, void* event_cookie,
                                     media_event_callback on_event);
```

Sets an event callback for the trigger to receive events such as recognition state changes.

**Parameters**:

- `handle` Trigger handle.
- `event_cookie` User data passed to the callback.
- `on_event` Event callback function.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Acoustic Model Management

### media_trigger_load_sound_model

```c
int media_trigger_load_sound_model(void* handle, void* model, size_t model_size);
```

Loads acoustic model data for the trigger.

**Parameters**:

- `handle` Trigger handle.
- `model` Pointer to acoustic model data.
- `model_size` Size of the model data in bytes.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_trigger_unload_sound_model

```c
int media_trigger_unload_sound_model(void* handle);
```

Unloads the currently loaded acoustic model.

**Parameters**:

- `handle` Trigger handle.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## Recognition Control

### media_trigger_start_recognition

```c
int media_trigger_start_recognition(void* handle);
```

Starts voice recognition. The trigger continuously detects input audio and matches keywords in the loaded model.

**Parameters**:

- `handle` Trigger handle.

**Returns**:

Returns `0` on success, or a negative errno on failure.

### media_trigger_stop_recognition

```c
int media_trigger_stop_recognition(void* handle);
```

Stops voice recognition.

**Parameters**:

- `handle` Trigger handle.

**Returns**:

Returns `0` on success, or a negative errno on failure.

## DSP Property Query

### media_trigger_get_property

```c
int media_trigger_get_property(char* properties, int len);
```

Queries property information of the underlying DSP for the trigger.

**Parameters**:

- `properties` Output buffer for receiving the property string.
- `len` Buffer length.

**Returns**:

Returns `0` on success, or a negative errno on failure.
