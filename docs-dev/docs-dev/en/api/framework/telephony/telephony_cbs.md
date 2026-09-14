\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_cbs.md) \]

# Telephony Cell Broadcast API

Cell Broadcast Service (CBS) provides cell broadcast capabilities over cellular networks, commonly used for receiving government emergency alerts (earthquakes, tsunamis) and carrier announcements.

Header: `#include <tapi_cbs.h>`

## openvela Implementation Notes

- **Power control**: Enable/disable cell broadcast reception via `set_cell_broadcast_power_on`
- **Topic subscription**: Configure broadcast topic ranges (by channel ID) via `set_cell_broadcast_topics`
- **Event callback**: Register event callbacks via `tapi_cbs_register` to receive broadcast messages
- **SIM identification**: All interfaces include a `slot_id` parameter for multi-SIM devices
- **Related protocol**: Corresponds to the Cell Broadcast procedure defined in 3GPP TS 23.041

## Power Control

### tapi_sms_set_cell_broadcast_power_on

```c
int tapi_sms_set_cell_broadcast_power_on(tapi_context context, int slot_id, bool enabled);
```

Enable or disable cell broadcast reception.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `enabled` `true` to enable, `false` to disable.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_sms_get_cell_broadcast_power_on

```c
int tapi_sms_get_cell_broadcast_power_on(tapi_context context, int slot_id, bool* enabled);
```

Query the cell broadcast reception power state.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `enabled` Output parameter, returns the current power state.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Topic Subscription

### tapi_sms_set_cell_broadcast_topics

```c
int tapi_sms_set_cell_broadcast_topics(tapi_context context, int slot_id, char* topics);
```

Configure the cell broadcast topic range (channel ID list).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `topics` Topic string, typically comma-separated channel IDs or ranges (e.g., `"4352-4356,919"`).

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_sms_get_cell_broadcast_topics

```c
int tapi_sms_get_cell_broadcast_topics(tapi_context context, int slot_id, char** topics);
```

Query the currently configured cell broadcast topics.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `topics` Output parameter, returns the topic string (caller is responsible for freeing).

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Event Subscription

### tapi_cbs_register

```c
int tapi_cbs_register(tapi_context context, int slot_id, tapi_indication_msg msg,
                      void* user_obj, tapi_async_function p_handle);
```

Register a cell broadcast event callback.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `msg` Event type to listen for.
- `user_obj` User data, passed back to the callback function.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns the event subscription watch ID on success, or a negative error code on failure.
