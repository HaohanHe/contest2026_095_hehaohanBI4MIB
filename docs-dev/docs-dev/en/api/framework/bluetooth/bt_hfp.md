\[ English | [简体中文](../../../../zh-cn/api/framework/bluetooth/bt_hfp.md) \]

# Bluetooth HFP API

openvela Bluetooth HFP (Hands-Free Profile) interface, supporting Bluetooth telephony functionality.

Header files: #include "bt_hfp.h", #include "bt_hfp_hf.h", #include "bt_hfp_ag.h"


## openvela Implementation Notes

- **Dual-role support**: HF (Hands-Free unit) and AG (Audio Gateway)
- **Features**: Answer/hang up calls, volume control, voice recognition, phonebook access


## Synchronous Interfaces


### bt_hfp_hf_unregister_callbacks

```c
bool bt_hfp_hf_unregister_callbacks(bt_instance_t* ins, void* cookie);
```

Unregister callback functions and stop receiving state change notifications.

**Parameters**:

- `cookie` User context.
- `ins` Bluetooth client instance.

**Returns**:

Returns true on success, or false on failure.


### bt_hfp_hf_is_connected

```c
bool bt_hfp_hf_is_connected(bt_instance_t* ins, bt_address_t* addr);
```

Check whether the HFP HF profile is connected to the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns true if connected, or false otherwise.


### bt_hfp_hf_is_audio_connected

```c
bool bt_hfp_hf_is_audio_connected(bt_instance_t* ins, bt_address_t* addr);
```

Check whether the SCO audio connection is established with the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns true if audio is connected, or false otherwise.


### bt_hfp_hf_get_connection_state

```c
profile_connection_state_t bt_hfp_hf_get_connection_state(bt_instance_t* ins, bt_address_t* addr);
```

Get the current HFP HF connection state with the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns the current profile connection state.


### bt_hfp_hf_connect

```c
bt_status_t bt_hfp_hf_connect(bt_instance_t* ins, bt_address_t* addr);
```

Initiate an HFP HF connection to a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.

**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_disconnect

```c
bt_status_t bt_hfp_hf_disconnect(bt_instance_t* ins, bt_address_t* addr);
```

Disconnect the HFP HF connection from a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_set_connection_policy

```c
bt_status_t bt_hfp_hf_set_connection_policy(bt_instance_t* ins, bt_address_t* addr, connection_policy_t policy);
```

Set the HFP HF connection policy for a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `policy` Connection policy value.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_connect_audio

```c
bt_status_t bt_hfp_hf_connect_audio(bt_instance_t* ins, bt_address_t* addr);
```

Establish a SCO audio connection with a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_disconnect_audio

```c
bt_status_t bt_hfp_hf_disconnect_audio(bt_instance_t* ins, bt_address_t* addr);
```

Disconnect the SCO audio connection from a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_start_voice_recognition

```c
bt_status_t bt_hfp_hf_start_voice_recognition(bt_instance_t* ins, bt_address_t* addr);
```

Start voice recognition on the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_stop_voice_recognition

```c
bt_status_t bt_hfp_hf_stop_voice_recognition(bt_instance_t* ins, bt_address_t* addr);
```

Stop voice recognition on the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_dial

```c
bt_status_t bt_hfp_hf_dial(bt_instance_t* ins, bt_address_t* addr, const char* number);
```

Initiate a call via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `number` Phone number to dial.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_dial_memory

```c
bt_status_t bt_hfp_hf_dial_memory(bt_instance_t* ins, bt_address_t* addr, uint32_t memory);
```

Dial a number stored in memory via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `memory` Memory location index.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_redial

```c
bt_status_t bt_hfp_hf_redial(bt_instance_t* ins, bt_address_t* addr);
```

Redial the last dialed number via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_accept_call

```c
bt_status_t bt_hfp_hf_accept_call(bt_instance_t* ins, bt_address_t* addr, hfp_call_accept_t flag);
```

Accept an incoming call via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `flag` Call accept flag.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_reject_call

```c
bt_status_t bt_hfp_hf_reject_call(bt_instance_t* ins, bt_address_t* addr);
```

