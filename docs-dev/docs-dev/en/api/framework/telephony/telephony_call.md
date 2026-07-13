\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_call.md) \]

# Call Management API

Voice call control, including dialing, answering, hanging up, holding, and more.

Header: `#include <tapi_call.h>`

## openvela Implementation Notes

- **SIM identification**: Some interfaces do not take a `slot_id` and use the default slot; use `tapi_call_set_default_slot` to switch when a specific slot is needed
- **Synchronous/Asynchronous**: Time-consuming operations such as dialing and answering provide both synchronous versions and `_async` versions (callback-style)
- **Operate by ID**: Long-lived calls are uniquely identified by the call ID (string) returned; `*_by_id` interfaces operate on the call accordingly
- **DTMF**: Keypad tones are triggered via `tapi_call_send_tones` (batch) or `tapi_call_start_dtmf` / `tapi_call_stop_dtmf` (continuous key press)
- **Conference calls**: Organized via `tapi_call_dial_conferece` and `tapi_call_merge_call`, and split via `tapi_call_separate_call`

## Dialing and Answering

### tapi_call_dial

```c
int tapi_call_dial(tapi_context context, int slot_id, char* number, int hide_callerid, int event_id, tapi_async_function p_handle);
```

Initiate a voice call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `number` Phone number.
- `hide_callerid` Whether to hide the caller ID.
- `event_id` Event ID, used for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_dial_async

```c
int tapi_call_dial_async(tapi_context context, int slot_id, char* number, int hide_callerid, int event_id, void* user_data, tapi_async_function p_handle);
```

Initiate a voice call (asynchronous version).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `number` Phone number.
- `hide_callerid` Whether to hide the caller ID.
- `event_id` Event ID, used for callback matching.
- `user_data` User data passed to the callback function.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Hangup Control

### tapi_call_hangup_all_calls

```c
int tapi_call_hangup_all_calls(tapi_context context, int slot_id);
```

Hang up all calls.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Hold and Swap

### tapi_call_release_and_answer

```c
int tapi_call_release_and_answer(tapi_context context, int slot_id);
```

Release the current call and answer the waiting incoming call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_hold_and_answer

```c
int tapi_call_hold_and_answer(tapi_context context, int slot_id);
```

Hold the current call and answer the waiting incoming call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_release_and_swap

```c
int tapi_call_release_and_swap(tapi_context context, int slot_id);
```

Release the current call and swap to the held call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_hold_call

```c
int tapi_call_hold_call(tapi_context context, int slot_id);
```

Hold the current call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_unhold_call

```c
int tapi_call_unhold_call(tapi_context context, int slot_id);
```

Resume the held call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Transfer and Conference

### tapi_call_transfer

```c
int tapi_call_transfer(tapi_context context, int slot_id);
```

Transfer the call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_merge_call

