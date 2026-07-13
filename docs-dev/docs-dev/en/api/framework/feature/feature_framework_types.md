\[ English | [简体中文](../../../../zh-cn/api/framework/feature/feature_framework_types.md) \]

# Feature Types API

Basic data type definitions for the Feature framework, used by Feature developers.

Header: `#include <feature_types.h>`

## openvela Implementation Notes

- **Frontend-agnostic**: Wraps objects from various frontends (QuickJS, WAMR, etc.) into a unified `ft_value_t` type, so Feature developers do not need to be aware of specific frontend differences
- **Type system**: Provides a unified type identifier for parameter passing through the `FeaturePrimitiveType` enum; internally encodes type information and memory management flags via the `FT_SET_PRIMITIVE_TYPE(base, flags)` macro
- **Reference counting**: `TypeFlags` marks whether a type requires managed memory (`TYPE_FLAGS_POINTER` requires `malloc/free`, while `TYPE_FLAGS_VALUE` does not)
- **Error code ranges**: General error codes start from 200, custom error codes start from 400, and developers can extend them as needed

## Primitive Type Aliases

Provides `Ft`-prefixed aliases for basic C types, making it easier to identify type semantics in Feature interfaces.

```c
typedef int       FtInt;       // Equivalent to int32_t
typedef int8_t    FtInt8;
typedef uint8_t   FtUint8;
typedef int16_t   FtInt16;
typedef uint16_t  FtUint16;
typedef int32_t   FtInt32;
typedef uint32_t  FtUint32;
typedef int64_t   FtInt64;
typedef uint64_t  FtUint64;
typedef float     FtFloat;
typedef double    FtDouble;
typedef bool      FtBool;
typedef const char*   FtString;  // Constant string
typedef ft_value_t*   FtAny;     // Generic ft_value_t reference

typedef int32_t   FtCallbackId;  // Callback ID
typedef int32_t   FtEventId;     // Event ID
typedef int32_t   FtPromiseId;   // Promise ID
```

## Handle Types

Opaque handles for core Feature framework objects. Developers can only operate on them through framework APIs and should not access the underlying structures directly.

```c
typedef void* FeatureRegistryHandle;    // Feature registry handle
typedef void* FeatureManagerHandle;     // Feature manager handle
typedef void* FeatureProtoHandle;       // Feature prototype handle (one per quick app)
typedef void* FeatureInstanceHandle;    // Feature instance handle (one per require)
typedef void* FeatureInterfaceHandle;   // Feature interface handle
typedef void* FeatureRuntimeContext;    // Frontend runtime context (e.g. QuickJS RuntimeContext)
typedef void* FeatureRawContextHandle;  // Raw runtime context

typedef struct _FeatureWorker* FeatureWorkerHandle;  // Worker handle
typedef uintptr_t FeatureType;          // Feature type flag
```

## Callback Function Types

```c
// Generic native function pointer
typedef void (*NativeFunc)(void);

// Feature async task callback
typedef void (*FeatureTaskCallback)(int status, void* data);

// Feature async task callback (extended, with instance handle)
typedef void (*FeatureTaskCallbackExt)(int status, uint64_t data,
                                       FeatureInstanceHandle feature);

// Event change listener callback
typedef void (*FeatureEventChangeListener)(FeatureInstanceHandle data,
                                           FtEventId eid,
                                           FeatureEventStatus status);

// Manager userdata release callback
typedef void (*ManagerUserdataFreeCallback)(void* data);

// Feature registration function
typedef bool (*FeatureRegistryFunc)(FeatureRegistryHandle);
```

## Enum Types

### FeatureTaskMode

Running mode for Feature asynchronous tasks.

```c
enum FeatureTaskMode {
    FEATURE_TASK_MODE_FREE   = 0,  // Async task has ended
    FEATURE_TASK_MODE_NORMAL = 1,  // Async task running normally
};
```

### FeaturePromiseType

Promise type identifier. The Feature framework supports both traditional callback and Promise asynchronous models.

```c
typedef enum FeaturePromiseType {
    FEATURE_PROMISE_TYPE_INVALID   = -1,  // Invalid type
    FEATURE_PROMISE_TYPE_PROMISE   = 0,   // Promise model
    FEATURE_PROMISE_TYPE_CALLBACKS = 1,   // Callback model
} FeaturePromiseType;
```

### TypeFlags

Memory management flags for types, used to determine whether memory needs to be freed.

```c
enum TypeFlags {
    TYPE_FLAGS_VALUE             = 1,    // Value type, no free needed
    TYPE_FLAGS_POINTER,                  // Pointer type, requires malloc/free
    TYPE_FLAGS_RAWPOINTER         = TYPE_FLAGS_POINTER | 1,  // Raw pointer, no free needed
    TYPE_FLAGS_UNMANAGED_POINTER  = TYPE_FLAGS_RAWPOINTER,   // Unmanaged pointer
};
```

