\[ English | [简体中文](../../../../zh-cn/api/framework/feature/feature_framework_export.md) \]

# Feature Export API

Core runtime interfaces provided by the Feature framework to Feature developers, covering memory and array management, reference access, callbacks, Promise, events, asynchronous Workers, JSON objects, and registration.

Header file: `#include <feature_exports.h>`

## openvela Implementation Notes

- **Memory ownership**: Memory allocated by `FeatureMalloc` / `FeatureInstanceAlloc*` is managed by the framework. Use `FeatureDupValue` to increase the reference and `FeatureFreeValue` to decrease it; memory is released when the reference count reaches 0
- **Array type**: `FtArray` is managed by the framework. Create via `FeatureCreateArray` / `FeatureArrayCopy`, with support for in-place operations such as `FeatureArrayAppend` / `FeatureArrayInsertAfter`
- **Callbacks and Promises**: Feature interfaces support both Callback and Promise asynchronous models; use `FeatureGetPromiseType` to query the type used by the current invocation
- **Worker mechanism**: Workers created by `FeatureCreateWorker` run in a separate thread or uv work queue, and notify the main thread of results upon completion
- **Thread safety**: Use `FeaturePost` / `FeaturePostExt` to throw tasks back to the event loop bound to the Feature manager, avoiding cross-thread operations on `FeatureInstanceHandle`
- **Reference management**: Use `FeatureDupInstanceHandle` to extend the lifetime of a `FeatureInstanceHandle`, preventing access to a destroyed instance in asynchronous callbacks
- **Instance detach**: In asynchronous callbacks, you must use `FeatureInstanceIsDetached` to check whether the instance has been detached; once detached, the handle must not be used

## Memory Allocation

### FeatureInstanceAlloc

```c
void* FeatureInstanceAlloc(FeatureInstanceHandle handle, size_t size);
```

Allocates memory for a Feature instance. The allocated memory is managed by the framework and can be released via `FeatureFreeValue`.

**Parameters**:

- `handle` Feature instance handle.
- `size` Number of bytes to allocate.

**Returns**:

Returns the starting pointer on success, or `NULL` on failure.

### FeatureInstanceAllocType

```c
void* FeatureInstanceAllocType(FeatureInstanceHandle hInst, size_t size, FeatureType type);
```

Allocates memory for a Feature instance by type, with the allocated memory carrying a type tag.

**Parameters**:

- `hInst` Feature instance handle.
- `size` Number of bytes to allocate.
- `type` Type identifier, see `FeatureType`.

**Returns**:

Returns the starting pointer on success, or `NULL` on failure.

### FeatureInstanceAllocProtobuf

```c
void* FeatureInstanceAllocProtobuf(FeatureInstanceHandle handle,
                                    const ProtobufCMessageDescriptor* desc);
```

Allocates memory for a Feature instance based on a Protobuf message descriptor, and automatically initializes the fields.

**Parameters**:

- `handle` Feature instance handle.
- `desc` Pointer to the Protobuf message descriptor.

**Returns**:

Returns a pointer to the initialized message object on success, or `NULL` on failure.

### FeatureInstanceDupValue

```c
void* FeatureInstanceDupValue(void* ptr);
```

Increments the reference count of a pointer allocated via `FeatureInstanceAlloc*`.

**Parameters**:

- `ptr` Pointer whose reference is to be incremented; must be a return value of `FeatureInstanceAlloc*`.

**Returns**:

Returns the original pointer (for chained use).

### FeatureInstanceFreeValue

```c
void FeatureInstanceFreeValue(void* ptr);
```

Decrements the reference count of a pointer allocated via `FeatureInstanceAlloc*`. The memory is actually freed when the count reaches 0.

**Parameters**:

- `ptr` Pointer to release.

### FeatureMalloc

```c
void* FeatureMalloc(size_t size, FeatureType featureType);
```

Allocates a block of Feature-managed memory by type.

**Parameters**:

- `size` Number of bytes to allocate.
- `featureType` Type identifier.

**Returns**:

Returns a pointer on success, or `NULL` on failure.

### FeatureDupValue

```c
void* FeatureDupValue(void* ptr);
```

Increments the reference count of a Feature value. Only applies to pointers allocated by `FeatureMalloc`.

**Parameters**:

- `ptr` Pointer whose reference is to be incremented.

