\[ English | [简体中文](../../../../zh-cn/api/framework/feature/feature_framework_trace.md) \]

# Feature Trace API

Macro definitions for performance tracing (trace) instrumentation in the Feature framework. When enabled, these macros call the NuttX `sched_note` interface to record events; when disabled, they expand to no-ops.

Header file: `#include <feature_trace.h>`

## openvela Implementation Notes

- **Conditional compilation**: All trace macros are controlled by the `CONFIG_FEATURE_USE_SCHED_NOTE` configuration option.
    - When enabled, they expand to `sched_note_*` series calls using the `NOTE_TAG_ALWAYS` tag.
    - When disabled, they expand to no-ops (zero CPU/memory overhead), suitable for production builds.
- **Dependencies**: Depends on the NuttX kernel's `sched_note` mechanism; `CONFIG_SCHED_INSTRUMENTATION` related configurations must also be enabled.
- **Usage scenarios**: Instrument at Feature interface implementations or JS-Native boundaries. Combined with openvela trace analysis tools (such as SystemView, Perfetto), performance bottlenecks can be visualized.
- **Paired usage**: `FEATURE_NOTE_BEGIN*` / `FEATURE_NOTE_END*` must be called in pairs; otherwise trace event pairing will fail.

## Basic Instrumentation Macros

### FEATURE_NOTE_PRINTF

```c
FEATURE_NOTE_PRINTF(format, ...)
```

Instruments with a formatted string, similar to `printf`. Used to record custom debug information.

**Parameters**:

- `format` Format string.
- `...` Variable argument list corresponding to format placeholders.

### FEATURE_NOTE_BEGIN

```c
FEATURE_NOTE_BEGIN()
```

Marks the beginning of a code execution segment (no additional information). Must be paired with `FEATURE_NOTE_END`.

### FEATURE_NOTE_END

```c
FEATURE_NOTE_END()
```

Marks the end of a code execution segment. Pairs with the most recent `FEATURE_NOTE_BEGIN`.

## Tagged Instrumentation Macros

### FEATURE_NOTE_BEGIN_STR

```c
FEATURE_NOTE_BEGIN_STR(str)
```

Begin instrumentation with a string tag, used to identify the semantics of a code segment.

**Parameters**:

- `str` Event tag string. This string must remain valid for the entire duration of the trace event.

### FEATURE_NOTE_END_STR

```c
FEATURE_NOTE_END_STR(str)
```

End instrumentation with a string tag, matching the corresponding `FEATURE_NOTE_BEGIN_STR` tag.

**Parameters**:

- `str` Event tag string (must match the tag of the begin instrumentation).

### FEATURE_NOTE_MARK

```c
FEATURE_NOTE_MARK(str)
```

Places an instant marker point without requiring pairing. Used to mark a single event on the timeline.

**Parameters**:

- `str` Marker tag string.

## Scoped Instrumentation Macros

### FEATURE_NOTE_BEGIN_LOCAL / FEATURE_NOTE_END_LOCAL

```c
FEATURE_NOTE_BEGIN_LOCAL(str)
    // traced code
FEATURE_NOTE_END_LOCAL()
```

Begin/end instrumentation with local variable scope. Internally saves the tag via a local variable, avoiding complex parameter passing from the caller.

**Parameters**:

- `str` Event tag string.

**Usage example**:

```c
void my_feature_func(void)
{
    FEATURE_NOTE_BEGIN_LOCAL("my_feature_func");
    // ... business logic ...
    FEATURE_NOTE_END_LOCAL();
}
```

**Notes**:

- `FEATURE_NOTE_BEGIN_LOCAL` and `FEATURE_NOTE_END_LOCAL` must be used as a pair within the same scope. The macro is wrapped internally using a `do { ... } while(0)` pattern and relies on the compiler recognizing the scope.
- The macro internally introduces a local variable named `note_temp_str`. Do not use this variable name within the same scope.