### FeaturePrimitiveType

Primitive type encoding used for parameter passing. The macro `FT_SET_PRIMITIVE_TYPE(base, flags)` combines the type base value with memory management flags.

```c
#define FT_SET_PRIMITIVE_TYPE(base, flags) ((base << 2) | (flags))

enum FeaturePrimitiveType {
    FT_VOID      = FT_SET_PRIMITIVE_TYPE(FT_VOID_BASE,    TYPE_FLAGS_VALUE),    // 1
    FT_INT       = FT_SET_PRIMITIVE_TYPE(FT_INT_BASE,     TYPE_FLAGS_VALUE),    // 5
    FT_INT8      = FT_SET_PRIMITIVE_TYPE(FT_INT8_BASE,    TYPE_FLAGS_VALUE),    // 9
    FT_UINT8     = FT_SET_PRIMITIVE_TYPE(FT_UINT8_BASE,   TYPE_FLAGS_VALUE),    // 13
    FT_INT16     = FT_SET_PRIMITIVE_TYPE(FT_INT16_BASE,   TYPE_FLAGS_VALUE),    // 17
    FT_UINT16    = FT_SET_PRIMITIVE_TYPE(FT_UINT16_BASE,  TYPE_FLAGS_VALUE),    // 21
    FT_INT32     = FT_SET_PRIMITIVE_TYPE(FT_INT32_BASE,   TYPE_FLAGS_VALUE),    // 25
    FT_UINT32    = FT_SET_PRIMITIVE_TYPE(FT_UINT32_BASE,  TYPE_FLAGS_VALUE),    // 29
    FT_INT64     = FT_SET_PRIMITIVE_TYPE(FT_INT64_BASE,   TYPE_FLAGS_VALUE),    // 33
    FT_UINT64    = FT_SET_PRIMITIVE_TYPE(FT_UINT64_BASE,  TYPE_FLAGS_VALUE),    // 37
    FT_FLOAT     = FT_SET_PRIMITIVE_TYPE(FT_FLOAT_BASE,   TYPE_FLAGS_VALUE),    // 41
    FT_DOUBLE    = FT_SET_PRIMITIVE_TYPE(FT_DOUBLE_BASE,  TYPE_FLAGS_VALUE),    // 45
    FT_BOOLEAN   = FT_SET_PRIMITIVE_TYPE(FT_BOOLEAN_BASE, TYPE_FLAGS_VALUE),    // 49
    FT_STRING    = FT_SET_PRIMITIVE_TYPE(FT_STRING_BASE,  TYPE_FLAGS_POINTER),  // 54
    FT_CHAR      = FT_STRING,                                                   // 54, equivalent to FT_STRING
    FT_ANY_REF   = FT_SET_PRIMITIVE_TYPE(FT_ANY_REF_BASE,  TYPE_FLAGS_POINTER), // 58
    FT_JSON_OBJ  = FT_SET_PRIMITIVE_TYPE(FT_JSON_OBJ_BASE, TYPE_FLAGS_POINTER), // 62
};
```

### FeatureErrorCode

Error code definitions for the Feature framework.

```c
typedef enum FeatureErrorCode {
    FT_ERR_GENERAL           = 200,   // General error
    FT_ERR_ARGS              = 202,   // Argument error
    FT_ERR_TIMEOUT           = 204,   // Timeout
    FT_ERR_IOERROR           = 300,   // IO error
    FT_ERR_PATH_NOT_EXISTS   = 301,   // Path does not exist
    FT_ERR_CUSTOM_BEGIN      = 400,   // Custom error code start
    FT_ERR_TASK_FAILED       = 1000,  // Task failed
    FT_ERR_TASK_NOT_EXISTS   = 1001,  // Task does not exist
    FT_ERR_CANCEL_ERROR_CODE = 1002,  // Cancellation error
} FeatureErrorCode;
```

### FeatureEventStatus

Event change status, used to notify listeners when events are added or removed.

```c
typedef enum FeatureEventStatus {
    FEATURE_EVENT_ADDED,    // Event added
    FEATURE_EVENT_REMOVED,  // Event removed
} FeatureEventStatus;
```

### FeaturePermsRejectReason

Permission rejection reason.

```c
typedef enum FeaturePermsRejectReason {
    FEATURE_PERMS_DENIED = 400,  // Permission denied
    FEATURE_PERMS_ERROR,         // Permission error
    FEATURE_PERMS_NO_BG,         // Background not allowed
} FeaturePermsRejectReason;
```