**Returns**:

Returns the original pointer.

### FeatureFreeValue

```c
void FeatureFreeValue(void* ptr);
```

Decrements the reference count of a Feature value. The memory is actually freed when the count reaches 0. Only applies to pointers allocated by `FeatureMalloc`.

**Parameters**:

- `ptr` Pointer to release.

### FeatureGetValueRefCount

```c
int32_t FeatureGetValueRefCount(void* ptr);
```

Gets the current reference count of a Feature value. Only valid for pointers allocated by `FeatureMalloc`.

**Parameters**:

- `ptr` Target pointer to query.

**Returns**:

Returns the reference count.

### FeatureStrCopy

```c
char* FeatureStrCopy(FeatureInstanceHandle handle, const char* str);
```

Creates a Feature-framework-managed copy from a raw C string.

**Parameters**:

- `handle` Feature instance handle.
- `str` Source string.

**Returns**:

Returns a pointer to the new string on success, or `NULL` on failure. The return value can be referenced via `FeatureStrAddRef` or released via `FeatureInstanceFreeValue`.

### FeatureStrAddRef

```c
#define FeatureStrAddRef(ptr) FeatureInstanceDupValue(ptr)
```

Increments the reference count of a Feature string. Equivalent to `FeatureInstanceDupValue`.

**Parameters**:

- `ptr` Feature string pointer.

**Returns**:

Returns the original pointer (for chained use).

## Array Management

### FeatureCreateArray

```c
FtArray* FeatureCreateArray(FeatureInstanceHandle handle, size_t capacity,
                             FeatureType element_type);
```

Creates an empty `FtArray` with the given reserved capacity.

**Parameters**:

- `handle` Feature instance handle.
- `capacity` Initial capacity.
- `element_type` Element type, see `FeatureType`.

**Returns**:

Returns a pointer to the new array on success, or `NULL` on failure.

### FeatureArrayCopy

```c
FtArray* FeatureArrayCopy(FeatureInstanceHandle handle, FeatureType element_type,
                          const void* data, size_t count);
```

Creates an `FtArray` by copying from existing data. A type-dependent deep copy is performed for each element.

**Parameters**:

- `handle` Feature instance handle.
- `element_type` Element type.
- `data` Starting pointer of the source data.
- `count` Number of elements.

**Returns**:

Returns a pointer to the new array on success, or `NULL` on failure.

### FeatureArrayCopyRaw

```c
FtArray* FeatureArrayCopyRaw(FeatureInstanceHandle handle, FeatureType element_type,
                              const void* data, size_t count);
```

Creates an `FtArray` via raw byte copy from existing data, without deep-copying elements.

**Parameters**:

- `handle` Feature instance handle.
- `element_type` Element type.
- `data` Starting pointer of the source data.
- `count` Number of elements.

**Returns**:

Returns a pointer to the new array on success, or `NULL` on failure.

### FeatureArrayResize

```c
FtArray* FeatureArrayResize(FtArray* arr, size_t new_size);
```

Resizes an array. If `new_size` is greater than the current capacity, the array grows; if smaller, it is truncated.

**Parameters**:

- `arr` Array to resize.
- `new_size` New number of elements.

**Returns**:

Returns the resized array pointer on success (may point to a new address after realloc), or `NULL` on failure.

### FeatureArrayGetLength

```c
size_t FeatureArrayGetLength(FtArray* arr);
```

Gets the current number of elements in the array.

**Parameters**:

- `arr` Target array.

**Returns**:

Returns the current number of elements.

### FeatureArrayGetData

```c
void* FeatureArrayGetData(FtArray* arr, int start);
```

Gets a pointer to elements starting from the specified index.

**Parameters**:

- `arr` Target array.
- `start` Starting index.

**Returns**:

Returns a pointer to the starting element. Returns `NULL` if the index is out of bounds.

### FeatureArrayAppend

```c
FtArray* FeatureArrayAppend(FtArray* arr, const void* data);
```

Appends an element to the end of the array (deep copy).

**Parameters**:

- `arr` Target array.
- `data` Pointer to the element data to append.

**Returns**:

Returns the array pointer on success, or `NULL` on failure.

### FeatureArrayAppendRaw

```c
FtArray* FeatureArrayAppendRaw(FtArray* arr, const void* data);
```

