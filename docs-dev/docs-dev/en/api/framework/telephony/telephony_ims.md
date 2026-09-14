\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_ims.md) \]

# IMS Service API

IP Multimedia Subsystem (VoLTE/VoWiFi) management.

Header: `#include <tapi_ims.h>`

## openvela Implementation Notes

- **IMS Switch**: Controls IMS service enable/disable state via `turn_on` / `turn_off`
- **Registration Status**: Queries whether IMS is registered to the network, subscribes to registration state change events
- **Service Switch**: `set_service_status` controls enabling of specific services (e.g. voice, video)
- **VoLTE Support**: Queries whether the current network supports VoLTE via `is_volte_available`
- **SIM identification**: All interfaces include a `slot_id` parameter

## IMS Switch

### tapi_ims_turn_on

```c
int tapi_ims_turn_on(tapi_context context, int slot_id);
```

Enables IMS service (VoLTE/VoWiFi).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_ims_turn_off

```c
int tapi_ims_turn_off(tapi_context context, int slot_id);
```

Disables IMS service.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Service Status Configuration

### tapi_ims_set_service_status

```c
int tapi_ims_set_service_status(tapi_context context, int slot_id, int capability);
```

Sets IMS service status (VoLTE/VoWiFi).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `capability` Capability value.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## Registration Status and Events

### tapi_ims_get_registration

```c
int tapi_ims_get_registration(tapi_context context, int slot_id, tapi_ims_registration_info* ims_reg);
```

Gets IMS registration information.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `ims_reg` IMS registration status.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_ims_register_registration_change

```c
int tapi_ims_register_registration_change(tapi_context context, int slot_id, void* user_obj, tapi_async_function p_handle);
```

Registers a callback for IMS registration state changes.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `user_obj` User object pointer.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_ims_is_registered

```c
int tapi_ims_is_registered(tapi_context context, int slot_id, bool* out);
```

Queries whether IMS is currently registered.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



## VoLTE and Service Query

### tapi_ims_is_volte_available

```c
int tapi_ims_is_volte_available(tapi_context context, int slot_id, bool* out);
```

Queries whether VoLTE is available on the current network.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_ims_get_subscriber_uri_number

```c
int tapi_ims_get_subscriber_uri_number(tapi_context context, int slot_id, char** out);
```

Gets the IMS subscriber URI number.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.



### tapi_ims_get_enabled

```c
int tapi_ims_get_enabled(tapi_context context, int slot_id, bool* out);
```

Queries whether IMS is enabled.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID (0 or 1).
- `out` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.
