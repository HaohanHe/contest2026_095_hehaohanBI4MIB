\[ English | [简体中文](../../../zh-cn/api/framework/security.md) \]

# Security Framework API

The openvela security framework is based on MiTEE (Trusted Execution Environment) and provides secure storage, key management, and secure payment capabilities, following the GlobalPlatform (GP) TEE standard.

This document covers:

- MiTEE CA application-level API (SST secure storage, triad, WeChat/Alipay payment, PIN)
- MiTEE Rootkey API
- GP TEE Client API (REE side, called by CA)
- GP TEE Internal API (TEE side, used by TA implementations)

Header files: Various CA headers under `frameworks/security/include/` (`comsst_ca_api.h` / `triad_ca_api.h`, etc.), as well as `<sys/boardctl.h>` (Rootkey), OP-TEE `<tee_client_api.h>` (TEE Client), and `<tee_internal_api.h>` (TEE Internal).

## openvela Implementation Notes

- **Dual security schemes**: openvela provides two optional security capabilities:
    - **MiTEE-based TEE scheme** (main content of this document): For scenarios requiring hardware-level trusted execution environments
    - **Android Keystore scheme** (see "Android Keystore Client API" section): Ported from AOSP, for lightweight scenarios requiring only key management and secure storage
- **Based on MiTEE**: openvela's TEE implementation, compatible with OP-TEE, following GlobalPlatform (GP) specifications
- **Two-side runtime**:
    - **REE side** (Rich Execution Environment): Runs CA (Client Application), initiates TEE requests through `libteec`
    - **TEE side** (Trusted Execution Environment): Runs TA (Trusted Application), scheduled by the `miteed` server
- **Communication channel**: Data exchange between CA and TA is performed via rpmsg socket
- **API layers**:
    - Application-level CA API (located in `frameworks/security/ca/`) encapsulates common security operations (SST, triad, payment, PIN)
    - GP TEE Client API (REE side) provides GP-standard Context/Session management and command invocation
    - GP TEE Internal API (TEE side) provides memory/object/cryptographic capabilities callable by TA implementors
- **Key root**: Rootkey is written once during factory provisioning, immutable at runtime; all derived keys are derived from Rootkey

## Common Secure Storage SST CA API

Performs read/write operations on the Secure Storage (SST) partition. Implementation located in `frameworks/security/ca/comsst`.

### comsst_data_read

```c
uint32_t comsst_data_read(uint8_t *scope, uint8_t *name, bool is_deletable,
                          uint8_t *buff, uint32_t *out_len);
```

Reads a data record from the SST partition.

**Parameters**:

- `scope` Namespace (scope identifier) used to distinguish different services.
- `name` Record name.
- `is_deletable` Whether this record is allowed to be deleted by the user side.
- `buff` Output buffer to receive the read data.
- `out_len` Input/output parameter; input is the buffer size, output is the actual bytes read.

**Returns**:

Returns `TEE_SUCCESS` (`0`) on success, or a TEE error code on failure.

### comsst_data_write

```c
uint32_t comsst_data_write(uint8_t *scope, uint8_t *name, bool is_deletable,
                           uint8_t *buff, uint32_t len);
```

Writes a data record to the SST partition.

**Parameters**:

- `scope` Namespace.
- `name` Record name.
- `is_deletable` Whether this record is allowed to be deleted by the user side.
- `buff` Data buffer to write.
- `len` Data length.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### comsst_data_delete

```c
uint32_t comsst_data_delete(uint8_t *scope, uint8_t *name, bool is_deletable);
```

Deletes a record from the SST partition.

**Parameters**:

- `scope` Namespace.
- `name` Record name.
- `is_deletable` The deletable attribute of the record, must match the value used during write.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### is_comsst_data_exited

```c
uint32_t is_comsst_data_exited(uint8_t *scope, uint8_t *name, bool is_deletable);
```

Queries whether a specified record exists in the SST partition.

**Parameters**:

