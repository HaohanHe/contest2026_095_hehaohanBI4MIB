\[ English | [简体中文](../../../../zh-cn/api/framework/feature/feature_framework_main_export.md) \]

# Feature Main Export API

Lifecycle management and global configuration interfaces for the Feature Manager. Primarily used for QuickApp framework initialization, binding the runtime event loop, registering Features, and managing permissions.

Header file: `#include <feature_main_exports.h>`

## openvela Implementation Notes

- **Usage scenario**: This set of APIs is primarily used by QuickApp framework implementors (Runtime integration layer). Feature plugin developers generally do not call these directly.
- **Relationship with Feature Manager**: One Feature Manager corresponds to one independent QuickApp instance. Created via `FeatureCreateManager`, it must be released via `FeatureFreeManager` when no longer needed.
- **Event loop integration**: Bind a libuv event loop via `FeatureSetUVLoop` to enable scheduling of asynchronous tasks. This must be completed before `FeatureCreateInstance`.
- **Permission callback mechanism**: Register a unified permission check entry via `FeatureSetPermissionsCallback`. All Feature calls requiring permissions will trigger the callback, and the caller must explicitly `Grant` or `Reject`.

## QuickApp Framework Example Code

```cpp
#ifdef CONFIG_FEATURE_FRAMEWORK
    FeatureManagerCreateInfo ft_info;
    ft_info.raw_ctx = (FeatureRawContextHandle)(qrt->env.ctx);
    ft_info.release_cb = nullptr;
    ft_info.manager_type = FEATURE_MANAGER_JS;
    ft_info.package_name = app->packageName();
    qrt->pFeatureMgr = FeatureCreateManager(&ft_info);
    FeatureSetArgsErrorCb(qrt->pFeatureMgr, on_feature_args_error, qrt);
    FeatureSetManagerUserData(qrt->pFeatureMgr, "app", app);
    FeatureSetUVLoop(qrt->pFeatureMgr, qrt->loop);
#endif
```

## Manager Lifecycle

### FeatureCreateManager

```c
FeatureManagerHandle FeatureCreateManager(FeatureManagerCreateInfo* pinfo);
```

Creates a Feature Manager instance based on the given configuration.

**Parameters**:

- `pinfo` Creation configuration for the Feature Manager, containing the raw runtime context, release callback, manager type, and QuickApp package name. See `FeatureManagerCreateInfo` for details.

**Returns**:

Returns a valid `FeatureManagerHandle` on success; returns `NULL` on failure.

### FeatureFreeManager

```c
void FeatureFreeManager(FeatureManagerHandle handle);
```

Releases the Feature Manager. Before releasing, `FeatureUnsetUVLoop` should be called to unbind the event loop.

**Parameters**:

- `handle` The Feature Manager handle to release.

### FeatureUninit

```c
void FeatureUninit(FeatureManagerHandle handle);
```

Performs de-initialization on the Feature Manager. Cleans up internal state without releasing the handle itself.

**Parameters**:

- `handle` Feature Manager handle.

## Global Configuration

### FeatureSetArgsErrorCb

```c
void FeatureSetArgsErrorCb(FeatureManagerHandle handle, ArgsErrorCb cb, void* data);
```

Registers an argument error callback for the Feature Manager. When any Feature call has mismatched parameter types, this callback is triggered.

**Parameters**:

- `handle` Feature Manager handle.
- `cb` Argument error callback with signature `bool (*)(void* data, ArgsErrorInfo* args_info)`.
- `data` User data passed to the callback.

### FeatureSetPackageVersion

```c
void FeatureSetPackageVersion(FeatureManagerHandle handle, const char* package_version);
```

Sets the package version of the QuickApp corresponding to the current manager. The version can be queried via `FeatureGetPackageVersion`.

**Parameters**:

- `handle` Feature Manager handle.
- `package_version` QuickApp version string.

### FeatureSetUVLoop

```c
void FeatureSetUVLoop(FeatureManagerHandle handle, uv_loop_t* loop);
```

Binds a libuv event loop to the Feature Manager. All asynchronous tasks such as `FeaturePost` and `FeatureWorker*` will be scheduled on this loop.

**Parameters**:

- `handle` Feature Manager handle.
- `loop` libuv event loop pointer.

**Notes**:

- Must be called before `FeatureCreateInstance`.
- The same `uv_loop_t` can be shared by multiple Feature Managers, but it is generally recommended that each QuickApp instance has its own dedicated loop.

### FeatureUnsetUVLoop

```c
void FeatureUnsetUVLoop(FeatureManagerHandle handle);
```

Unbinds the libuv event loop from the Feature Manager. After unbinding, all pending asynchronous tasks become invalid.

**Parameters**:

- `handle` Feature Manager handle.

**Notes**:

- Must be called before `FeatureFreeManager`.

## Runtime Access

### FeatureManagerGetContext