### FeatureWorkerCancelResult

Worker cancellation result.

```c
enum FeatureWorkerCancelResult {
    FeatureWorkerCancelSuccess,       // Successfully cancelled
    FeatureWorkerCancelPending,       // Task is pending, cannot cancel
    FeatureWorkerCancelInvalid,       // Worker is invalid
    FeatureWorkerCancelUnknownError,  // Unknown error
};
```

### FeatureWorkerState

Worker running state.

```c
enum FeatureWorkerState {
    FEATURE_WORKER_PENDING,    // Pending
    FEATURE_WORKER_RUNNING,    // Running
    FEATURE_WORKER_INVALID,    // Invalid state
    FEATURE_WORKER_RESOLVED,   // Resolved
    FEATURE_WORKER_REJECTED,   // Rejected
    FEATURE_WORKER_FINISHED,   // Finished
};
```

### FeatureManagerType

Feature manager type.

```c
typedef enum FeatureManagerType {
    FEATURE_MANAGER_JS,    // JS type Feature manager
    FEATURE_MANAGER_WAMR,  // WAMR type Feature manager
} FeatureManagerType;
```

## Structures

### FtArray

Generic dynamic array structure for the Feature framework.

```c
typedef struct FtArray {
    int32_t _size;       // Current number of elements
    int32_t _capacity;   // Current capacity
    void*   _element;    // Element pointer
} FtArray;
```

### FtJsonObject

JSON object handle, internally a flexible string.

```c
typedef struct _FtJsonObject {
    char str[0];  // Internal string data
} *FtJsonObject;
```

### AppendData

Generic union for appending elements to an array, supporting multiple primitive types.

```c
typedef union AppendData {
    int32_t     i32;  // 32-bit signed integer
    int64_t     i64;  // 64-bit signed integer
    uint32_t    u32;  // 32-bit unsigned integer
    uint64_t    u64;  // 64-bit unsigned integer
    float       f32;  // Single-precision float
    double      f64;  // Double-precision float
    void*       ptr;  // Arbitrary pointer
    const char* str;  // String
} AppendData;
```

### FtVariParams

Variable-length parameter pack.

```c
typedef struct FtVariParams {
    int32_t     vari_count;  // Parameter count
    ft_value_t* vari_args;   // Parameter array pointer
} FtVariParams;
```

### FeatureWorkerResult

Union for Worker execution results.

```c
typedef union _FeatureWorkerResult {
    int64_t  ival;   // Signed integer result
    uint64_t uval;   // Unsigned integer result
    double   dval;   // Floating-point result
    char*    str;    // String result
    void*    ptr;    // Pointer result
} FeatureWorkerResult;
```

### VTable

Virtual function table used when creating Feature interfaces.

```c
typedef struct VTable {
    int               size;       // Number of members
    NativeFunc        finalizer;  // Destructor function
    const NativeFunc* members;    // Member function array
} VTable;
```

### FeatureManagerCreateInfo

Configuration information required when creating a Feature manager.

```c
typedef struct FeatureManagerCreateInfo {
    FeatureRawContextHandle raw_ctx;       // Raw context handle
    ReleaseRawContextCb     release_cb;    // Raw context release callback
    FeatureManagerType      manager_type;  // Manager type (JS / WAMR)
    const char*             package_name;  // Quick app package name
} FeatureManagerCreateInfo;
```

### FeatureMemoryDump

Memory diagnostic callback structure, used to collect memory usage statistics during debugging.

```c
typedef struct {
    MemoryDumpCountCB     count;       // Single item memory count callback
    MemoryDumpCountMetaCB count_meta;  // Metadata count callback with name
    MemoryDumpSubCB       sub;         // Recursive sub-item callback
} FeatureMemoryDump;
```

### ArgsErrorInfo

Argument error information. When a Feature call has mismatched parameter types, this information is passed through the `ArgsErrorCb` callback.

```c
typedef struct {
    int         argc;        // Argument count
    void*       argv;        // Argument list pointer
    int         error_code;  // Error code
    const char* error_msg;   // Error message
} ArgsErrorInfo;
```

### FeaturePermissionsInfo

Permission check information.

```c
typedef struct FeaturePermissionsInfo {
    const FeaturePermissions* permissions;   // Permission descriptor
    const char*               api_name;      // API name
    bool                      has_async_cbs; // Whether it has async callbacks
} FeaturePermissionsInfo;
```

### FeatureRegistryTable

Feature registry table, used for batch registration of multiple Features.

```c
typedef struct _FeatureRegistryTable {
    size_t              count;    // Number of entries
    FeatureRegistryFunc data[];   // Registration function array (flexible member)
} FeatureRegistryTable, *FeatureRegistryTableHandle;
```