- `scope` Namespace.
- `name` Record name.
- `is_deletable` Deletable attribute.

**Returns**:

Returns `TEE_SUCCESS` if the record exists, or a corresponding error code if it does not.

### comsst_data_verify

```c
uint32_t comsst_data_verify(uint8_t *scope, uint8_t *name, bool is_deletable,
                            uint8_t *buff, uint32_t len);
```

Compares the provided data against the data already stored in SST. Commonly used to verify whether the application-side copy is up to date.

**Parameters**:

- `scope` Namespace.
- `name` Record name.
- `is_deletable` Deletable attribute.
- `buff` Data to compare.
- `len` Data length.

**Returns**:

Returns `TEE_SUCCESS` if the data matches, or an error code if it does not match or the operation fails.

## Triad CA API

Performs read/write operations on the device triad's Device ID (DID) and Key. Implementation located in `frameworks/security/ca/triad`.

### triad_store_did

```c
int triad_store_did(uint8_t *did, uint16_t len);
```

Writes the device DID to secure storage.

**Parameters**:

- `did` DID buffer.
- `len` DID length in bytes.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### triad_load_did

```c
int triad_load_did(uint8_t *did, uint16_t len);
```

Reads the DID from secure storage.

**Parameters**:

- `did` Output buffer to receive the DID.
- `len` Buffer length.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### triad_store_key

```c
int triad_store_key(uint8_t *key, uint16_t len);
```

Writes the device key to secure storage.

**Parameters**:

- `key` Key buffer.
- `len` Key length.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### triad_load_key

```c
int triad_load_key(uint8_t *key, uint16_t len);
```

Reads the device key from secure storage.

**Parameters**:

- `key` Output buffer.
- `len` Buffer length.

**Returns**:

Returns `0` on success, or a negative error code on failure.

### triad_get_hmac

```c
int triad_get_hmac(uint8_t *input, uint16_t inlen,
                   uint8_t *output, uint16_t outlen);
```

Computes an HMAC over the input data using the device key, writing the result to the output buffer.

**Parameters**:

- `input` Input data.
- `inlen` Input data length.
- `output` Output buffer to receive the HMAC result.
- `outlen` Output buffer length.

**Returns**:

Returns `0` on success, or a negative error code on failure.

## WeChat Pay CA API

Performs read/write operations on WeChat secure payment data. Implementation located in `frameworks/security/ca/wxcodepay`.

### wxcodepay_tee_data_read

```c
uint32_t wxcodepay_tee_data_read(int item, uint8_t *buff, uint32_t *out_len);
```

Reads a WeChat Pay data item.

**Parameters**:

- `item` Data item ID.
- `buff` Output buffer.
- `out_len` Input/output parameter, returns the actual bytes read.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### wxcodepay_tee_data_write

```c
uint32_t wxcodepay_tee_data_write(int item, const uint8_t *buf, uint32_t len);
```

Writes a WeChat Pay data item.

**Parameters**:

- `item` Data item ID.
- `buf` Data buffer.
- `len` Data length.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### wxcodepay_tee_data_delete

```c
uint32_t wxcodepay_tee_data_delete(int item);
```

Deletes the specified WeChat Pay data item.

**Parameters**:

- `item` Data item ID.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### is_wxcodepay_tee_data_exited

```c
bool is_wxcodepay_tee_data_exited(int item);
```

Queries whether the specified WeChat Pay data item is stored.

**Parameters**:

- `item` Data item ID.

**Returns**:

Returns `true` if the item exists, `false` otherwise.

## Alipay CA API

Performs read/write operations on Alipay secure payment data. Implementation located in `frameworks/security/ca/alipay`.

### alipay_tee_data_read

```c
uint32_t alipay_tee_data_read(const char *item_name, uint8_t *buff,
                              uint32_t *out_len);
```

Reads an Alipay data item.

**Parameters**:

- `item_name` Data item name string.
- `buff` Output buffer.
- `out_len` Input/output parameter, returns the actual bytes read.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### alipay_tee_data_write