Appends an element to the end of the array (raw byte copy, no deep copy).

**Parameters**:

- `arr` Target array.
- `data` Pointer to the element data to append.

**Returns**:

Returns the array pointer on success, or `NULL` on failure.

### FeatureArrayClear

```c
int FeatureArrayClear(FtArray* arr);
```

Clears all elements in the array while preserving allocated capacity.

**Parameters**:

- `arr` Target array.

**Returns**:

Returns 0 on success, or a negative number on failure.

### FeatureArrayRemove

```c
int FeatureArrayRemove(FtArray* arr, int start, size_t count);
```

Removes `count` elements starting at the specified position.

**Parameters**:

- `arr` Target array.
- `start` Starting index.
- `count` Number of elements to remove.

**Returns**:

Returns 0 on success, or a negative number on failure.

### FeatureArrayInsertAfter

```c
int FeatureArrayInsertAfter(FtArray* arr, int start, const void* data, size_t count);
```

Inserts `count` elements **after** the specified position (deep copy).

**Parameters**:

- `arr` Target array.
- `start` Reference position index.
- `data` Pointer to the element data.
- `count` Number of elements to insert.

**Returns**:

Returns 0 on success, or a negative number on failure.

### FeatureArrayInsertRawAfter

```c
int FeatureArrayInsertRawAfter(FtArray* arr, int start, const void* data, size_t count);
```

Same as `FeatureArrayInsertAfter`, but performs raw byte copy.

**Parameters**:

- `arr` Target array.
- `start` Reference position index.
- `data` Pointer to the element data.
- `count` Number of elements to insert.

**Returns**:

Returns 0 on success, or a negative number on failure.

### FeatureArrayInsertBefore

```c
int FeatureArrayInsertBefore(FtArray* arr, int start, const void* data, size_t count);
```

Inserts `count` elements **before** the specified position (deep copy).

**Parameters**:

- `arr` Target array.
- `start` Reference position index.
- `data` Pointer to the element data.
- `count` Number of elements to insert.

**Returns**:

Returns 0 on success, or a negative number on failure.

### FeatureArrayInsertRawBefore

```c
int FeatureArrayInsertRawBefore(FtArray* arr, int start, const void* data, size_t count);
```

Same as `FeatureArrayInsertBefore`, but performs raw byte copy.

**Parameters**:

- `arr` Target array.
- `start` Reference position index.
- `data` Pointer to the element data.
- `count` Number of elements to insert.

**Returns**:

Returns 0 on success, or a negative number on failure.

## Feature Handles and Context Access


### FeatureGetProtoHandle

```c
FeatureProtoHandle FeatureGetProtoHandle(FeatureInstanceHandle handle);
```

Gets the corresponding prototype handle from a Feature instance handle.

**Parameters**:

- `handle` Feature instance handle.

**Returns**:

Returns the corresponding `FeatureProtoHandle`, or `NULL` on failure.

### FeatureGetProtoData

```c
void* FeatureGetProtoData(FeatureProtoHandle handle);
```

Gets the user data bound to the Feature prototype. This data is visible to all instances of that prototype.

**Parameters**:

- `handle` Feature prototype handle.

**Returns**:

Returns a pointer to the user data, or `NULL` if not bound.

### FeatureSetProtoData

```c
void FeatureSetProtoData(FeatureProtoHandle handle, void* data);
```

Attaches user data to a Feature prototype.

**Parameters**:

- `handle` Feature prototype handle.
- `data` Pointer to the user data.

### FeatureGetObjectData

```c
void* FeatureGetObjectData(FeatureInstanceHandle handle);
```

Gets the user data bound to a Feature instance.

**Parameters**:

- `handle` Feature instance handle.

**Returns**:

Returns a pointer to the user data, or `NULL` if not bound.

### FeatureSetObjectData

```c
void FeatureSetObjectData(FeatureInstanceHandle handle, void* data);
```

Attaches user data to a Feature instance.

**Parameters**:

- `handle` Feature instance handle.
- `data` Pointer to the user data.

### FeatureGetContext

```c
ft_context_ref FeatureGetContext(FeatureInstanceHandle handle);
```

Gets the runtime context (frontend-related) of a Feature instance.

**Parameters**:

- `handle` Feature instance handle.

**Returns**:

Returns an `ft_context_ref`, or `NULL` on failure.

