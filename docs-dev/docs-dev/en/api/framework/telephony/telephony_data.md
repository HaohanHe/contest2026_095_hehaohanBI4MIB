\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_data.md) \]

# Data Connection API

Cellular data connection management.

Header file: `#include <tapi_data.h>`

## openvela Implementation Notes

- **APN context**: Manage APN configurations (add/remove/edit/query) via the `tapi_data_*_apn_context` series of interfaces
- **On-demand connection**: `tapi_data_request_network` / `tapi_data_release_network` controls data network establishment and release
- **Roaming control**: Explicitly toggle data roaming via `tapi_data_enable_roaming`
- **SIM identification**: Operations involving a specific SIM use the `slot_id` parameter; the default data SIM is set via `tapi_data_set_default_slot`
- **State subscription**: `tapi_data_register` / `tapi_data_unregister` for registering/unregistering state change events

## APN Configuration Management

### tapi_data_load_apn_contexts

```c
int tapi_data_load_apn_contexts(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Load APN configurations.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_add_apn_context

```c
int tapi_data_add_apn_context(tapi_context context, int slot_id, int event_id, tapi_data_context* apn, tapi_async_function p_handle);
```

Add an APN configuration.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `apn` Access Point Name (APN) configuration.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_remove_apn_context

```c
int tapi_data_remove_apn_context(tapi_context context, int slot_id, int event_id, tapi_data_context* apn, tapi_async_function p_handle);
```

Remove an APN configuration.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `apn` Access Point Name (APN) configuration.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_edit_apn_context

```c
int tapi_data_edit_apn_context(tapi_context context, int slot_id, int event_id, tapi_data_context* apn, tapi_async_function p_handle);
```

Edit an APN configuration.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `apn` Access Point Name (APN) configuration.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_reset_apn_contexts

```c
int tapi_data_reset_apn_contexts(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Reset APN configurations to defaults.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Data Network Status

### tapi_data_is_registered

```c
int tapi_data_is_registered(tapi_context context, int slot_id, bool* out);
```

Query whether the data network is registered.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_is_data_emergency_only

```c
int tapi_data_is_data_emergency_only(tapi_context context, int slot_id, bool* out);
```

Query whether the data connection is emergency-only.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_get_network_type

```c
int tapi_data_get_network_type(tapi_context context, int slot_id, tapi_network_type* out);
```

Get the current network type.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_is_data_roaming

```c
int tapi_data_is_data_roaming(tapi_context context, int slot_id, bool* out);
```

Query whether data roaming is active.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Data Connection Control

### tapi_data_request_network

```c
int tapi_data_request_network(tapi_context context, int slot_id, const char* type);
```

Request to establish a data connection.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `type` Type.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_release_network

```c
int tapi_data_release_network(tapi_context context, int slot_id, const char* type);
```

Release a data connection.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `type` Type.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_get_data_connection_list

```c
int tapi_data_get_data_connection_list(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Get the data connection list.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Preferred APN

### tapi_data_set_preferred_apn

```c
int tapi_data_set_preferred_apn(tapi_context context, int slot_id, tapi_data_context* apn);
```

Set the preferred APN.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `apn` Access Point Name (APN) configuration.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_get_preferred_apn

```c
int tapi_data_get_preferred_apn(tapi_context context, int slot_id, char** out);
```

Get the preferred APN.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Data Switch

### tapi_data_enable_data

```c
int tapi_data_enable_data(tapi_context context, bool enabled);
```

Enable or disable cellular data.

**Parameters**:

- `context` Telephony context handle.
- `enabled` Whether to enable.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_get_enabled

```c
int tapi_data_get_enabled(tapi_context context, bool* out);
```

Query whether cellular data is enabled.

**Parameters**:

- `context` Telephony context handle.
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Roaming Control

### tapi_data_enable_roaming

```c
int tapi_data_enable_roaming(tapi_context context, bool enabled);
```

Enable or disable data roaming.

**Parameters**:

- `context` Telephony context handle.
- `enabled` Whether to enable.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_get_roaming_enabled

```c
int tapi_data_get_roaming_enabled(tapi_context context, bool* out);
```

Query whether data roaming is enabled.

**Parameters**:

- `context` Telephony context handle.
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Default Slot and Authorization

### tapi_data_set_default_slot

```c
int tapi_data_set_default_slot(tapi_context context, int slot_id);
```

Set the default data SIM card slot.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_get_default_slot

```c
int tapi_data_get_default_slot(tapi_context context, int* out);
```

Get the default data SIM card slot.

**Parameters**:

- `context` Telephony context handle.
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_set_data_allow

```c
int tapi_data_set_data_allow(tapi_context context, int slot_id, int event_id, bool allowed, tapi_async_function p_handle);
```

Set data permission for the specified slot.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `allowed` Whether to allow.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Event Subscription

### tapi_data_register

```c
int tapi_data_register(tapi_context context, int slot_id, tapi_indication_msg msg, void* user_obj, tapi_async_function p_handle);
```

Register a data state change event callback.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `msg` Message content.
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_data_unregister

```c
int tapi_data_unregister(tapi_context context, int watch_id);
```

Unregister a data state change event callback.

**Parameters**:

- `context` Telephony context handle.
- `watch_id` Watch ID (used to cancel the subscription).

**Returns**:

Returns 0 on success, or a negative error code on failure.
