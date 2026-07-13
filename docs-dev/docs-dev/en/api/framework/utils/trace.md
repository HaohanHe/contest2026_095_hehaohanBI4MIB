\[ English | [简体中文](../../../../zh-cn/api/framework/utils/trace.md) \]

# ATrace Introduction

ATrace (Android Trace) provides a set of application-layer Trace APIs. You can use these APIs to instrument your application for performance analysis and optimize execution efficiency.

## API Reference

```c
// Header file
#include <cutils/trace.h>

// Check whether Trace is enabled. Can be used to conditionally execute tracing code to reduce performance overhead in non-tracing mode.
ATRACE_ENABLED()

// Begin tracing a context (typically used for function execution time tracking). The name parameter identifies the context.
ATRACE_BEGIN(name)

// End tracing a context. This call should be paired with a corresponding ATRACE_BEGIN and executed after it.
ATRACE_END()

// Begin tracing an asynchronous event. Unlike ATRACE_BEGIN/ATRACE_END, asynchronous events do not need to be nested.
// name describes the event, while cookie provides a unique identifier to distinguish simultaneous events.
// The name and cookie used when starting and ending the event must be consistent.
ATRACE_ASYNC_BEGIN(name, cookie)

// End tracing an asynchronous event. This call should have a corresponding ATRACE_ASYNC_BEGIN.
ATRACE_ASYNC_END(name, cookie)

// Begin tracing an asynchronous event. In addition to name and cookie, a track_name parameter specifies
// the name of the track where this asynchronous event should be recorded.
// The track_name, name, and cookie used when starting the event must match those used when ending it.
ATRACE_ASYNC_FOR_TRACK_BEGIN(track_name, name, cookie)

// End tracing an asynchronous event. This call should correspond to a previous ATRACE_ASYNC_FOR_TRACK_BEGIN.
ATRACE_ASYNC_FOR_TRACK_END(track_name, cookie)

// Trace an instant context. name identifies the context.
// An instant event is an event with no defined duration, visualized as a single marker on the timeline.
ATRACE_INSTANT(name)

// Trace an instant context with a specified track name for recording the event.
// Similar to ATRACE_INSTANT, but allows placing different instant events into the same timeline track/row.
ATRACE_INSTANT_FOR_TRACK(name, track_name)

// Trace an integer counter value. name identifies the counter. This can be used to track values that change over time.
ATRACE_INT(name, value)

// Trace a 64-bit integer counter value. Used the same way as ATRACE_INT, but for larger value ranges.
ATRACE_INT64(name, value)
```

## Usage Example

```c
#define ATRACE_TAG ATRACE_TAG_ALWAYS
#include <cutils/trace.h>

int main(int argc, char *argv[])
{
    // Instrument the current function
    ATRACE_BEGIN("hello_main");
    sleep(1);
    ATRACE_INSTANT("printf");
    printf("hello world!");
    // End instrumentation
    ATRACE_END();
    return 0;
}
```

The trace dump command output looks like:

```
   hello-7   [0]   3.187400000: sched_wakeup_new: comm=hello pid=7 target_cpu=0
   hello-7   [0]   3.187400000: tracing_mark_write: B|7|hello_main
   hello-7   [0]   4.197700000: tracing_mark_write: I|7|printf
   hello-7   [0]   4.187700000: tracing_mark_write: E|7|hello_main
```
