\[ English | [简体中文](../../../zh-cn/api/network/net_ftp.md) \]

# FTP Server API

A simple FTP server interface providing user management and session handling capabilities.

Header file: `#include <netutils/ftpd.h>`

## openvela Implementation Notes

- **Use cases**: IoT device debugging, firmware upload, file download, and other lightweight FTP applications
- **Configuration dependency**: Requires enabling `CONFIG_NETUTILS_FTPD`
- **User management**: Users and permissions are added via `ftpd_adduser`
- **Session model**: `ftpd_session` provides session handling for a single client connection, typically called in a dedicated thread

## FTP Server

Header file: `#include <netutils/ftpd.h>`

### ftpd_open

```c
FTPD_SESSION ftpd_open(int port, sa_family_t family);
```

Creates an FTP server session.

**Parameters**:

- `port` Listening port (usually 21).
- `family` Address family (`AF_INET` or `AF_INET6`).

**Returns**:

Returns a session handle on success.

### ftpd_adduser

```c
int ftpd_adduser(FTPD_SESSION handle, uint8_t accountflags,
                 const char *user, const char *passwd, const char *home);
```

Adds an FTP user.

**Parameters**:

- `handle` Handle returned by `ftpd_open()`.
- `accountflags` User attribute flags (see `FTPD_ACCOUNTFLAGS_*`).
- `user` Username (`NULL` means no login required).
- `passwd` Password (`NULL` means no password required).
- `home` User home directory.

### ftpd_session

```c
int ftpd_session(FTPD_SESSION handle, int timeout);
```

Runs an FTP server session, waiting for and handling a single client connection.

**Parameters**:

- `handle` Session handle.
- `timeout` Timeout for waiting for a connection (milliseconds), 0 means wait indefinitely.

### ftpd_close

```c
void ftpd_close(FTPD_SESSION handle);
```

Closes the FTP server session.