```c
uint32_t alipay_tee_data_write(const char *item_name, const uint8_t *buf,
                               uint32_t len);
```

Writes an Alipay data item.

**Parameters**:

- `item_name` Data item name string.
- `buf` Data buffer.
- `len` Data length.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### alipay_tee_data_delete

```c
uint32_t alipay_tee_data_delete(const char *item_name);
```

Deletes the specified Alipay data item.

**Parameters**:

- `item_name` Data item name.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### is_alipay_tee_data_exited

```c
bool is_alipay_tee_data_exited(const char *item_name);
```

Queries whether the specified Alipay data item is stored.

**Parameters**:

- `item_name` Data item name.

**Returns**:

Returns `true` if the item exists, `false` otherwise.

## PIN CA API

Performs storage, verification, and modification operations on Personal Identification Numbers (PINs). Implementation located in `frameworks/security/ca/pin`.

### pin_store

```c
uint32_t pin_store(bool is_deletable, uint8_t *buff, uint32_t len);
```

Stores a PIN in secure storage.

**Parameters**:

- `is_deletable` Whether the PIN is allowed to be deleted by the user side.
- `buff` PIN data buffer.
- `len` PIN data length.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### pin_is_exist

```c
bool pin_is_exist(bool is_deletable);
```

Queries whether a PIN of the specified type is stored.

**Parameters**:

- `is_deletable` Deletable attribute, used to distinguish different types of PINs.

**Returns**:

Returns `true` if the PIN exists, `false` otherwise.

### pin_delete

```c
uint32_t pin_delete(bool is_deletable);
```

Deletes a PIN from secure storage.

**Parameters**:

- `is_deletable` Deletable attribute.

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

### pin_verify

```c
uint32_t pin_verify(bool is_deletable, uint8_t *buff, uint32_t len);
```

Verifies whether the PIN matches the record in secure storage.

**Parameters**:

- `is_deletable` Deletable attribute.
- `buff` PIN data to verify.
- `len` PIN data length.

**Returns**:

Returns `TEE_SUCCESS` if verification passes, or an error code if it fails.

### pin_change

```c
uint32_t pin_change(bool is_deletable, uint8_t *old, uint32_t oldlen,
                    uint8_t *new, uint32_t newlen);
```

Changes a stored PIN. Both the old PIN and new PIN must be provided.

**Parameters**:

- `is_deletable` Deletable attribute.
- `old` Old PIN buffer.
- `oldlen` Old PIN length.
- `new` New PIN buffer.
- `newlen` New PIN length.

**Returns**:

Returns `TEE_SUCCESS` on success (old PIN verified and new PIN written), or an error code on failure.

### pin_getsha256

```c
uint32_t pin_getsha256(bool is_deletable, uint8_t *buff, uint32_t len);
```

Retrieves the SHA-256 digest of the PIN.

**Parameters**:

- `is_deletable` Deletable attribute.
- `buff` Output buffer to receive the 32-byte digest.
- `len` Buffer length (must be at least 32).

**Returns**:

Returns `TEE_SUCCESS` on success, or a TEE error code on failure.

## MiTEE Rootkey Management

Rootkey is the **root of trust key** in the TEE security system, used to derive other keys. This key is written once during factory provisioning and read by the TEE OS at runtime.

### boardctl_BOARDIOC_UNIQUEKEY

```c
#include <sys/boardctl.h>

boardctl(BOARDIOC_UNIQUEKEY, tmp_key);
```

The TEE Server (`miteed`) in the TEE OS reads the Rootkey through the `boardctl` system call.

**Parameters**:

- `BOARDIOC_UNIQUEKEY` Fixed command identifier.
- `tmp_key` Output buffer pointer to receive the Rootkey content.

**Returns**:

Returns `0` on success, or a negative value on failure and sets `errno`.

**Notes**:

- Only the TEE OS side is allowed to call this interface; REE-side applications cannot access the Rootkey.

### rootkey_provision