Reject an incoming call via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_hold_call

```c
bt_status_t bt_hfp_hf_hold_call(bt_instance_t* ins, bt_address_t* addr);
```

Hold the current call via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_terminate_call

```c
bt_status_t bt_hfp_hf_terminate_call(bt_instance_t* ins, bt_address_t* addr);
```

Terminate the current call via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_control_call

```c
bt_status_t bt_hfp_hf_control_call(bt_instance_t* ins, bt_address_t* addr, hfp_call_control_t chld, uint8_t index);
```

Control call state via HFP CHLD command.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `chld` CHLD command type.
- `index` Call index.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_query_current_calls

```c
bt_status_t bt_hfp_hf_query_current_calls(bt_instance_t* ins, bt_address_t* addr, hfp_current_call_t** calls, int* num, bt_allocator_t allocator);
```

Query the status of all current calls (CLCC).

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `calls` Output parameter, stores the call information array.
- `num` Output parameter, stores the number of calls.
- `allocator` Memory allocator function.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_send_at_cmd

```c
bt_status_t bt_hfp_hf_send_at_cmd(bt_instance_t* ins, bt_address_t* addr, const char* cmd);
```

Send a custom AT command to the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `cmd` AT command string.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_update_battery_level

```c
bt_status_t bt_hfp_hf_update_battery_level(bt_instance_t* ins, bt_address_t* addr, uint8_t level);
```

Update the local battery level information to the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `level` Battery level value.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_volume_control

```c
bt_status_t bt_hfp_hf_volume_control(bt_instance_t* ins, bt_address_t* addr, hfp_volume_type_t type, uint8_t volume);
```

Control the volume on the remote device via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `type` Volume type (speaker or microphone).
- `volume` Volume level.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_send_dtmf

```c
bt_status_t bt_hfp_hf_send_dtmf(bt_instance_t* ins, bt_address_t* addr, char dtmf);
```

Send a DTMF tone via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `dtmf` DTMF key character.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_hf_get_subscriber_number

```c
bt_status_t bt_hfp_hf_get_subscriber_number(bt_instance_t* ins, bt_address_t* addr);
```

Get the subscriber number from the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


### bt_hfp_hf_query_current_calls_with_callback

```c
bt_status_t bt_hfp_hf_query_current_calls_with_callback(bt_instance_t* ins, bt_address_t* addr);
```

Query the status of all current calls (CLCC), with results returned via callback.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


### bt_hfp_ag_unregister_callbacks

```c
bool bt_hfp_ag_unregister_callbacks(bt_instance_t* ins, void* cookie);
```

Unregister callback functions and stop receiving state change notifications.

**Parameters**:

- `cookie` User context.
- `ins` Bluetooth client instance.

**Returns**:

Returns true on success, or false on failure.


### bt_hfp_ag_is_connected

```c
bool bt_hfp_ag_is_connected(bt_instance_t* ins, bt_address_t* addr);
```

Check whether the HFP AG profile is connected to the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns true if connected, or false otherwise.


### bt_hfp_ag_is_audio_connected

```c
bool bt_hfp_ag_is_audio_connected(bt_instance_t* ins, bt_address_t* addr);
```

Check whether the SCO audio connection is established with the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns true if audio is connected, or false otherwise.


### bt_hfp_ag_get_connection_state

```c
profile_connection_state_t bt_hfp_ag_get_connection_state(bt_instance_t* ins, bt_address_t* addr);
```

Get the current HFP AG connection state with the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns the current profile connection state.


### bt_hfp_ag_connect

```c
bt_status_t bt_hfp_ag_connect(bt_instance_t* ins, bt_address_t* addr);
```

Initiate an HFP AG connection to a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.

**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_disconnect

```c
bt_status_t bt_hfp_ag_disconnect(bt_instance_t* ins, bt_address_t* addr);
```

Disconnect the HFP AG connection from a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_connect_audio

```c
bt_status_t bt_hfp_ag_connect_audio(bt_instance_t* ins, bt_address_t* addr);
```

Establish a SCO audio connection with a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_disconnect_audio

