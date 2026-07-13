\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_ss.md) \]

# Telephony Supplementary Services (SS) API

Supplementary Services (SS) are value-added call capabilities defined by 3GPP cellular standards, including Call Barring, Call Forwarding, Calling Line Identification Restriction/Presentation (CLIR/CLIP), Call Waiting, USSD, etc.

Header file: `#include <tapi_ss.h>`

## openvela Implementation Notes

- **Call Barring**: `tapi_ss_*_call_barring*` series controls the number range for outgoing/incoming calls
- **Call Forwarding**: `tapi_ss_*_call_forwarding*` series configures unconditional/busy/no-reply/unreachable forwarding
- **CLIR/CLIP**: Calling line identification display and restriction via `calling_line_restriction` and `calling_line_presentation_info` interfaces
- **USSD**: `tapi_ss_send_ussd` sends `*#xxxx#` commands, `tapi_ss_cancel_ussd` cancels the session
- **FDN**: Fixed Dialing Number switch via `tapi_ss_enable_fdn` / `tapi_ss_query_fdn`
- **SIM identification**: All interfaces include `slot_id`
- **Asynchronous callback**: All operations use `tapi_async_function`

## Call Barring

### tapi_ss_request_call_barring

```c
int tapi_ss_request_call_barring(tapi_context context, int slot_id, int event_id,
                                 char* fac, char* pin2,
                                 tapi_async_function p_handle);
```

Request a specific type of call barring (by FAC code).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `fac` Call barring FAC code (e.g., `"OI"`, `"IR"`, etc.).
- `pin2` SIM card PIN2 code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_set_call_barring_option

```c
int tapi_ss_set_call_barring_option(tapi_context context, int slot_id, int event_id,
                                    char* facility, char* pin2,
                                    tapi_async_function p_handle);
```

Set call barring options.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `facility` Barring type string.
- `pin2` SIM card PIN2 code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_get_call_barring_option

```c
int tapi_ss_get_call_barring_option(tapi_context context, int slot_id,
                                    const char* service_type, char** out);
```

Query the current call barring configuration.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `service_type` Service type string.
- `out` Output parameter, returns the configuration string.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_change_call_barring_password

```c
int tapi_ss_change_call_barring_password(tapi_context context, int slot_id, int event_id,
                                         char* old_pin, char* new_pin,
                                         tapi_async_function p_handle);
```

Change the call barring service password.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `old_pin` Old password.
- `new_pin` New password.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_disable_all_call_barrings

```c
int tapi_ss_disable_all_call_barrings(tapi_context context, int slot_id, int event_id,
                                      char* passwd, tapi_async_function p_handle);
```

Disable all call barrings.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `passwd` Service password.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_disable_all_incoming

```c
int tapi_ss_disable_all_incoming(tapi_context context, int slot_id,
                                 int event_id, char* passwd,
                                 tapi_async_function p_handle);
```

Disable all incoming call barrings.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `passwd` Service password.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_disable_all_outgoing

```c
int tapi_ss_disable_all_outgoing(tapi_context context, int slot_id,
                                 int event_id, char* passwd,
                                 tapi_async_function p_handle);
```

Disable all outgoing call barrings.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `passwd` Service password.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Call Forwarding

### tapi_ss_query_call_forwarding_option

```c
int tapi_ss_query_call_forwarding_option(tapi_context context, int slot_id, int event_id,
                                         int cf_reason, tapi_async_function p_handle);
```

Query call forwarding configuration.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `cf_reason` Forwarding type (unconditional/busy/no-reply/unreachable, see `tapi_call_forward_option`).
- `p_handle` Asynchronous callback function, returns `tapi_call_forwarding_info` on callback.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_set_call_forwarding_option

```c
int tapi_ss_set_call_forwarding_option(tapi_context context, int slot_id, int event_id,
                                       tapi_call_forwarding_info* info,
                                       tapi_async_function p_handle);
```