### FeatureGetPackageName

```c
const char* FeatureGetPackageName(FeatureProtoHandle handle);
```

Gets the QuickApp package name associated with the Feature prototype.

**Parameters**:

- `handle` Feature prototype handle.

**Returns**:

Returns the package name string on success, or `NULL` on failure. The return value is managed by the framework; do not release it manually.

### FeatureGetPackageVersion

```c
const char* FeatureGetPackageVersion(FeatureProtoHandle handle);
```

Gets the QuickApp version string associated with the Feature prototype.

**Parameters**:

- `handle` Feature prototype handle.

**Returns**:

Returns the version string on success, or `NULL` on failure.

### FeatureGetEnvironmentName

```c
const char* FeatureGetEnvironmentName(FeatureProtoHandle handle);
```

Gets the name of the runtime environment in which the Feature prototype resides.

**Parameters**:

- `handle` Feature prototype handle.

**Returns**:

Returns the environment name string on success, or `NULL` on failure.

### FeatureInstanceGetManagerUserData

```c
void* FeatureInstanceGetManagerUserData(FeatureInstanceHandle handle, const char* name);
```

Gets user data from the manager that owns the Feature instance, by name.

**Parameters**:

- `handle` Feature instance handle.
- `name` User data name.

**Returns**:

Returns a pointer to the user data on success, or `NULL` if not found.

### FeatureGetManagerHandleFromInstance

```c
FeatureManagerHandle FeatureGetManagerHandleFromInstance(FeatureInstanceHandle handle);
```

Gets the handle of the Feature manager that owns the given Feature instance.

**Parameters**:

- `handle` Feature instance handle.

**Returns**:

Returns a `FeatureManagerHandle` on success, or `NULL` on failure.

### FeatureGetManagerHandleFromProto

```c
FeatureManagerHandle FeatureGetManagerHandleFromProto(FeatureProtoHandle handle);
```

Gets the handle of the Feature manager that owns the given Feature prototype.

**Parameters**:

- `handle` Feature prototype handle.

**Returns**:

Returns a `FeatureManagerHandle` on success, or `NULL` on failure.

### FeatureGetUVLoop

```c
uv_loop_t* FeatureGetUVLoop(FeatureManagerHandle handle);
```

Gets the libuv event loop bound to the Feature manager.

**Parameters**:

- `handle` Feature manager handle.

**Returns**:

Returns a `uv_loop_t*` on success, or `NULL` if not bound.

### FeatureDupInstanceHandle

```c
FeatureInstanceHandle FeatureDupInstanceHandle(FeatureInstanceHandle handle);
```

Increments the reference count of a Feature instance handle and returns the same handle (for chained use).

**Parameters**:

- `handle` Feature instance handle.

**Returns**:

Returns the original handle.

### FeatureFreeInstanceHandle

```c
void FeatureFreeInstanceHandle(FeatureInstanceHandle handle);
```

Decrements the reference count of a Feature instance handle; releases it when the count reaches 0.

**Parameters**:

- `handle` Feature instance handle.

### FeatureInstanceIsDetached

```c
bool FeatureInstanceIsDetached(FeatureInstanceHandle handle);
```

Determines whether a Feature instance has been detached from its prototype (typically during instance destruction). Asynchronous callbacks should call this before using the handle.

**Parameters**:

- `handle` Feature instance handle.

**Returns**:

Returns `true` if detached; `false` otherwise. Once detached, no further interfaces may be called on this handle.

### FeatureCreateInterface

```c
FeatureInterfaceHandle FeatureCreateInterface(FeatureInstanceHandle handle, VTable* vtable);
```

Creates a Feature interface handle based on a vtable. Used to expose methods to the frontend as an object interface.

**Parameters**:

- `handle` Feature instance handle.
- `vtable` Virtual function table, see `VTable`.

**Returns**:

Returns the new `FeatureInterfaceHandle` on success, or `NULL` on failure.

## Manager User Data Operations

### FeatureSetManagerUserData

```c
void FeatureSetManagerUserData(FeatureManagerHandle handle, const char* name, void* data);
```

Attaches user data to a Feature manager by name. A single manager may attach multiple data items under different names.

**Parameters**:

- `handle` Feature manager handle.
- `name` User data name.
- `data` Pointer to the user data.

