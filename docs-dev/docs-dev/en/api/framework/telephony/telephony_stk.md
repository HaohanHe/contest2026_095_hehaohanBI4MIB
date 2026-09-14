\[ English | [简体中文](../../../../zh-cn/api/framework/telephony/telephony_stk.md) \]

# Telephony SIM Toolkit (STK) API

SIM Application Toolkit (STK / CAT) provides carrier-provisioned interactive menus and event handling capabilities on the SIM card. Common use cases include carrier value-added menus, service password management, and URL browser launching.

Header file: `#include <tapi_stk.h>`

## openvela Implementation Notes

- **Agent mode**: The application registers as an "STK Agent" with TAPI. Display/input/confirmation requests initiated proactively by the SIM card are delivered to the application through Agent callbacks.
- **Registration levels**: Supports per-slot Agent (via `tapi_stk_agent_register`) and default Agent (system default UI).
- **Main menu**: `tapi_stk_get_main_menu*` queries the main menu structure provided by the SIM card.
- **Proactive Command responses**: The `tapi_stk_handle_agent_*` family of interfaces is used to send the Agent's responses to SIM card proactive commands back to the SIM.
- **SIM identification**: All interfaces include a `slot_id` parameter.

## Agent Registration

### tapi_stk_agent_register

```c
int tapi_stk_agent_register(tapi_context context, int slot_id,
                            char* agent_id, tapi_async_function p_handle);
```

Registers an STK Agent for the specified SIM card slot.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `agent_id` Agent identifier string.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_agent_unregister

```c
int tapi_stk_agent_unregister(tapi_context context, int slot_id,
                              char* agent_id, tapi_async_function p_handle);
```

Unregisters an STK Agent.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `agent_id` Agent identifier string.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_default_agent_register

```c
int tapi_stk_default_agent_register(tapi_context context, int slot_id,
                                    char* agent_id, tapi_async_function p_handle);
```

Registers as the default STK Agent (global fallback).

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `agent_id` Agent identifier string.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_default_agent_unregister

```c
int tapi_stk_default_agent_unregister(tapi_context context, int slot_id,
                                      tapi_async_function p_handle);
```

Unregisters the default STK Agent.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_agent_interface_register

```c
int tapi_stk_agent_interface_register(tapi_context context, int slot_id, char* agent_id,
                                      tapi_stk_agent_interface* iface);
```

Registers the concrete interface implementation (callback function set) at the Agent layer.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `agent_id` Agent identifier string.
- `iface` Pointer to the Agent interface callback structure.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_agent_interface_unregister

```c
int tapi_stk_agent_interface_unregister(tapi_context context, char* agent_id);
```

Unregisters the Agent interface implementation.

**Parameters**:

- `context` Telephony context handle.
- `agent_id` Agent identifier string.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_default_agent_interface_register

```c
int tapi_stk_default_agent_interface_register(tapi_context context, int slot_id,
                                              tapi_stk_agent_interface* iface);
```

Registers the interface implementation for the default Agent.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `iface` Pointer to the Agent interface callback structure.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_default_agent_interface_unregister

```c
int tapi_stk_default_agent_interface_unregister(tapi_context context, int slot_id);
```

Unregisters the interface implementation of the default Agent.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Main Menu and Idle Mode

### tapi_stk_select_item

```c
int tapi_stk_select_item(tapi_context context, int slot_id,
                         int item_idx, tapi_async_function p_handle);
```

Selects an item from the main menu, triggering the SIM card's service response.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `item_idx` Item index.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_get_idle_mode_text

```c
int tapi_stk_get_idle_mode_text(tapi_context context, int slot_id, char** text);
```

Queries the idle mode display text set by the SIM card.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `text` Output parameter, returns the text string.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_get_idle_mode_icon

```c
int tapi_stk_get_idle_mode_icon(tapi_context context, int slot_id, char** icon);
```

Queries the idle mode icon identifier set by the SIM card.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `icon` Output parameter, returns the icon identifier string.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_get_main_menu

```c
int tapi_stk_get_main_menu(tapi_context context, int slot_id, int* length,
                           tapi_stk_menu_item out[]);
```

Retrieves the list of main menu items provided by the SIM card.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `length` Input/output parameter: on input specifies buffer capacity, on output returns the actual number of items.
- `out` Output buffer to receive the menu item array.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_get_main_menu_title

```c
int tapi_stk_get_main_menu_title(tapi_context context, int slot_id, char** title);
```

Queries the main menu title.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `title` Output parameter, returns the title string.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### tapi_stk_get_main_menu_icon

```c
int tapi_stk_get_main_menu_icon(tapi_context context, int slot_id, int* icon);
```

Queries the main menu icon number.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `icon` Output parameter, returns the icon number.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## Agent Response Handling