Set call forwarding configuration.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `info` Call forwarding configuration structure.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## USSD Session

### tapi_ss_initiate_service

```c
int tapi_ss_initiate_service(tapi_context context, int slot_id, int event_id,
                             char* command, tapi_async_function p_handle);
```

Initiate an SS service command (USSD/SS string format, e.g., `*#06#`).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `command` Command string.
- `p_handle` Asynchronous callback function, returns `tapi_ss_initiate_info` on callback.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_get_ussd_state

```c
int tapi_get_ussd_state(tapi_context context, int slot_id, char** out);
```

Query the current USSD session state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `out` Output parameter, returns the state string (e.g., `"idle"`, `"user-response"`).

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_send_ussd

```c
int tapi_ss_send_ussd(tapi_context context, int slot_id, int event_id, char* reply,
                     tapi_async_function p_handle);
```

Send a USSD reply message.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `reply` Reply string.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_cancel_ussd

```c
int tapi_ss_cancel_ussd(tapi_context context, int slot_id, int event_id,
                       tapi_async_function p_handle);
```

Cancel the current USSD session.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Call Waiting

### tapi_ss_set_call_waiting

```c
int tapi_ss_set_call_waiting(tapi_context context, int slot_id, int event_id, bool enable,
                             tapi_async_function p_handle);
```

Enable or disable call waiting.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `enable` `true` to enable, `false` to disable.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_get_call_waiting

```c
int tapi_ss_get_call_waiting(tapi_context context, int slot_id, int event_id,
                             tapi_async_function p_handle);
```

Query the call waiting switch state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `p_handle` Asynchronous callback function, returns the current state on callback.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## CLIR / CLIP (Calling Line Identification)

### tapi_ss_get_calling_line_presentation_info

```c
int tapi_ss_get_calling_line_presentation_info(tapi_context context, int slot_id,
                                               int event_id, tapi_async_function p_handle);
```

Query the Calling Line Identification Presentation (CLIP) state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_set_calling_line_restriction

```c
int tapi_ss_set_calling_line_restriction(tapi_context context, int slot_id, int event_id,
                                         tapi_clir_status status,
                                         tapi_async_function p_handle);
```

Set the Calling Line Identification Restriction (CLIR) state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `status` CLIR status enum value (`CLIR_DEFAULT` / `CLIR_INVOCATION` / `CLIR_SUPPRESSION`).
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_get_calling_line_restriction_info

```c
int tapi_ss_get_calling_line_restriction_info(tapi_context context, int slot_id,
                                              int event_id, tapi_async_function p_handle);
```

Query the Calling Line Identification Restriction (CLIR) state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## FDN (Fixed Dialing Number) Switch

### tapi_ss_enable_fdn

```c
int tapi_ss_enable_fdn(tapi_context context, int slot_id, int event_id,
                      bool enable, char* pin2, tapi_async_function p_handle);
```

Enable or disable FDN mode (requires PIN2).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `enable` `true` to enable FDN, `false` to disable.
- `pin2` SIM card PIN2 code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_ss_query_fdn

```c
int tapi_ss_query_fdn(tapi_context context, int slot_id, int event_id,
                     tapi_async_function p_handle);
```

Query the FDN switch state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Event Subscription

### tapi_ss_register

```c
int tapi_ss_register(tapi_context context, int slot_id, tapi_indication_msg msg,
                    void* user_obj, tapi_async_function p_handle);
```

Register an SS event callback.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `msg` Event type to monitor.
- `user_obj` User data.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns the watch ID on success, or a negative error code on failure.

### tapi_ss_unregister

```c
int tapi_ss_unregister(tapi_context context, int watch_id);
```

Unregister an SS event subscription.

**Parameters**:

- `context` Telephony context handle.
- `watch_id` Watch ID returned during subscription.

**Returns**:

Returns `0` on success, or a negative error code on failure.
