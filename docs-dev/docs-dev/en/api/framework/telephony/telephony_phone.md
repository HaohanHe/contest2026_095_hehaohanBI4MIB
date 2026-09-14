\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_phone.md) \]

# Telephony Phone Service API

Simplified phone service interface for lightweight clients. Compared to `tapi_call`, this module provides a more compact call control encapsulation and integrates audio type control, radio power switch, and WTP (Wireless Telephony Profile) companion interfaces.

Header: `#include <tapi_phone.h>`

## openvela Implementation Notes

- **Client session**: Started via `tapi_start_phone_service_client`, stopped via `tapi_stop_phone_service_client`
- **Callback registration**: Use `tapi_client_register_callbacks` to register a unified callback set for call events
- **No tapi_context required**: This interface internally manages the connection to the service; callers do not need to hold a `tapi_context`
- **WTP support**: Encapsulates WTP (Wireless Telephony Profile) adaptation for Bluetooth-paired watches/devices
- **Use cases**: Embedded wearable devices, simple call clients

## Service Lifecycle

### tapi_start_phone_service_client

```c
int tapi_start_phone_service_client(const char* client_name);
```

Start the phone service client.

**Parameters**:

- `client_name` Client name.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stop_phone_service_client

```c
int tapi_stop_phone_service_client(void);
```

Stop the phone service client.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_client_register_callbacks

```c
int tapi_client_register_callbacks(void* callbacks);
```

Register the client callback set.

**Parameters**:

- `callbacks` Pointer to the callback function set.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_client_unregister_callbacks

```c
int tapi_client_unregister_callbacks(void);
```

Unregister the client callbacks.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Call Control

### tapi_dial_call

```c
int tapi_dial_call(const char* number);
```

Dial a call.

**Parameters**:

- `number` Phone number string to dial.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_answer_call

```c
int tapi_answer_call(void);
```

Answer an incoming call.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_reject_call

```c
int tapi_reject_call(void);
```

Reject an incoming call.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_hangup_call

```c
int tapi_hangup_call(void);
```

Hang up the current call.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_hold_call

```c
int tapi_hold_call(void);
```

Hold the current call.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_hold_and_answer_call

```c
int tapi_hold_and_answer_call(void);
```

Hold the current call and answer a new incoming call.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_release_and_answer_call

```c
int tapi_release_and_answer_call(void);
```

Release the current call and answer a new incoming call.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_merge_call

```c
int tapi_merge_call(void);
```

Merge multiple calls into a conference call.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_send_tones

```c
int tapi_send_tones(const char* tones);
```

Send a DTMF tone sequence.

**Parameters**:

- `tones` DTMF string (`0-9 * # A-D`).

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Audio and Radio Control

### tapi_client_set_audio_type

```c
int tapi_client_set_audio_type(int type);
```

Set the audio type used during a call.

**Parameters**:

- `type` Audio type enumeration value.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_client_set_radio_power

```c
int tapi_client_set_radio_power(bool enabled);
```

Simplified radio power switch.

**Parameters**:

- `enabled` `true` to enable radio, `false` to disable.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## WTP (Wireless Telephony Profile)

### tapi_client_wtp_register_cb

```c
int tapi_client_wtp_register_cb(void* callbacks);
```

Register WTP event callbacks.

**Parameters**:

- `callbacks` Pointer to the WTP callback set.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_client_wtp_unregister_cb

```c
int tapi_client_wtp_unregister_cb(void);
```

Unregister WTP event callbacks.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_wtp_set_local_info

```c
int tapi_wtp_set_local_info(const char* info);
```

Set WTP local information (device identity, capabilities, etc.).

**Parameters**:

- `info` Local information string.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_wtp_modify_discovery

```c
int tapi_wtp_modify_discovery(int mode);
```

Modify the WTP discovery mode.

**Parameters**:

- `mode` Discovery mode enumeration value.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_wtp_modify_visibility

```c
int tapi_wtp_modify_visibility(int visibility);
```

Modify the WTP visibility configuration.

**Parameters**:

- `visibility` Visibility enumeration value.

**Returns**:

Returns `0` on success, or a negative error code on failure.