### FeatureSetManagerUserDataWithFreeCallback

```c
void* FeatureSetManagerUserDataWithFreeCallback(FeatureManagerHandle handle,
    const char* name, void* data, ManagerUserdataFreeCallback free_cb);
```

Attaches user data by name and registers a free callback. The `free_cb` is invoked to clean up the user data when the manager is destroyed.

**Parameters**:

- `handle` Feature manager handle.
- `name` User data name.
- `data` Pointer to the user data.
- `free_cb` Free callback with signature `void (*)(void* data)`.

**Returns**:

Returns the previous value if data of the same name was already bound; otherwise returns `NULL`.

### FeatureManagerHasUserData

```c
bool FeatureManagerHasUserData(FeatureManagerHandle handle, const char* name);
```

Determines whether the manager has user data bound under the specified name.

**Parameters**:

- `handle` Feature manager handle.
- `name` User data name.

**Returns**:

Returns `true` if bound, `false` otherwise.

### FeatureGetManagerUserData

```c
void* FeatureGetManagerUserData(FeatureManagerHandle handle, const char* name);
```

Gets the user data bound to the manager by name.

**Parameters**:

- `handle` Feature manager handle.
- `name` User data name.

**Returns**:

Returns a pointer to the user data on success, or `NULL` if not found.

## Callback Management

### FeatureInvokeCallback

```c
bool FeatureInvokeCallback(FeatureInstanceHandle handle, FtCallbackId cid, ...);
```

Triggers a JS-layer callback by callback ID. Variadic arguments are passed in the order declared by the Feature interface.

**Parameters**:

- `handle` Feature instance handle.
- `cid` Callback ID.
- `...` Variadic arguments passed to the callback.

**Returns**:

Returns `true` on success, or `false` on failure (e.g., the callback ID is invalid or the instance has been detached).

### FeatureInvokeCallbackCount

```c
bool FeatureInvokeCallbackCount(FeatureInstanceHandle handle, FtCallbackId cid,
                                 int count, ...);
```

Same as `FeatureInvokeCallback`, but explicitly specifies the argument count. Suitable for scenarios with a dynamic number of arguments.

**Parameters**:

- `handle` Feature instance handle.
- `cid` Callback ID.
- `count` Argument count.
- `...` Variadic argument list.

**Returns**:

Returns `true` on success, or `false` on failure.

### FeatureRemoveCallback

```c
bool FeatureRemoveCallback(FeatureInstanceHandle handle, FtCallbackId cid);
```

Removes the callback with the specified ID from the Feature instance. After removal, further calls to `FeatureInvokeCallback` will fail.

**Parameters**:

- `handle` Feature instance handle.
- `cid` Callback ID.

**Returns**:

Returns `true` on success, or `false` if the callback ID does not exist.

### FeatureCheckCallbackId

```c
bool FeatureCheckCallbackId(FeatureInstanceHandle handle, FtCallbackId cid);
```

Checks whether the JS function corresponding to the callback ID is still valid.

**Parameters**:

- `handle` Feature instance handle.
- `cid` Callback ID.

**Returns**:

Returns `true` if the function still exists and is valid; `false` otherwise.

## Asynchronous Task Dispatch

### FeaturePost

```c
bool FeaturePost(FeatureInstanceHandle handle, FeatureTaskCallback task_cb, void* data);
```

Dispatches a task to the event loop bound to the Feature manager. Commonly used to cross back to the main thread for Feature instance operations.

**Parameters**:

- `handle` Feature instance handle.
- `task_cb` Task callback with signature `void (*)(int status, void* data)`.
- `data` User data passed to the callback.

**Returns**:

Returns `true` if the dispatch succeeded, `false` otherwise.

**Notes**:

- Use `FeatureInstanceIsDetached` inside `task_cb` to check whether the handle has been detached. The handle must not be used once detached.

### FeaturePostExt

```c
bool FeaturePostExt(FeatureInstanceHandle handle, FeatureTaskCallbackExt task_cb_ext,
                    uint64_t data);
```

Extended version of `FeaturePost`. The callback signature includes the instance handle, making it easier to access the instance directly inside the callback.

**Parameters**:

- `handle` Feature instance handle.
- `task_cb_ext` Extended task callback with signature `void (*)(int status, uint64_t data, FeatureInstanceHandle feature)`.
- `data` 64-bit user data passed to the callback.