The following interfaces are used by Agent implementations to send proactive command responses back to the SIM card. All interfaces return `0` on success, or a negative error code on failure.

### tapi_stk_handle_agent_request_selection

```c
int tapi_stk_handle_agent_request_selection(tapi_context context, int slot_id,
                                            char* agent_id, int selection,
                                            tapi_async_function p_handle);
```

Handles the SIM card's menu item selection request by sending back the index of the item selected by the user.

**Parameters**:

- `context` Telephony context handle.
- `slot_id` SIM card slot ID.
- `agent_id` Agent identifier string.
- `selection` Index of the item selected by the user.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_display_text

```c
int tapi_stk_handle_agent_display_text(tapi_context context, int slot_id,
                                       char* agent_id, int result,
                                       tapi_async_function p_handle);
```

Responds to the SIM card's text display request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `result` Display operation result (whether the user confirmed, etc.).
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_request_input

```c
int tapi_stk_handle_agent_request_input(tapi_context context, int slot_id,
                                        char* agent_id, char* input,
                                        tapi_async_function p_handle);
```

Responds to the SIM card's string input request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `input` String entered by the user.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_request_digits

```c
int tapi_stk_handle_agent_request_digits(tapi_context context, int slot_id,
                                         char* agent_id, char* digits,
                                         tapi_async_function p_handle);
```

Responds to the SIM card's digit sequence input request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `digits` Digit sequence entered by the user.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_request_key

```c
int tapi_stk_handle_agent_request_key(tapi_context context, int slot_id,
                                      char* agent_id, char key,
                                      tapi_async_function p_handle);
```

Responds to the SIM card's single key input request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `key` Key character entered by the user.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_request_digit

```c
int tapi_stk_handle_agent_request_digit(tapi_context context, int slot_id,
                                        char* agent_id, char digit,
                                        tapi_async_function p_handle);
```

Responds to the SIM card's single digit input request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `digit` Single digit character entered by the user.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_request_quick_digit

```c
int tapi_stk_handle_agent_request_quick_digit(tapi_context context, int slot_id,
                                              char* agent_id, char digit,
                                              tapi_async_function p_handle);
```

Responds to the SIM card's quick digit input request (no echo required).

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `digit` Single digit character entered by the user.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_request_confirmation

```c
int tapi_stk_handle_agent_request_confirmation(tapi_context context, int slot_id,
                                               char* agent_id, bool confirmed,
                                               tapi_async_function p_handle);
```

Responds to the SIM card's confirmation/cancellation request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `confirmed` Whether the user confirmed.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_confirm_call_setup

```c
int tapi_stk_handle_agent_confirm_call_setup(tapi_context context, int slot_id,
                                             char* agent_id, bool confirmed,
                                             tapi_async_function p_handle);
```

Responds to the SIM card's call-setup confirmation request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `confirmed` Whether the user confirmed the outgoing call.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_play_tone

```c
int tapi_stk_handle_agent_play_tone(tapi_context context, int slot_id,
                                    char* agent_id, int result,
                                    tapi_async_function p_handle);
```

Responds to the SIM card's play tone request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `result` Playback result.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_loop_tone

```c
int tapi_stk_handle_agent_loop_tone(tapi_context context, int slot_id,
                                    char* agent_id, int result,
                                    tapi_async_function p_handle);
```

Responds to the SIM card's loop tone request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `result` Playback result.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_display_action_information

```c
int tapi_stk_handle_agent_display_action_information(tapi_context context, int slot_id,
                                                     char* agent_id, int result,
                                                     tapi_async_function p_handle);
```

Responds to the SIM card's action progress information display request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `result` Display operation result.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_confirm_launch_browser

```c
int tapi_stk_handle_agent_confirm_launch_browser(tapi_context context, int slot_id,
                                                 char* agent_id, bool confirmed,
                                                 tapi_async_function p_handle);
```

Responds to the SIM card's browser launch confirmation request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `confirmed` Whether the user confirmed launching the browser.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_display_action

```c
int tapi_stk_handle_agent_display_action(tapi_context context, int slot_id,
                                         char* agent_id, int result,
                                         tapi_async_function p_handle);
```

Responds to the SIM card's action status update request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `result` Operation result.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.


### tapi_stk_handle_agent_confirm_open_channel

```c
int tapi_stk_handle_agent_confirm_open_channel(tapi_context context, int slot_id,
                                               char* agent_id, bool confirmed,
                                               tapi_async_function p_handle);
```

Responds to the SIM card's open data channel confirmation request.

**Parameters**:

- `context` / `slot_id` / `agent_id` Same as above.
- `confirmed` Whether the user confirmed opening the channel.
- `p_handle` Asynchronous callback function.

**Returns**:

Returns `0` on success, or a negative error code on failure.