```c
norflash_api_security_register_erase(HAL_FLASH_ID_0, 2048, 32);
norflash_api_security_register_write(HAL_FLASH_ID_0, 2048, rn, 32);
norflash_api_security_register_lock(HAL_FLASH_ID_0, 2048, 32);
```

Rootkey is written only in the factory build during the TEE OS first boot. The typical flow is "erase, write, lock":

**Parameters** (using `norflash_api_security_register_*` as example):

- `HAL_FLASH_ID_0` Flash device identifier.
- `2048` Security register start offset.
- `32` Byte length (256 bits).
- `rn` Rootkey data buffer provided during write.

**Notes**:

- Once `_lock` is executed, the region is permanently locked and cannot be written again. The correctness of the written data must be ensured.
- Production devices should never contain code that calls the above interfaces.

## Android Keystore Client API

**This section describes a Keystore that is independent of MiTEE**. The source code is ported from the Android Keystore service framework and follows the Keystore/Keymaster standard interfaces. The Keymaster layer supports multiple implementation backends, including MiTEE integration, pure software implementation, and implementations customized for Secure Elements (SE).

The openvela Keystore exposes a C API to upper layers, providing key management and secure storage capabilities for scenarios such as account SDKs. Users do not need to be aware of underlying hardware differences or storage details.

Header file: `#include <keystore/client.h>`

Source path: `external/android/system/security/keystore/`

### Keystore Usage Conventions

- **Storage unit**: Each data item is uniquely identified by a `name` string
- **Naming rules**: `name` length is limited to `CONFIG_NAME_MAX - 12`; if it contains special characters (ASCII `0` to `~` range), each character counts as 2 bytes
- **Return value convention**: All interfaces return `KEYSTORE_NO_ERROR` (value `1`) on success, and an error code greater than `1` on failure
- **Memory management**: Data returned by `keyStoreGet` is allocated internally; the caller must release it with `free()`

### keyStoreInsert

```c
int keyStoreInsert(const char *name, size_t nameLength,
                   const uint8_t *item, size_t itemLength);
```

Writes a data item to the Keystore. The data is encrypted internally within the Keystore.

**Parameters**:

- `name` Data item name. Must be unique, subject to the `CONFIG_NAME_MAX - 12` length limit.
- `nameLength` Name length in bytes.
- `item` Data buffer to write.
- `itemLength` Data length in bytes.

**Returns**:

Returns `KEYSTORE_NO_ERROR` on success, or another `KEYSTORE_*` error code on failure.

### keyStoreGet

```c
int keyStoreGet(const char *name, size_t nameLength,
                uint8_t **item, size_t *itemLength);
```

Reads a data item from the Keystore by name. The data is allocated internally; the caller must release it with `free()`.

**Parameters**:

- `name` Data item name.
- `nameLength` Name length.
- `item` Output parameter, returns a pointer to the internally allocated data buffer.
- `itemLength` Output parameter, returns the data length.

**Returns**:

Returns `KEYSTORE_NO_ERROR` on success, or another `KEYSTORE_*` error code on failure.

### keyStoreDel

```c
int keyStoreDel(const char *name, size_t nameLength);
```

Deletes a data item from the Keystore by name.

**Parameters**:

- `name` Data item name.
- `nameLength` Name length.

**Returns**:

Returns `KEYSTORE_NO_ERROR` on success, or another `KEYSTORE_*` error code on failure.

### keyStoreExist

```c
int keyStoreExist(const char *name, size_t nameLength);
```

Checks whether a data item with the specified name exists in the Keystore.

**Parameters**:

- `name` Data item name.
- `nameLength` Name length.

**Returns**:

Returns `KEYSTORE_NO_ERROR` if the item exists, or another `KEYSTORE_*` error code if it does not exist or the operation fails.

### keyStoreReset

```c
int keyStoreReset(void);
```

Deletes **all** data items belonging to the current application in the Keystore.

**Returns**:

Returns `KEYSTORE_NO_ERROR` on success, or another `KEYSTORE_*` error code on failure.

**Notes**:

- This operation is irreversible and only affects the current application's namespace.

### Keystore Error Codes

Error codes returned by all Keystore interfaces (defined in header `keystore/client.h`):

| Error Code                                                | Value | Description                                   |
| --------------------------------------------------------- | ----- | --------------------------------------------- |
| `KEYSTORE_NO_ERROR`                                       | 1     | Operation successful                          |
| `KEYSTORE_LOCKED`                                         | 2     | Keystore is locked                            |
| `KEYSTORE_UNINITIALIZED`                                  | 3     | Not initialized                               |
| `KEYSTORE_SYSTEM_ERROR`                                   | 4     | System error                                  |
| `KEYSTORE_PROTOCOL_ERROR`                                 | 5     | Protocol error                                |
| `KEYSTORE_PERMISSION_DENIED`                              | 6     | Permission denied                             |
| `KEYSTORE_KEY_NOT_FOUND`                                  | 7     | Specified data item does not exist            |
| `KEYSTORE_VALUE_CORRUPTED`                                | 8     | Data corrupted                                |
| `KEYSTORE_UNDEFINED_ACTION`                               | 9     | Undefined action                              |
| `KEYSTORE_WRONG_PASSWORD_0` ~ `KEYSTORE_WRONG_PASSWORD_3` | 10-13 | Wrong password (up to 4 retries)              |
| `KEYSTORE_SIGNATURE_INVALID`                              | 14    | Invalid signature                             |
| `KEYSTORE_OP_AUTH_NEEDED`                                 | 15    | Authentication required before this operation |
| `KEYSTORE_KEY_ALREADY_EXISTS`                             | 16    | Data item already exists                      |
| `KEYSTORE_KEY_PERMANENTLY_INVALIDATED`                    | 17    | Data item permanently invalidated             |
| `KEYSTORE_ABORT_CALLED`                                   | 18    | Operation aborted                             |
| `KEYSTORE_PRUNED`                                         | 19    | Data pruned                                   |
| `KEYSTORE_BINDER_DIED`                                    | 20    | Binder connection lost                        |

## GP TEE Client API (REE Side)

The following interfaces comply with the GlobalPlatform TEE Client API specification. They are called by CAs on the REE side to establish contexts with the TEE, open sessions, and execute commands.

### TEEC_InitializeContext

```c
TEEC_Result TEEC_InitializeContext(const char *name, TEEC_Context *context);
```

Initializes a TEE context, establishing a connection between the CA and the specified TEE.

**Parameters**:

- `name` Null-terminated string identifying the TEE to connect to. The current implementation only supports `NULL`, indicating the default TEE.
- `context` Pointer to the context structure to initialize.

**Returns**:

Returns `TEEC_SUCCESS` on success, or another `TEEC_Result` error code on failure.

### TEEC_FinalizeContext

```c
void TEEC_FinalizeContext(TEEC_Context *context);
```

Destroys an initialized TEE context, closing the connection between the CA and the TEE.

**Parameters**:

- `context` The context to destroy.

**Notes**:

- All associated sessions must be closed and all shared memory must be released before calling this function.

### TEEC_OpenSession

```c
TEEC_Result TEEC_OpenSession(TEEC_Context *context,
                             TEEC_Session *session,
                             const TEEC_UUID *destination,
                             uint32_t connectionMethod,
                             const void *connectionData,
                             TEEC_Operation *operation,
                             uint32_t *returnOrigin);
```

Opens a new session between the CA and the specified TA.

**Parameters**:

- `context` An initialized TEE context.
- `session` Pointer to the session structure to initialize.
- `destination` UUID of the target TA.
- `connectionMethod` Connection method.
- `connectionData` Connection-related data (currently unused, should pass `NULL`).
- `operation` Operation parameter structure; pass `NULL` if no parameters are needed.
- `returnOrigin` Output parameter, returns the origin of the error when a failure occurs.

