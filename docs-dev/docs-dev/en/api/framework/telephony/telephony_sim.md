\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_sim.md) \]

# SIM Card Management API

SIM card status query and management.

Header file: `#include <tapi_sim.h>`

## openvela Implementation Notes

- **SIM management**: All interfaces include `slot_id` parameter for SIM card identification
- **PIN management**: Provides `enter_pin` / `change_pin` / `reset_pin` / `lock_pin` / `unlock_pin` for complete PIN/PUK workflows
- **APDU channel**: Use `open_logical_channel` / `close_logical_channel` / `transmit_apdu_*` to send APDU commands directly to the SIM card
- **UICC switch**: Control SIM card enablement state via `get_uicc_enablement` / `set_uicc_enablement`
- **Event subscription**: `tapi_sim_register` / `tapi_sim_unregister` to monitor SIM card state changes

## SIM Status Query

### tapi_sim_has_icc_card

```c
int tapi_sim_has_icc_card(tapi_context context, int slot_id, bool* out);
```

Query whether a SIM card is inserted.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_get_sim_state

```c
int tapi_sim_get_sim_state(tapi_context context, int slot_id, int* out);
```

Get the SIM card state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_get_sim_operator

```c
int tapi_sim_get_sim_operator(tapi_context context, int slot_id, int length, char* out);
```

Get the SIM card operator information.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `length` Data length.
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_get_sim_operator_name

```c
int tapi_sim_get_sim_operator_name(tapi_context context, int slot_id, char** out);
```

Get the SIM card operator name.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_get_sim_iccid

```c
int tapi_sim_get_sim_iccid(tapi_context context, int slot_id, char** out);
```

Get the SIM card ICCID.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_get_subscriber_id

```c
int tapi_sim_get_subscriber_id(tapi_context context, int slot_id, char** out);
```

Get the subscriber identity (IMSI).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Event Subscription

### tapi_sim_register

```c
int tapi_sim_register(tapi_context context, int slot_id, tapi_indication_msg msg, void* user_obj, tapi_async_function p_handle);
```

Register a SIM event callback.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `msg` Message content.
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_unregister

```c
int tapi_sim_unregister(tapi_context context, int watch_id);
```

Unregister a SIM event callback.

**Parameters**:

- `context` Telephony context handle.
- `watch_id` Watch ID (used to cancel the subscription).

**Returns**:

Returns 0 on success, or a negative error code on failure.



## PIN Management

### tapi_sim_change_pin

```c
int tapi_sim_change_pin(tapi_context context, int slot_id, int event_id, char* pin_type, char* old_pin, char* new_pin, tapi_async_function p_handle);
```

Change the SIM card PIN code.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `pin_type` PIN code type.
- `old_pin` Old PIN code.
- `new_pin` New PIN code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_enter_pin

```c
int tapi_sim_enter_pin(tapi_context context, int slot_id, int event_id, char* pin_type, char* pin, tapi_async_function p_handle);
```

Enter the SIM card PIN code.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `pin_type` PIN code type.
- `pin` PIN code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_reset_pin

```c
int tapi_sim_reset_pin(tapi_context context, int slot_id, int event_id, char* puk_type, char* puk, char* new_pin, tapi_async_function p_handle);
```

Reset the SIM card PIN using PUK code.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `puk_type` PUK code type.
- `puk` PUK code.
- `new_pin` New PIN code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_lock_pin

```c
int tapi_sim_lock_pin(tapi_context context, int slot_id, int event_id, char* pin_type, char* pin, tapi_async_function p_handle);
```

Lock the SIM card PIN.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `pin_type` PIN code type.
- `pin` PIN code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_unlock_pin

```c
int tapi_sim_unlock_pin(tapi_context context, int slot_id, int event_id, char* pin_type, char* pin, tapi_async_function p_handle);
```

Unlock the SIM card PIN.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `pin_type` PIN code type.
- `pin` PIN code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## APDU Logical Channel

### tapi_sim_open_logical_channel

```c
int tapi_sim_open_logical_channel(tapi_context context, int slot_id, int event_id, unsigned char aid[], int len, tapi_async_function p_handle);
```

Open a SIM card logical channel.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `aid` Application ID.
- `len` Length.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_close_logical_channel

```c
int tapi_sim_close_logical_channel(tapi_context context, int slot_id, int event_id, int session_id, tapi_async_function p_handle);
```

Close a SIM card logical channel.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `session_id` Session ID.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_transmit_apdu_logical_channel

```c
int tapi_sim_transmit_apdu_logical_channel(tapi_context context, int slot_id, int event_id, int session_id, unsigned char pdu[], int len, tapi_async_function p_handle);
```

Transmit an APDU command through the logical channel.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `session_id` Session ID.
- `pdu` PDU data.
- `len` Length.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_transmit_apdu_basic_channel

```c
int tapi_sim_transmit_apdu_basic_channel(tapi_context context, int slot_id, int event_id, unsigned char pdu[], int len, tapi_async_function p_handle);
```

Transmit an APDU command through the basic channel.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `pdu` PDU data.
- `len` Length.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## UICC Switch

### tapi_sim_get_uicc_enablement

```c
int tapi_sim_get_uicc_enablement(tapi_context context, int slot_id, tapi_sim_uicc_app_state* out);
```

Get the UICC enablement state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_set_uicc_enablement

```c
int tapi_sim_set_uicc_enablement(tapi_context context, int slot_id, int event_id, int state, tapi_async_function p_handle);
```

Set the UICC enablement state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `event_id` Event ID for callback matching.
- `state` State.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_sim_get_sim_invalid

```c
int tapi_sim_get_sim_invalid(tapi_context context, int slot_id, int* out);
```

Get the SIM card invalid state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.
