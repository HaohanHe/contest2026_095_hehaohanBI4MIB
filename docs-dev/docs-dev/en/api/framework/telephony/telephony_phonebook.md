\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_phonebook.md) \]

# Telephony Phonebook API

SIM card phonebook management interfaces, supporting ADN (Abbreviated Dialling Numbers) and FDN (Fixed Dialling Numbers) entries.

Header: `#include <tapi_phonebook.h>`

## openvela Implementation Notes

- **ADN**: Abbreviated Dialling Numbers, regular numbers stored on the SIM card
- **FDN**: Fixed Dialling Numbers; when enabled, the phone can only dial numbers in the FDN list, protected by PIN2
- **FDN operations require PIN2**: `insert_fdn_entry` / `delete_fdn_entry` / `update_fdn_entry` calls require PIN2
- **SIM identification**: All interfaces include a `slot_id` parameter
- **Asynchronous callbacks**: All operations return results asynchronously via `tapi_async_function`

## ADN Phonebook

### tapi_phonebook_load_adn_entries

```c
int tapi_phonebook_load_adn_entries(tapi_context context, int slot_id, int event_id,
                                    tapi_async_function p_handle);
```

Load ADN phonebook entries from the SIM card.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID for callback matching.
- `p_handle` Asynchronous callback function, returns the ADN entry list on callback.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## FDN Fixed Dialling

### tapi_phonebook_load_fdn_entries

```c
int tapi_phonebook_load_fdn_entries(tapi_context context, int slot_id, int event_id,
                                    tapi_async_function p_handle);
```

Load FDN entries from the SIM card.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `p_handle` Asynchronous callback function, returns the FDN entry list on callback.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_phonebook_insert_fdn_entry

```c
int tapi_phonebook_insert_fdn_entry(tapi_context context, int slot_id, int event_id,
                                    char* name, char* number, char* pin2,
                                    tapi_async_function p_handle);
```

Insert a new entry into the FDN list (requires PIN2 verification).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `name` Contact name.
- `number` Phone number.
- `pin2` SIM card PIN2 code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_phonebook_update_fdn_entry

```c
int tapi_phonebook_update_fdn_entry(tapi_context context, int slot_id, int event_id,
                                    int fdn_idx, char* new_name, char* new_number,
                                    char* pin2, tapi_async_function p_handle);
```

Update an existing FDN entry (requires PIN2 verification).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `fdn_idx` Index of the entry to update.
- `new_name` New contact name.
- `new_number` New phone number.
- `pin2` SIM card PIN2 code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_phonebook_delete_fdn_entry

```c
int tapi_phonebook_delete_fdn_entry(tapi_context context, int slot_id, int event_id,
                                    int fdn_idx, char* pin2,
                                    tapi_async_function p_handle);
```

Delete a specified FDN entry (requires PIN2 verification).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `event_id` Event ID.
- `fdn_idx` Index of the entry to delete.
- `pin2` SIM card PIN2 code.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.
