\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_network.md) \]

# Network Service API

Cellular network registration, signal strength, operator information, etc.

Header: `#include <tapi_network.h>`

## openvela Implementation Notes

- **Network selection mode**: Supports automatic selection (`select_auto`) and manual selection (`select_manual`)
- **Scanning**: `tapi_network_scan` scans for available network operators
- **Cell information**: `get_serving_cellinfos` retrieves the current serving cell, `get_neighbouring_cellinfos` retrieves neighboring cells
- **SIM identification**: Most interfaces include a `slot_id` parameter to distinguish network state across different SIM slots
- **Event subscription**: `tapi_network_register` / `tapi_network_unregister` monitor registration state and signal strength changes

## Network Selection and Scanning

### tapi_network_select_auto

```c
int tapi_network_select_auto(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Automatically select a network.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_select_manual

```c
int tapi_network_select_manual(tapi_context context, int slot_id, int event_id, tapi_operator_info* network, tapi_async_function p_handle);
```

Manually select a network.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `network` Network information.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_scan

```c
int tapi_network_scan(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Scan for available network operators.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Cell Information

### tapi_network_get_serving_cellinfos

```c
int tapi_network_get_serving_cellinfos(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Get serving cell information.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_get_neighbouring_cellinfos

```c
int tapi_network_get_neighbouring_cellinfos(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Get neighboring cell information.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Voice Network State

### tapi_network_is_voice_registered

```c
int tapi_network_is_voice_registered(tapi_context context, int slot_id, bool* out);
```

Query whether voice service is registered.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_is_voice_emergency_only

```c
int tapi_network_is_voice_emergency_only(tapi_context context, int slot_id, bool* out);
```

Query whether only emergency calls are available.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_get_voice_network_type

```c
int tapi_network_get_voice_network_type(tapi_context context, int slot_id, tapi_network_type* out);
```

Get the voice network type.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_is_voice_roaming

```c
int tapi_network_is_voice_roaming(tapi_context context, int slot_id, bool* out);
```

Query whether voice is roaming.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Operator Information

### tapi_network_get_display_name

```c
int tapi_network_get_display_name(tapi_context context, int slot_id, char** out);
```

Get the operator display name.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Signal Strength

### tapi_network_get_signalstrength

```c
int tapi_network_get_signalstrength(tapi_context context, int slot_id, tapi_signal_strength* out);
```

Get the signal strength.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Registration Information

### tapi_network_get_registration_info

```c
int tapi_network_get_registration_info(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Get network registration information.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Reporting Rate and Events

### tapi_network_set_cell_info_list_rate

```c
int tapi_network_set_cell_info_list_rate(tapi_context context, int slot_id, int event_id, u_int32_t period, tapi_async_function p_handle);
```

Set the cell information list reporting rate.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `period` Period in milliseconds.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_register

```c
int tapi_network_register(tapi_context context, int slot_id, tapi_indication_msg msg, void* user_obj, tapi_async_function p_handle);
```

Register for network event notifications.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `msg` Message type.
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_unregister

```c
int tapi_network_unregister(tapi_context context, int watch_id);
```

Unregister from network event notifications.

**Parameters**:

- `context` Telephony context handle.
- `watch_id` Watch ID (used to cancel the subscription).

**Returns**:

Returns 0 on success, or a negative error code on failure.



## MCC / MNC

### tapi_network_get_mcc

```c
int tapi_network_get_mcc(tapi_context context, int slot_id, char** mcc);
```

Get the Mobile Country Code.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `mcc` Mobile Country Code.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_get_mnc

```c
int tapi_network_get_mnc(tapi_context context, int slot_id, char** mnc);
```

Get the Mobile Network Code.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `mnc` Mobile Network Code.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_get_operator_status

```c
int tapi_network_get_operator_status(tapi_context context, int slot_id, int* out);
```

Get the operator status.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_get_operator_name

```c
int tapi_network_get_operator_name(tapi_context context, int slot_id, char** out);
```

Get the operator name.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_network_get_reg_state

```c
int tapi_network_get_reg_state(tapi_context context, int slot_id, tapi_registration_state* out);
```

Get the network registration state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.