**Returns**:

Returns `TEEC_SUCCESS` on success, or another `TEEC_Result` error code on failure.

### TEEC_CloseSession

```c
void TEEC_CloseSession(TEEC_Session *session);
```

Closes an opened TA session.

**Parameters**:

- `session` The session to close.

### TEEC_InvokeCommand

```c
TEEC_Result TEEC_InvokeCommand(TEEC_Session *session,
                               uint32_t commandID,
                               TEEC_Operation *operation,
                               uint32_t *returnOrigin);
```

Invokes a TA command within the specified session.

**Parameters**:

- `session` An opened session handle.
- `commandID` The command ID internal to the TA.
- `operation` Operation parameter structure; pass `NULL` if no parameters are needed.
- `returnOrigin` Output parameter, returns the origin of the error when a failure occurs.

**Returns**:

Returns `TEEC_SUCCESS` on success, or another `TEEC_Result` error code on failure.

### TEEC_AllocateSharedMemory

```c
TEEC_Result TEEC_AllocateSharedMemory(TEEC_Context *context,
                                      TEEC_SharedMemory *sharedMem);
```

Allocates a block of shared memory within the specified TEE context.

**Parameters**:

- `context` An initialized TEE context.
- `sharedMem` Pointer to the shared memory structure to allocate.

**Returns**:

Returns `TEEC_SUCCESS` on success; returns `TEEC_ERROR_OUT_OF_MEMORY` if memory is insufficient; returns other `TEEC_Result` error codes for other failures.

### TEEC_RegisterSharedMemory

```c
TEEC_Result TEEC_RegisterSharedMemory(TEEC_Context *context,
                                      TEEC_SharedMemory *sharedMem);
```

Registers a **caller-allocated** memory block as shared memory. Unlike `TEEC_AllocateSharedMemory` (which allocates via the framework), this interface allows the CA to reuse an existing memory buffer as the TEE communication data area.

**Parameters**:

- `context` An initialized TEE context.
- `sharedMem` Pointer to the shared memory structure. The caller should pre-fill the `buffer`, `size`, and `flags` fields.

**Returns**:

Returns `TEEC_SUCCESS` on success, or a corresponding `TEEC_Result` error code on failure.

### TEEC_ReleaseSharedMemory

```c
void TEEC_ReleaseSharedMemory(TEEC_SharedMemory *sharedMemory);
```