**Returns**:

Returns `true` on success, or `false` on failure.

**Notes**:

- Also requires `FeatureInstanceIsDetached` inside the callback to check the instance state.

## Promise Management

The Feature framework supports both Callback and Promise asynchronous models. Internally, both use the same `FtPromiseId` reference. The following interfaces are used to resolve or reject asynchronous invocations.

### FeaturePromiseResolve

```c
bool FeaturePromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, ...);
```

Resolves a Promise with variadic arguments. Only a single argument is supported.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `...` The argument to pass.

**Returns**:

Returns `true` on success, or `false` on failure.

### FeaturePromiseReject

```c
bool FeaturePromiseReject(FeatureInstanceHandle handle, FtPromiseId pid,
                          int code, const char* msg);
```

Rejects a Promise with an error code and error message.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `code` Error code.
- `msg` Error message.

**Returns**:

Returns `true` on success, or `false` on failure.

### FeatureGetPromiseType

```c
enum FeaturePromiseType FeatureGetPromiseType(FeatureInstanceHandle handle, FtPromiseId pid);
```

Gets the model type used by the current asynchronous invocation. Feature implementations can use this to choose between Callback or Promise interfaces.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.

**Returns**:

Returns an `FeaturePromiseType` enum:

- `FEATURE_PROMISE_TYPE_INVALID`: Invalid (typically indicates the Promise is no longer valid)
- `FEATURE_PROMISE_TYPE_PROMISE`: Promise model
- `FEATURE_PROMISE_TYPE_CALLBACKS`: Callback model

## Typed Promise Resolve

The following set of interfaces specialize Promise resolution by type, avoiding type ambiguity from variadic arguments. All return `FT_TRUE` on success and `FT_FALSE` on failure.

### FeatureFtStringPromiseResolve

```c
FtBool FeatureFtStringPromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtString val);
```

Resolves a Promise with a string.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` String result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtIntPromiseResolve

```c
FtBool FeatureFtIntPromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtInt val);
```

Resolves a Promise with an `FtInt` (int32).

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` int32 result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtUint32PromiseResolve

```c
FtBool FeatureFtUint32PromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtUint32 val);
```

Resolves a Promise with an `FtUint32`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` uint32 result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtInt8PromiseResolve

```c
FtBool FeatureFtInt8PromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtInt8 val);
```

Resolves a Promise with an `FtInt8`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` int8 result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtUint8PromiseResolve

```c
FtBool FeatureFtUint8PromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtUint8 val);
```

Resolves a Promise with an `FtUint8`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` uint8 result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtInt16PromiseResolve

```c
FtBool FeatureFtInt16PromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtInt16 val);
```

Resolves a Promise with an `FtInt16`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` int16 result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtUint16PromiseResolve

```c
FtBool FeatureFtUint16PromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtUint16 val);
```

Resolves a Promise with an `FtUint16`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` uint16 result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtInt64PromiseResolve

```c
FtBool FeatureFtInt64PromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtInt64 val);
```

Resolves a Promise with an `FtInt64`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` int64 result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtUint64PromiseResolve

```c
FtBool FeatureFtUint64PromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtUint64 val);
```

Resolves a Promise with an `FtUint64`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` uint64 result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtFloatPromiseResolve

```c
FtBool FeatureFtFloatPromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtFloat val);
```

Resolves a Promise with an `FtFloat` (float).

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` float result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtDoublePromiseResolve

```c
FtBool FeatureFtDoublePromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtDouble val);
```

Resolves a Promise with an `FtDouble` (double).

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` double result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtBoolPromiseResolve

```c
FtBool FeatureFtBoolPromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtBool val);
```

Resolves a Promise with an `FtBool`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` bool result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtAnyPromiseResolve

```c
FtBool FeatureFtAnyPromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtAny val);
```