```c
int tapi_call_merge_call(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Merge calls (multi-party call).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID, used for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_merge_call_async

```c
int tapi_call_merge_call_async(tapi_context context, int slot_id, int event_id, void* user_data, tapi_async_function p_handle);
```

Merge calls (multi-party call) (asynchronous version).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID, used for callback matching.
- `user_data` User data passed to the callback function.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_separate_call

```c
int tapi_call_separate_call(tapi_context context, int slot_id, int event_id, char* call_id, tapi_async_function p_handle);
```

Separate a specified call from a multi-party call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID, used for callback matching.
- `call_id` Call ID.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_hangup_multiparty

```c
int tapi_call_hangup_multiparty(tapi_context context, int slot_id);
```

Hang up a conference call session.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



## DTMF and Dial Tones

### tapi_call_send_tones

```c
int tapi_call_send_tones(void* context, int slot_id, char* tones);
```

Send a DTMF tone playing request.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `tones` DTMF tone sequence.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Call Queries

### tapi_call_get_all_calls

```c
int tapi_call_get_all_calls(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Get all current calls.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID, used for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_get_call_by_state

```c
int tapi_call_get_call_by_state(tapi_context context, int slot_id, int state, tapi_call_info* call_list, int size, tapi_call_info* out_list);
```

Filter calls by the given call state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `state` State.
- `call_list` Call list.
- `size` Size.
- `out_list` Output list.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Emergency Numbers

### tapi_call_get_ecc_list

```c
int tapi_call_get_ecc_list(tapi_context context, int slot_id, ecc_info* out);
```

Get the emergency call number list.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_is_emergency_number

```c
int tapi_call_is_emergency_number(tapi_context context, char* number);
```

Check whether the given number is an emergency number.

**Parameters**:

- `context` Telephony context handle.
- `number` Phone number.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Event Subscription

### tapi_call_register_emergency_list_change

```c
int tapi_call_register_emergency_list_change(tapi_context context, int slot_id, void* user_obj, tapi_async_function p_handle);
```

Register a callback for emergency number list changes.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_register_ringback_tone_change

```c
int tapi_call_register_ringback_tone_change(tapi_context context, int slot_id, void* user_obj, tapi_async_function p_handle);
```

Register a callback for ringback tone changes.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_register_default_voicecall_slot_change

```c
int tapi_call_register_default_voicecall_slot_change(tapi_context context, void* user_obj, tapi_async_function p_handle);
```

Register a callback for default voice call slot changes.

**Parameters**:

- `context` Telephony context handle.
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Conference Calls

### tapi_call_dial_conferece

```c
int tapi_call_dial_conferece(tapi_context context, int slot_id, char* participants[], int size);
```

Initiate an IMS conference call.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `participants` Participant list.
- `size` Size.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_invite_participants

```c
int tapi_call_invite_participants(tapi_context context, int slot_id, char* participants[], int size);
```

Request the conference server to invite additional participants to the conference.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `participants` Participant list.
- `size` Size.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_register_call_state_change

```c
int tapi_call_register_call_state_change(tapi_context context, int slot_id, void* user_obj, tapi_async_function p_handle);
```

Register a callback for call state changes.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Operate by ID

### tapi_call_answer_by_id

```c
int tapi_call_answer_by_id(tapi_context context, int slot_id, char* call_id);
```

Answer a call by its ID.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `call_id` Call ID.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_answer_by_id_async

```c
int tapi_call_answer_by_id_async(tapi_context context, int slot_id, char* call_id, void* user_obj, tapi_async_function p_handle);
```

Answer a call by its ID (asynchronous version; the result is returned via callback).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `call_id` Call ID.
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_hangup_by_id

```c
int tapi_call_hangup_by_id(tapi_context context, int slot_id, char* call_id);
```

Hang up a call by its ID.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `call_id` Call ID.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_deflect_by_id

```c
int tapi_call_deflect_by_id(tapi_context context, int slot_id, char* call_id, char* number);
```

Deflect an incoming call to the specified number.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `call_id` Call ID.
- `number` Phone number.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Continuous DTMF

### tapi_call_start_dtmf

```c
int tapi_call_start_dtmf(tapi_context context, int slot_id, unsigned char digit, int event_id, tapi_async_function p_handle);
```

Start sending a DTMF tone.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `digit` DTMF key character.
- `event_id` Event ID, used for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_stop_dtmf

```c
int tapi_call_stop_dtmf(tapi_context context, int slot_id, int event_id, tapi_async_function p_handle);
```

Stop sending a DTMF tone.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).
- `event_id` Event ID, used for callback matching.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Default Slot

### tapi_call_set_default_slot

```c
int tapi_call_set_default_slot(tapi_context context, int slot_id);
```

Set the default voice call slot.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_call_get_default_slot

```c
int tapi_call_get_default_slot(tapi_context context, int* out);
```

Get the default voice call slot.

**Parameters**:

- `context` Telephony context handle.
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.

