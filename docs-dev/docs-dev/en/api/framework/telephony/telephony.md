\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony.md) \]

# Telephony Common Utilities API

General-purpose utility functions provided by TAPI, covering status string conversion, modem path resolution, operator status parsing, and other helper capabilities.

Header: `#include <tapi.h>`

## openvela Implementation Notes

- **String/enum conversion**: The `*_from_string` / `*_to_string` series convert between oFono D-Bus strings and TAPI enumerations for status parsing
- **Modem path**: `tapi_utils_get_modem_path` converts a `slot_id` to an oFono modem object path
- **Utility nature**: These interfaces do not depend on tapi_context and can be called directly from anywhere
- **Use cases**: Implementing custom event handling, printing debug logs, or extending TAPI capabilities

## SIM State

### tapi_sim_state_to_string

```c
const char* tapi_sim_state_to_string(tapi_sim_state state);
```

Convert a SIM state enumeration to a human-readable string.

**Parameters**:

- `state` SIM state enumeration value.

**Returns**:

Returns the string representation of the state, or `NULL` / a placeholder string on failure.

## APN Utilities

### tapi_utils_apn_auth_from_string

```c
int tapi_utils_apn_auth_from_string(const char* str);
```

Convert an authentication type string to an APN authentication enumeration value.

**Parameters**:

- `str` Authentication type string (e.g., `"pap"`, `"chap"`).

**Returns**:

Returns the authentication enumeration value; returns an error value for invalid strings.

### tapi_utils_apn_auth_to_string

```c
const char* tapi_utils_apn_auth_to_string(int auth);
```

Convert an APN authentication enumeration value to a string.

**Parameters**:

- `auth` Authentication enumeration value.

**Returns**:

Returns the corresponding string.

### tapi_utils_apn_proto_from_string

```c
int tapi_utils_apn_proto_from_string(const char* str);
```

Convert a protocol string to an APN protocol enumeration value.

**Parameters**:

- `str` Protocol string (e.g., `"ip"`, `"ipv6"`, `"dual"`).

**Returns**:

Returns the protocol enumeration value.

### tapi_utils_apn_proto_to_string

```c
const char* tapi_utils_apn_proto_to_string(int proto);
```

Convert an APN protocol enumeration value to a string.

**Parameters**:

- `proto` Protocol enumeration value.

**Returns**:

Returns the corresponding string.

### tapi_utils_apn_type_from_string

```c
int tapi_utils_apn_type_from_string(const char* str);
```

Convert an APN type string to a type enumeration value.

**Parameters**:

- `str` APN type string (e.g., `"default"`, `"mms"`, `"ims"`).

**Returns**:

Returns the type enumeration value.

### tapi_utils_apn_type_to_string

```c
const char* tapi_utils_apn_type_to_string(int type);
```

Convert an APN type enumeration value to a string.

**Parameters**:

- `type` APN type enumeration value.

**Returns**:

Returns the corresponding string.

## Call Utilities

### tapi_utils_call_disconnected_reason

```c
int tapi_utils_call_disconnected_reason(const char* reason);
```

Convert a call disconnection reason string to a TAPI disconnection reason enumeration value.

**Parameters**:

- `reason` Disconnection reason string.

**Returns**:

Returns the disconnection reason enumeration value.

### tapi_utils_call_status_from_string

```c
int tapi_utils_call_status_from_string(const char* status);
```

Convert a call status string to a status enumeration value.

**Parameters**:

- `status` Status string (e.g., `"active"`, `"held"`, `"dialing"`).

**Returns**:

Returns the status enumeration value.

## Cell and Network Utilities

### tapi_utils_cell_type_from_string

```c
int tapi_utils_cell_type_from_string(const char* str);
```

Convert a cell type string to an enumeration value.

**Parameters**:

- `str` Cell type string.

**Returns**:

Returns the cell type enumeration value.

### tapi_utils_cell_type_to_string

```c
const char* tapi_utils_cell_type_to_string(int type);
```

Convert a cell type enumeration value to a string.

**Parameters**:

- `type` Cell type enumeration value.

**Returns**:

Returns the corresponding string.

### tapi_utils_network_mode_from_string

```c
int tapi_utils_network_mode_from_string(const char* str);
```

Convert a network mode string to an enumeration value.

**Parameters**:

- `str` Network mode string (e.g., `"gsm"`, `"lte"`).

**Returns**:

Returns the network mode enumeration value.

### tapi_utils_network_mode_to_string

```c
const char* tapi_utils_network_mode_to_string(int mode);
```

Convert a network mode enumeration value to a string.

**Parameters**:

- `mode` Network mode enumeration value.

**Returns**:

Returns the corresponding string.

### tapi_utils_network_type_from_ril_tech

```c
int tapi_utils_network_type_from_ril_tech(int tech);
```

Convert a RIL layer network technology value to a TAPI network type enumeration.

**Parameters**:

- `tech` RIL network technology value.

**Returns**:

Returns the TAPI network type enumeration value.

### tapi_utils_network_operator_status_from_string

```c
int tapi_utils_network_operator_status_from_string(const char* str);
```

Convert an operator status string to an enumeration value.

**Parameters**:

- `str` Operator status string.

**Returns**:

Returns the operator status enumeration value.

### tapi_utils_operator_status_from_string

```c
int tapi_utils_operator_status_from_string(const char* str);
```

Shorthand version of `tapi_utils_network_operator_status_from_string` with equivalent functionality.

**Parameters**:

- `str` Operator status string.

**Returns**:

Returns the operator status enumeration value.

## Registration State Utilities

### tapi_utils_registration_mode_from_string

```c
int tapi_utils_registration_mode_from_string(const char* str);
```

Convert a registration mode string to an enumeration value.

**Parameters**:

- `str` Registration mode string (e.g., `"auto"`, `"manual"`).

**Returns**:

Returns the registration mode enumeration value.

### tapi_utils_registration_status_from_string

```c
int tapi_utils_registration_status_from_string(const char* str);
```

Convert a registration status string to an enumeration value.

**Parameters**:

- `str` Registration status string.

**Returns**:

Returns the registration status enumeration value.

### tapi_utils_get_registration_status_string

```c
const char* tapi_utils_get_registration_status_string(int status);
```

Convert a registration status enumeration value to a human-readable string.

**Parameters**:

- `status` Registration status enumeration value.

**Returns**:

Returns the corresponding string.

## Modem Path and Slot

### tapi_utils_get_modem_path

```c
const char* tapi_utils_get_modem_path(int slot_id);
```

Get the oFono modem object path for a given SIM slot ID.

**Parameters**:

- `slot_id` SIM slot ID (0 or 1).

**Returns**:

Returns the modem object path string (e.g., `/ril_0`, `/ril_1`).

### tapi_utils_get_slot_id

```c
int tapi_utils_get_slot_id(const char* path);
```

Parse the SIM slot ID from an oFono modem object path.

**Parameters**:

- `path` Modem object path.

**Returns**:

Returns the corresponding slot ID (0 or 1), or a negative value for invalid paths.