Resolves a Promise with an `FtAny` (`ft_value_t*`). Can be used to return results of any type.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` `ft_value_t*` result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

### FeatureFtArrayPromiseResolve

```c
FtBool FeatureFtArrayPromiseResolve(FeatureInstanceHandle handle, FtPromiseId pid, FtArray* val);
```

Resolves a Promise with an `FtArray*`.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Promise ID.
- `val` Array result.

**Returns**:

Returns `FT_TRUE` on success, or `FT_FALSE` on failure.

## Event Management

### FeatureGetEventId

```c
FtEventId FeatureGetEventId(FeatureInstanceHandle handle, const char* name);
```

Looks up the event ID for the given event name.

**Parameters**:

- `handle` Feature instance handle.
- `name` Event name.

**Returns**:

Returns a valid event ID (positive integer) on success, or `≤ 0` if the event does not exist.

### FeatureGetEventName

```c
const char* FeatureGetEventName(FeatureInstanceHandle handle, FtEventId eid);
```

Gets the event name for the given event ID.

**Parameters**:

- `handle` Feature instance handle.
- `eid` Event ID.

**Returns**:

Returns the event name string on success, or `NULL` if it does not exist.

### FeatureEmitEvent

```c
bool FeatureEmitEvent(FeatureInstanceHandle handle, FtEventId eid, ...);
```

Emits an event by event ID, allowing variadic arguments to be passed to subscribers.

**Parameters**:

- `handle` Feature instance handle.
- `eid` Event ID.
- `...` Event payload; types must match the Feature interface declaration.

**Returns**:

Returns `true` on successful emission, or `false` on failure (e.g., the event does not exist, or the instance has been detached).

### FeatureEmitEventByName

```c
bool FeatureEmitEventByName(FeatureInstanceHandle handle, const char* name, ...);
```

Emits an event by name. Internally looks up the ID via `FeatureGetEventId` before emitting.

**Parameters**:

- `handle` Feature instance handle.
- `name` Event name.
- `...` Event payload.

**Returns**:

Returns `true` on success, or `false` on failure.

### FeatureSetEventChangeListener

```c
void FeatureSetEventChangeListener(FeatureInstanceHandle handle,
                                    FeatureEventChangeListener listener);
```

Sets an event-change listener on a Feature instance. The callback is triggered when the number of subscribers changes (a subscriber is added or removed).

**Parameters**:

- `handle` Feature instance handle.
- `listener` Event-change listener with signature `void (*)(FeatureInstanceHandle, FtEventId, FeatureEventStatus)`.
    - Status `FEATURE_EVENT_ADDED` indicates a subscription was added
    - Status `FEATURE_EVENT_REMOVED` indicates a subscription was removed

### FeatureGetEventCallbackCount

```c
int FeatureGetEventCallbackCount(FeatureInstanceHandle handle, FtEventId eid);
```

Queries the number of registered subscribers for the given event ID. Can be used to skip event emission when there are no subscribers to save overhead.

**Parameters**:

- `handle` Feature instance handle.
- `eid` Event ID.

**Returns**:

Returns the number of subscribers; 0 if the event does not exist or on failure.

### FeatureGetEventCallbackCountByName

```c
static inline int FeatureGetEventCallbackCountByName(FeatureInstanceHandle handle, const char* name);
```

Queries the number of registered subscribers for the given event name. Internally implemented via `FeatureGetEventId` + `FeatureGetEventCallbackCount`.

**Parameters**:

- `handle` Feature instance handle.
- `name` Event name.

**Returns**:

Returns the number of subscribers; 0 if the event does not exist.

## Asynchronous Worker

The Worker mechanism is used to move time-consuming tasks to background execution, then notify the main thread of the result upon completion. Suitable for file I/O, compute-intensive scenarios, etc.

### FeatureCreateWorker

```c
FeatureWorkerHandle FeatureCreateWorker(FeatureInstanceHandle handle, FtPromiseId pid,
    size_t buf_size,
    void (*do_work)(FeatureWorkerHandle),
    void (*do_after_worker)(FeatureWorkerHandle),
    void (*free)(void*));
```

Creates a Worker object, ready to be dispatched for background execution.

**Parameters**:

- `handle` Feature instance handle.
- `pid` Associated Promise ID used to trigger resolve/reject after the Worker completes.
- `buf_size` Size of the Worker's private buffer, usable for passing data between the two callbacks.
- `do_work` Background execution function, run on a worker thread.
- `do_after_worker` Completion callback, run on the main thread; typically calls `FeatureWorkerResolve` or `FeatureWorkerReject` here.
- `free` Resource release function, called when the Worker is destroyed.

**Returns**:

Returns a `FeatureWorkerHandle` on success, or `NULL` on failure.

### FeatureWorkerCommit

```c
bool FeatureWorkerCommit(FeatureInstanceHandle handle, FeatureWorkerHandle hworker);
```

Commits the Worker for background execution.

**Parameters**:

- `handle` Feature instance handle.
- `hworker` Worker handle.

**Returns**:

Returns `true` on successful commit, or `false` on failure.

### FeatureWorkerResolve

```c
void FeatureWorkerResolve(FeatureInstanceHandle handle, FeatureWorkerHandle hworker,
                          FeatureWorkerResult result);
