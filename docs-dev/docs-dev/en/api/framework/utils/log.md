\[ English | [简体中文](../../../../zh-cn/api/framework/utils/log.md) \]

# ALOG Introduction

ALOG (Android Log) is a commonly used set of logging macros that provides a simplified way to record log messages. It is a macro wrapper around the `__android_log_print` function, making it more convenient and intuitive to log messages in C or C++ code.

## Data Structure Definitions

### LOG Priority

LOG priority is used to control filtering of different log levels. The default output LOG level can be controlled via `CONFIG_ALOG`:

```c
typedef enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0, // Unknown
    ANDROID_LOG_DEFAULT, // Default
    ANDROID_LOG_VERBOSE, // Verbose
    ANDROID_LOG_DEBUG, // Debug
    ANDROID_LOG_INFO, // Info
    ANDROID_LOG_WARN, // Warning
    ANDROID_LOG_ERROR, // Error
    ANDROID_LOG_FATAL, // Fatal
    ANDROID_LOG_SILENT, // Silent
} android_LogPriority;
```

### LOG ID

LOG ID is used to identify a specific log buffer, used with `__android_log_buf_write()` and `__android_log_buf_print()`.

```c
typedef enum log_id {
    LOG_ID_MIN = 0,
    LOG_ID_MAIN = 0,
    LOG_ID_RADIO = 1,
    LOG_ID_EVENTS = 2,
    LOG_ID_SYSTEM = 3,
    LOG_ID_CRASH = 4,
    LOG_ID_STATS = 5,
    LOG_ID_SECURITY = 6,
    LOG_ID_KERNEL = 7,
    LOG_ID_MAX,

    LOG_ID_DEFAULT = 0x7FFFFFFF
} log_id_t;
```

## API List

Below are the definitions and parameter descriptions of the raw LOG APIs:

```c
/**
 * @brief Write a string to the log system.
 *
 * @param prio The priority of the log message, using `android_LogPriority` enum values.
 * @param tag The tag associated with the log message.
 * @param text The constant string to log.
 * @return int Returns 0 on success, non-zero on failure.
 */
int __android_log_write(int prio, const char* tag, const char* text);

/**
 * @brief Write a formatted log message.
 *
 * @param prio The priority of the log message, using `android_LogPriority` enum values.
 * @param tag The tag associated with the log message.
 * @param fmt Format string, followed by a series of arguments.
 * @return int Returns 0 on success, non-zero on failure.
 */
int __android_log_print(int prio, const char* tag, const char* fmt, ...);

/**
 * @brief Write a formatted log message using a variable argument list.
 *
 * @param prio The priority of the log message, using `android_LogPriority` enum values.
 * @param tag The tag associated with the log message.
 * @param fmt Format string.
 * @param ap Variable argument list containing all parameters.
 * @return int Returns 0 on success, non-zero on failure.
 */
int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap);

/**
 * @brief Write an assertion message to the log system when a condition fails.
 *
 * @param cond String representation of the assertion condition.
 * @param tag The tag associated with the log message.
 * @param fmt Format string, followed by a series of arguments (optional).
 */
void __android_log_assert(const char* cond, const char* tag, const char* fmt, ...);
```

In addition to the raw APIs, ALOG provides a set of macros for simplified usage:

```c
/**
 * @brief Send a log message at the specified level.
 *
 * @param ... Variable argument list, format and parameters similar to printf.
 */
#define ALOGV(...) ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#define ALOGD(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define ALOGI(...) ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__))
#define ALOGW(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#define ALOGE(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))

/**
 * @brief Write a log message when the condition is true.
 *
 * @param cond The condition to evaluate.
 * @param ... Variable argument list, format and parameters similar to printf.
 */
#define ALOGV_IF(cond, ...)
#define ALOGD_IF(cond, ...)
#define ALOGI_IF(cond, ...)
#define ALOGW_IF(cond, ...)
#define ALOGE_IF(cond, ...)
```

## Usage Example

```c
#include <log/log.h>

#define LOG_TAG "MyAppTag"

void log_info_alogi() {
}

int main() {
    // Print log with custom priority
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Formatted number: %d", 42);

    // Print INFO level log using macro
    ALOGI("ALOGI: A log message from my app.");
    return 0;
}
```