```c
ft_context_ref FeatureManagerGetContext(FeatureManagerHandle handle);
```

Retrieves the Feature context reference from the Feature Manager, which can be used for `ft_value_t` related operations.

**Parameters**:

- `handle` Feature Manager handle.

**Returns**:

Returns `ft_context_ref`; returns `NULL` on failure.

### FeatureSetManagerUserData

```c
void FeatureSetManagerUserData(FeatureManagerHandle handle, const char* name, void* data);
```

Attaches user data to the Feature Manager by name. Can be used to share information across Feature instances.

**Parameters**:

- `handle` Feature Manager handle.
- `name` User data name (key).
- `data` User data pointer.

### FeatureHasFeature

```c
bool FeatureHasFeature(FeatureManagerHandle handle, FtString feature_method);
```

Checks whether a Feature with the given name is registered in the current manager.

**Parameters**:

- `handle` Feature Manager handle.
- `feature_method` The Feature name to query.

**Returns**:

Returns `true` if the Feature is registered; otherwise returns `false`.

## Feature Operations

### FeatureRequire

```c
ft_value_t FeatureRequire(FeatureManagerHandle handle,
                          ft_value_t binding_obj, const char* name);
```

Requests a Feature instance from the Feature Manager by name. Equivalent to `require('@system.xxx')` at the JS layer.

**Parameters**:

- `handle` Feature Manager handle.
- `binding_obj` Binding object (typically the JS global object where the Feature resides).
- `name` Feature name.

**Returns**:

Returns a `ft_value_t` wrapping the Feature instance. Returns an undefined-type `ft_value_t` on failure.

**Notes**:

- Each `FeatureRequire` call produces an independent Feature instance.

### FeatureFindFeature

```c
ft_value_t FeatureFindFeature(FeatureManagerHandle handle, const char* name);
```

Finds an already-created Feature instance without creating a new one.

**Parameters**:

- `handle` Feature Manager handle.
- `name` Feature name.

**Returns**:

Returns the `ft_value_t` corresponding to the Feature instance; returns undefined if not found.

### FeatureCreateFeature

```c
ft_value_t FeatureCreateFeature(FeatureManagerHandle handle,
                                ft_value_t prototype, ft_value_t binding_obj);
```

Creates a Feature instance from a prototype. Used in advanced scenarios that require direct manipulation of prototype objects.

**Parameters**:

- `handle` Feature Manager handle.
- `prototype` Feature prototype object.
- `binding_obj` Binding object.

**Returns**:

Returns the `ft_value_t` of the newly created Feature instance on success; returns undefined on failure.

## Memory Diagnostics

### FeatureDumpMemory

```c
void FeatureDumpMemory(FeatureManagerHandle feature_manager,
                       FeatureMemoryDump* dump, void* userdata);
```

Callback-based memory usage diagnostic interface for the Feature framework, allowing the upper layer to integrate custom memory statistics capabilities.

**Parameters**:

- `feature_manager` Feature Manager handle.
- `dump` Memory diagnostic callback structure containing `count`, `count_meta`, and `sub` callbacks. See `FeatureMemoryDump` for details.
- `userdata` User data passed through to each callback.

## Permission Management

### FeatureSetPermissionsCallback

```c
void FeatureSetPermissionsCallback(FeatureManagerHandle hmanager,
                                   FeaturePermissionsCb cb, void* data);
```

Registers a permission check callback. When a Feature API requires permissions, the framework triggers this callback, and the business layer decides whether to grant or reject.

**Parameters**:

- `hmanager` Feature Manager handle.
- `cb` Permission check callback with signature `void (*)(FeaturePermissionsHandle, const FeaturePermissionsInfo*, void*)`.
- `data` User data passed through to the callback.

**Notes**:

- The callback must call either `FeatureGrantPermissions` or `FeatureRejectPermissions`; otherwise the corresponding Feature call will remain suspended.

### FeatureGrantPermissions

```c
void FeatureGrantPermissions(FeatureManagerHandle hmanager,
                             FeaturePermissionsHandle handle);
```

Grants a permission request. After calling, the corresponding Feature API call continues execution.

**Parameters**:

- `hmanager` Feature Manager handle.
- `handle` Permission request handle (passed in by the permission callback).

### FeatureRejectPermissions

```c
void FeatureRejectPermissions(FeatureManagerHandle hmanager,
                              FeaturePermissionsHandle handle,
                              FeaturePermsRejectReason reason);
```

Rejects a permission request. After calling, the corresponding Feature API call returns a permission error.

**Parameters**:

- `hmanager` Feature Manager handle.
- `handle` Permission request handle.
- `reason` Rejection reason. See `FeaturePermsRejectReason`:
    - `FEATURE_PERMS_DENIED`: Permission denied
    - `FEATURE_PERMS_ERROR`: Permission check error
    - `FEATURE_PERMS_NO_BG`: Background calls not allowed