```

Notifies the Worker of a successful completion, triggering resolve on the associated Promise. Typically called inside the `do_after_worker` callback.

**Parameters**:

- `handle` Feature instance handle.
- `hworker` Worker handle.
- `result` Execution result, see `FeatureWorkerResult`.

### FeatureWorkerReject

```c
void FeatureWorkerReject(FeatureInstanceHandle handle, FeatureWorkerHandle hworker,
                         int errcode, const char* err_msg);
```

Notifies the Worker of a failed completion, triggering reject on the associated Promise.

**Parameters**:

- `handle` Feature instance handle.
- `hworker` Worker handle.
- `errcode` Error code.
- `err_msg` Error message.

### FeatureWorkerIsValid

```c
bool FeatureWorkerIsValid(FeatureInstanceHandle handle, FeatureWorkerHandle hworker);
```

Determines whether the Worker is still valid.

**Parameters**:

- `handle` Feature instance handle.
- `hworker` Worker handle.

**Returns**:

Returns `true` if valid, or `false` if invalid or already completed.

### FeatureWorkerGetState

```c
int FeatureWorkerGetState(FeatureWorkerHandle hworker);
```

Gets the current state of the Worker.

**Parameters**:

- `hworker` Worker handle.

**Returns**:

Returns a `FeatureWorkerState` enum value:

- `FEATURE_WORKER_PENDING`: Pending
- `FEATURE_WORKER_RUNNING`: Running
- `FEATURE_WORKER_INVALID`: Invalid
- `FEATURE_WORKER_RESOLVED`: Resolved
- `FEATURE_WORKER_REJECTED`: Rejected
- `FEATURE_WORKER_FINISHED`: Finished

### FeatureWorkerCancel

```c
int FeatureWorkerCancel(FeatureInstanceHandle handle, FeatureWorkerHandle hworker);
```

Attempts to cancel Worker execution. Tasks in the pending state cannot be canceled immediately.

**Parameters**:

- `handle` Feature instance handle.
- `hworker` Worker handle.

**Returns**:

Returns a `FeatureWorkerCancelResult` enum value:

- `FeatureWorkerCancelSuccess`: Successfully canceled
- `FeatureWorkerCancelPending`: Worker is pending, cancellation failed
- `FeatureWorkerCancelInvalid`: Worker is invalid
- `FeatureWorkerCancelUnknownError`: Unknown error

## JSON Object

### FeatureNewJsonObject

```c
FtJsonObject FeatureNewJsonObject(const char* str);
```

Creates a JSON object from the given string.

**Parameters**:

- `str` JSON string.

**Returns**:

Returns an `FtJsonObject` on success, or `NULL` on failure.

### FeatureAllocJsonObject

```c
FtJsonObject FeatureAllocJsonObject(size_t str_len);
```

Allocates space for a JSON object by the specified string length (content not initialized).

**Parameters**:

- `str_len` String length.

**Returns**:

Returns an `FtJsonObject` on success, or `NULL` on failure.

### FeatureGetJsonString

```c
const char* FeatureGetJsonString(const FtJsonObject json_obj);
```

Gets the string view corresponding to the JSON object.

**Parameters**:

- `json_obj` JSON object.

**Returns**:

Returns the string pointer on success, or `NULL` on failure. The returned pointer is managed by the framework; do not release it manually.

## Feature Registration

### FeatureRegisterFeatures

```c
bool FeatureRegisterFeatures(FeatureRegistryHandle handle,
                              const FeatureRegistryTableHandle regTable);
```

Registers a group of Features to the Feature registry in batch. Typically called during module initialization.

**Parameters**:

- `handle` Feature registry handle.
- `regTable` Registry entry table, see `FeatureRegistryTable`.

**Returns**:

Returns `true` if all registrations succeed, or `false` if any fails.