```c
bt_status_t bt_hfp_ag_disconnect_audio(bt_instance_t* ins, bt_address_t* addr);
```

Disconnect the SCO audio connection from a remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_start_virtual_call

```c
bt_status_t bt_hfp_ag_start_virtual_call(bt_instance_t* ins, bt_address_t* addr);
```

Start a virtual call on the AG side.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_stop_virtual_call

```c
bt_status_t bt_hfp_ag_stop_virtual_call(bt_instance_t* ins, bt_address_t* addr);
```

Stop a virtual call on the AG side.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_start_voice_recognition

```c
bt_status_t bt_hfp_ag_start_voice_recognition(bt_instance_t* ins, bt_address_t* addr);
```

Start voice recognition on the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_stop_voice_recognition

```c
bt_status_t bt_hfp_ag_stop_voice_recognition(bt_instance_t* ins, bt_address_t* addr);
```

Stop voice recognition on the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_phone_state_change

```c
bt_status_t bt_hfp_ag_phone_state_change(bt_instance_t* ins, bt_address_t* addr, uint8_t num_active, uint8_t num_held, hfp_ag_call_state_t call_state, hfp_call_addrtype_t type, const char* number, const char* name);
```

Notify the remote device of a phone state change (incoming call, active call, hang up, etc.).

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `num_active` Number of active calls.
- `num_held` Number of held calls.
- `call_state` Call state.
- `type` Address type.
- `number` Phone number.
- `name` Caller name.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_notify_device_status

```c
bt_status_t bt_hfp_ag_notify_device_status(bt_instance_t* ins, bt_address_t* addr, hfp_network_state_t network, hfp_roaming_state_t roam, uint8_t signal, uint8_t battery);
```

Notify the remote device of the current device status.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `network` Network state.
- `roam` Roaming state.
- `signal` Signal strength.
- `battery` Battery level.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_volume_control

```c
bt_status_t bt_hfp_ag_volume_control(bt_instance_t* ins, bt_address_t* addr, hfp_volume_type_t type, uint8_t volume);
```

Control the volume on the remote device via HFP.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `type` Volume type (speaker or microphone).
- `volume` Volume level.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_send_at_command

```c
bt_status_t bt_hfp_ag_send_at_command(bt_instance_t* ins, bt_address_t* addr, const char* at_command);
```

Send an AT command to the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `at_command` AT command string.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_send_vendor_specific_at_command

```c
bt_status_t bt_hfp_ag_send_vendor_specific_at_command(bt_instance_t* ins, bt_address_t* addr, const char* command, const char* value);
```

Send a vendor-specific AT command to the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `command` Vendor-specific command.
- `value` Command value.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_send_clcc_response

```c
bt_status_t bt_hfp_ag_send_clcc_response(bt_instance_t* ins, bt_address_t* addr, uint32_t index, hfp_call_direction_t dir, hfp_ag_call_state_t state, hfp_call_mode_t mode, hfp_call_mpty_type_t mpty, hfp_call_addrtype_t type, const char* number);
```

Send a CLCC response to the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `index` Call index.
- `dir` Call direction (incoming/outgoing).
- `state` Call state.
- `mode` Call mode.
- `mpty` Whether the call is a multiparty call.
- `type` Address type.
- `number` Phone number.

**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.


### bt_hfp_ag_send_cind_response

```c
bt_status_t bt_hfp_ag_send_cind_response(bt_instance_t* ins, bt_address_t* addr, hfp_network_state_t network, hfp_call_t call, hfp_callheld_t call_held, hfp_callsetup_t call_setup, uint8_t signal, hfp_roaming_state_t roam, uint8_t battery);
```

Send a CIND response to the remote device.

**Parameters**:

- `ins` Bluetooth client instance.
- `addr` Bluetooth address of the remote device.
- `network` Network state.
- `call` Call information.
- `call_held` Number of held calls.
- `call_setup` Call setup state.
- `signal` Signal strength.
- `roam` Roaming state.
- `battery` Battery level.


**Returns**:

Returns BT_STATUS_SUCCESS on success, or an error code on failure.
