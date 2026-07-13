\[ English | [简体中文](../../../../zh-cn/api/framework/media/media_trigger_model.md) \]

# Sound Model API

The Sound Model interface handles low-level acoustic model data required by the media trigger, providing model loading, unloading, property/option queries, and hotword detection capabilities.

Header: `#include <media_trigger_model.h>`

## openvela Implementation Notes

- **Relationship with Media Trigger**: `media_trigger_*` is the high-level voice wakeup interface (including DSP path), while `media_trigger_model_*` is the low-level model operation interface (no audio capture involved)
- **Typical Usage**: Custom recognition flows, running one-shot keyword detection on a specific PCM buffer
- **Model Properties/Options**: Use `get_properties` to query vendor properties, and `get_options` to read recommended audio sampling parameters
- **One-shot Detection**: `detect_hotword` performs a single synchronous detection on a given PCM buffer

## Model Lifecycle

### media_trigger_model_load

```c
void* media_trigger_model_load(const void* model, size_t size);
```

Loads acoustic model data and returns a model context.

**Parameters**:

- `model` Pointer to the start of model data.
- `size` Size of the model data in bytes.

**Returns**:

Returns the model context pointer on success, or `NULL` on failure.

### media_trigger_model_unload

```c
void media_trigger_model_unload(void* context);
```

Unloads a previously loaded model context.

**Parameters**:

- `context` The model context to unload.

## Model Property Queries

### media_trigger_model_get_properties

```c
void media_trigger_model_get_properties(void* properties, size_t* size);
```

Queries vendor-provided model property information.

**Parameters**:

- `properties` Output buffer for storing property data.
- `size` Input/output parameter: on entry specifies the buffer size, on return updated to the actual number of bytes written.

### media_trigger_model_get_options

```c
void media_trigger_model_get_options(void* context, char* options, size_t size);
```

Queries the model's recommended audio capture options string, e.g. `"format=s16le:sample_rate=16000:ch_layout=mono"`.

**Parameters**:

- `context` Model context (returned by `media_trigger_model_load`).
- `options` Output buffer for receiving the options string.
- `size` Output buffer size in bytes.

### media_trigger_model_get_buffer_size

```c
void media_trigger_model_get_buffer_size(void* context, size_t* size);
```

Queries the model's recommended recording buffer size.

**Parameters**:

- `context` Model context.
- `size` Output parameter, returns the recommended buffer size in bytes.

## Hotword Detection

### media_trigger_model_detect_hotword

```c
bool media_trigger_model_detect_hotword(void* context, const char* buffer, size_t size);
```

Runs a single model detection pass on the given PCM buffer to determine whether a hotword is matched.

**Parameters**:

- `context` Model context.
- `buffer` PCM audio buffer to analyze.
- `size` Buffer size in bytes.

**Returns**:

Returns `true` if a hotword is detected, `false` otherwise.
