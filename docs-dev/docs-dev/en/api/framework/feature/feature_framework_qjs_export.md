\[ English | [简体中文](../../../../zh-cn/api/framework/feature/feature_framework_qjs_export.md) \]

# Feature QJS Export API

Interoperability interfaces between the Feature framework and the QuickJS runtime. Provides the ability to convert between `ft_value_t` and `JSValue`, available only in QuickJS frontend scenarios.

Header file: `#include <feature_qjs_exports.h>`

## openvela Implementation Notes

- **QuickJS only**: This set of interfaces can only be called under the QuickJS runtime. Do not use with other frontends such as WAMR.
- **Depends on QuickJS headers**: `feature_qjs_exports.h` internally includes `quickjs/quickjs.h`, requiring QuickJS public headers to be visible.
- **Typical usage**: When a Feature implementation needs to access QuickJS native APIs (e.g., creating objects using QuickJS-specific APIs), use these interfaces to convert between QuickJS types and the unified `ft_value_t` type of the Feature framework.
- **Not recommended for general business code**: Binding to QuickJS loses cross-runtime compatibility. Prefer the generic APIs in `feature_context.h`.

## JSValue and ft_value_t Conversion

### ft_from_jsvalue

```c
ft_value_t ft_from_jsvalue(ft_context_ref rt_ctx, JSValue val);
```

Converts a QuickJS `JSValue` to the Feature framework's `ft_value_t`.

**Parameters**:

- `rt_ctx` Current Feature context reference.
- `val` The QuickJS JSValue object to convert.

**Returns**:

Returns the corresponding `ft_value_t` object. The lifetime of the return value is managed by the Feature framework.

**Notes**:

- The caller must ensure the `JSValue` passed in is valid within the QuickJS Runtime corresponding to `rt_ctx`.
- If the returned `ft_value_t` is retained for a subsequent asynchronous context, use the relevant interfaces in `feature_context.h` to control its lifetime.

### ft_to_jsvalue

```c
JSValue ft_to_jsvalue(ft_context_ref rt_ctx, ft_value_t val);
```

Converts the Feature framework's `ft_value_t` to a QuickJS `JSValue`.

**Parameters**:

- `rt_ctx` Current Feature context reference.
- `val` The `ft_value_t` object to convert.

**Returns**:

Returns the corresponding `JSValue` object. The `JSValue` follows QuickJS's own reference counting rules, and the caller is responsible for releasing it via `JS_FreeValue` at the appropriate time.

**Notes**:

- This interface allocates the corresponding JS object within the QuickJS Runtime; the reference count is incremented by 1 before returning.
- You must call `JS_FreeValue` after use, otherwise it will cause a memory leak on the QuickJS side.

### ft_ctx_to_js_ctx

```c
JSContext* ft_ctx_to_js_ctx(ft_context_ref rt_ctx);
```

Retrieves the underlying QuickJS `JSContext*` from a Feature context reference.

**Parameters**:

- `rt_ctx` Current Feature context reference.

**Returns**:

Returns the corresponding `JSContext*` on success, which can be used directly as a parameter to QuickJS native APIs.

**Notes**:

- The returned `JSContext*` lifetime is managed by the Feature framework. **Do not** manually call `JS_FreeContext` to release it.
- Only returns a valid pointer under the QuickJS frontend; behavior is undefined under other frontends.