Releases or deregisters a previously allocated shared memory block. For memory allocated by `TEEC_AllocateSharedMemory`, the memory is freed. For memory registered by `TEEC_RegisterSharedMemory`, only deregistration is performed (the caller's buffer is not freed).

**Parameters**:

- `sharedMemory` Pointer to the shared memory structure to release.

### TEEC_RequestCancellation

```c
void TEEC_RequestCancellation(TEEC_Operation *operation);
```

Requests cancellation of an in-progress `TEEC_OpenSession` or `TEEC_InvokeCommand` operation. After calling this interface, the corresponding operation may be asynchronously aborted by the TEE.

**Parameters**:

- `operation` Pointer to the target `TEEC_Operation` structure. This must be the same object used by an in-progress `OpenSession` / `InvokeCommand`.

**Notes**:

- This interface is a "request" rather than a "force"; whether the operation is actually aborted depends on the TEE and the target TA.
- The TA needs to call `TEE_GetCancellationFlag` in the GP Internal API to respond to cancellation requests.

## GP TEE Internal API (TEE Side)

The GP TEE Internal Core API is a standard TA development interface defined by GlobalPlatform. **Function signatures, parameter semantics, and return value semantics are authoritative per the official GP specification.** openvela's MiTEE is compatible with these interfaces; however, due to current implementation progress, some interfaces are in an "Incomplete" state.

> **Authoritative References**:
> - [GlobalPlatform TEE Internal Core API Specification v1.3.1](https://globalplatform.org/specs-library/tee-internal-core-api-specification/)
> - `<tee_internal_api.h>` in the `optee_os` source tree

This section provides a **status lookup table** listing openvela's support status for each GP Internal API, helping TA developers determine which APIs can be used directly. For complete signatures, parameters, and return values, refer to the official GP specification above.

**Implementation Status column legend**:

- **Supported** — openvela has fully implemented the function; behavior is consistent with the GP specification
- **Incomplete** — The function symbol exists, but some behavior is not yet implemented or has not been fully validated; not recommended for production use

### TA Lifecycle Entry Points

| Function                     | Implementation Status | Description                    |
| ---------------------------- | --------------------- | ------------------------------ |
| `TA_CreateEntryPoint`        | Supported             | TA creation entry point        |
| `TA_DestroyEntryPoint`       | Supported             | TA destruction entry point     |
| `TA_OpenSessionEntryPoint`   | Supported             | Session open entry point       |
| `TA_CloseSessionEntryPoint`  | Supported             | Session close entry point      |
| `TA_InvokeCommandEntryPoint` | Supported             | Command invocation entry point |

### Inter-TA Communication

| Function              | Implementation Status | Description                    |
| --------------------- | --------------------- | ------------------------------ |
| `TEE_OpenTASession`   | Incomplete            | Open an inter-TA session       |
| `TEE_CloseTASession`  | Incomplete            | Close an inter-TA session      |
| `TEE_InvokeTACommand` | Incomplete            | Invoke a command on another TA |

### Memory Access Check

| Function                      | Implementation Status | Description                |
| ----------------------------- | --------------------- | -------------------------- |
| `TEE_CheckMemoryAccessRights` | Incomplete            | Check memory access rights |

### Memory Management

| Function         | Implementation Status | Description       |
| ---------------- | --------------------- | ----------------- |
| `TEE_Malloc`     | Supported             | Allocate memory   |
| `TEE_Realloc`    | Supported             | Reallocate memory |
| `TEE_Free`       | Supported             | Free memory       |
| `TEE_MemMove`    | Supported             | Move memory       |
| `TEE_MemCompare` | Supported             | Compare memory    |
| `TEE_MemFill`    | Supported             | Fill memory       |

### Generic Object Operations

| Function             | Implementation Status | Description            |
| -------------------- | --------------------- | ---------------------- |
| `TEE_GetObjectInfo1` | Supported             | Get object information |
| `TEE_CloseObject`    | Supported             | Close an object        |

### Transient Object Operations

| Function                      | Implementation Status | Description                          |
| ----------------------------- | --------------------- | ------------------------------------ |
| `TEE_AllocateTransientObject` | Supported             | Allocate a transient object          |
| `TEE_FreeTransientObject`     | Supported             | Free a transient object              |
| `TEE_ResetTransientObject`    | Supported             | Reset a transient object             |
| `TEE_PopulateTransientObject` | Supported             | Populate transient object attributes |
| `TEE_InitRefAttribute`        | Supported             | Initialize a reference attribute     |
| `TEE_InitValueAttribute`      | Supported             | Initialize a value attribute         |
| `TEE_CopyObjectAttributes1`   | Supported             | Copy object attributes               |
| `TEE_GenerateKey`             | Incomplete            | Generate a key                       |

### Persistent Object Operations

| Function                             | Implementation Status | Description                          |
| ------------------------------------ | --------------------- | ------------------------------------ |
| `TEE_OpenPersistentObject`           | Incomplete            | Open a persistent object             |
| `TEE_CreatePersistentObject`         | Incomplete            | Create a persistent object           |
| `TEE_CloseAndDeletePersistentObject` | Incomplete            | Close and delete a persistent object |
| `TEE_RenamePersistentObject`         | Incomplete            | Rename a persistent object           |

### Persistent Object Data Stream Operations

| Function                 | Implementation Status | Description             |
| ------------------------ | --------------------- | ----------------------- |
| `TEE_ReadObjectData`     | Incomplete            | Read object data        |
| `TEE_WriteObjectData`    | Incomplete            | Write object data       |
| `TEE_TruncateObjectData` | Incomplete            | Truncate object data    |
| `TEE_SeekObjectData`     | Incomplete            | Seek object data offset |

### Cryptographic Operation Management

| Function                       | Implementation Status | Description                         |
| ------------------------------ | --------------------- | ----------------------------------- |
| `TEE_AllocateOperation`        | Supported             | Allocate a cryptographic operation  |
| `TEE_FreeOperation`            | Supported             | Free a cryptographic operation      |
| `TEE_GetOperationInfo`         | Supported             | Get operation information           |
| `TEE_GetOperationInfoMultiple` | Supported             | Get multi-key operation information |
| `TEE_ResetOperation`           | Supported             | Reset an operation                  |
| `TEE_SetOperationKey`          | Supported             | Set operation key                   |
| `TEE_SetOperationKey2`         | Supported             | Set dual-key operation              |
| `TEE_CopyOperation`            | Supported             | Copy an operation                   |

### Message Digest

| Function            | Implementation Status | Description                 |
| ------------------- | --------------------- | --------------------------- |
| `TEE_DigestUpdate`  | Supported             | Update digest data          |
| `TEE_DigestDoFinal` | Supported             | Finalize digest computation |

### Symmetric Cipher

| Function            | Implementation Status | Description                           |
| ------------------- | --------------------- | ------------------------------------- |
| `TEE_CipherInit`    | Supported             | Initialize symmetric cipher operation |
| `TEE_CipherUpdate`  | Supported             | Update cipher data                    |
| `TEE_CipherDoFinal` | Supported             | Finalize cipher operation             |

### MAC (Message Authentication Code)

| Function              | Implementation Status | Description              |
| --------------------- | --------------------- | ------------------------ |
| `TEE_MACInit`         | Supported             | Initialize MAC operation |
| `TEE_MACUpdate`       | Supported             | Update MAC data          |
| `TEE_MACComputeFinal` | Supported             | Compute final MAC value  |
| `TEE_MACCompareFinal` | Supported             | Compare final MAC value  |

### Authenticated Encryption (AE)

| Function             | Implementation Status | Description                          |
| -------------------- | --------------------- | ------------------------------------ |
| `TEE_AEInit`         | Incomplete            | Initialize AE operation              |
| `TEE_AEUpdateAAD`    | Incomplete            | Update additional authenticated data |
| `TEE_AEUpdate`       | Incomplete            | Update AE data                       |
| `TEE_AEEncryptFinal` | Incomplete            | Finalize AE encryption               |
| `TEE_AEDecryptFinal` | Incomplete            | Finalize AE decryption               |

### Asymmetric Cryptography

| Function                     | Implementation Status | Description             |
| ---------------------------- | --------------------- | ----------------------- |
| `TEE_AsymmetricEncrypt`      | Incomplete            | Asymmetric encryption   |
| `TEE_AsymmetricDecrypt`      | Incomplete            | Asymmetric decryption   |
| `TEE_AsymmetricSignDigest`   | Incomplete            | Asymmetric signature    |
| `TEE_AsymmetricVerifyDigest` | Incomplete            | Asymmetric verification |

### Key Derivation

| Function        | Implementation Status | Description  |
| --------------- | --------------------- | ------------ |
| `TEE_DeriveKey` | Incomplete            | Derive a key |

### Random Number Generation

| Function             | Implementation Status | Description          |
| -------------------- | --------------------- | -------------------- |
| `TEE_GenerateRandom` | Incomplete            | Generate random data |

### Time API

| Function                  | Implementation Status | Description            |
| ------------------------- | --------------------- | ---------------------- |
| `TEE_GetSystemTime`       | Incomplete            | Get system time        |
| `TEE_GetTAPersistentTime` | Incomplete            | Get TA persistent time |
| `TEE_SetTAPersistentTime` | Incomplete            | Set TA persistent time |
| `TEE_GetREETime`          | Incomplete            | Get REE time           |
