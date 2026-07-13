\[ English | [简体中文](../../../zh-cn/api/network/net.md) \]

# Network API

openvela provides BSD-compatible socket interfaces, supporting protocol families such as IPv4 (`AF_INET`), IPv6 (`AF_INET6`), as well as stream sockets (`SOCK_STREAM`), datagram sockets (`SOCK_DGRAM`), and raw sockets (`SOCK_RAW`).

Header files: `#include <sys/socket.h>`, `#include <netinet/in.h>`, `#include <arpa/inet.h>`

## openvela Implementation Notes

- **Protocol family support**: `AF_INET` (IPv4), `AF_INET6` (IPv6), `AF_LOCAL`/`AF_UNIX` (local sockets), `AF_PACKET` (raw link layer), etc., depending on network stack configuration
- **Configuration dependency**: The network subsystem is optional and requires enabling `CONFIG_NET` and related protocol configurations (e.g., `CONFIG_NET_TCP`, `CONFIG_NET_UDP`)
- **Non-blocking I/O**: Set via `fcntl(fd, F_SETFL, O_NONBLOCK)` or the `SOCK_NONBLOCK` flag
- **DNS resolution**: Requires enabling `CONFIG_NETDB_DNSCLIENT`, DNS servers are managed through the `nuttx/net/dns.h` interface

## Socket Creation and Management

### socket

```c
int socket(int domain, int type, int protocol);
```

Creates a communication endpoint (socket) and returns a file descriptor.

**Parameters**:

- `domain` Protocol family:
  - `AF_INET` IPv4
  - `AF_INET6` IPv6
  - `AF_LOCAL` / `AF_UNIX` Local socket
  - `AF_PACKET` Raw link layer
- `type` Socket type:
  - `SOCK_STREAM` Connection-oriented stream socket (TCP)
  - `SOCK_DGRAM` Connectionless datagram socket (UDP)
  - `SOCK_RAW` Raw socket
  - Can be bitwise OR'd with `SOCK_NONBLOCK`, `SOCK_CLOEXEC`
- `protocol` Protocol number, usually 0 (auto-select). Can also specify `IPPROTO_TCP`, `IPPROTO_UDP`, etc.

**Returns**:

Returns a non-negative file descriptor on success, or -1 on failure and sets `errno`:

- `EAFNOSUPPORT` Unsupported protocol family.
- `EPROTONOSUPPORT` Unsupported protocol type.
- `EMFILE` Process file descriptor limit reached.
- `ENOMEM` Insufficient memory.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### socketpair

```c
int socketpair(int domain, int type, int protocol, int sv[2]);
```

Creates a pair of connected sockets, commonly used for inter-process communication between parent and child processes.

**Parameters**:

- `domain` Protocol family, usually `AF_LOCAL`.
- `type` Socket type (`SOCK_STREAM` or `SOCK_DGRAM`).
- `protocol` Usually 0.
- `sv` Output parameter, stores two connected file descriptors.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno`:

- `EAFNOSUPPORT` Unsupported protocol family.
- `EMFILE` File descriptor limit reached.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### shutdown

```c
int shutdown(int sockfd, int how);
```

Shuts down part or all communication directions of a socket. Unlike `close()`, `shutdown()` can close only the read or write direction.

**Parameters**:

- `sockfd` Socket file descriptor.
- `how` Shutdown mode:
  - `SHUT_RD` Close the read direction.
  - `SHUT_WR` Close the write direction (sends FIN).
  - `SHUT_RDWR` Close both read and write directions.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `ENOTCONN` Socket is not connected.
- `EINVAL` Invalid `how` parameter.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

## Connection Management

### bind

```c
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

Binds a socket to a specified local address and port. The server side must call `bind()` before `listen()`.

**Parameters**:

- `sockfd` Socket file descriptor.
- `addr` Local address structure (`struct sockaddr_in` or `struct sockaddr_in6`).
- `addrlen` Size of the address structure.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `EINVAL` Socket is already bound, or address is invalid.
- `EADDRINUSE` Address already in use.
- `EADDRNOTAVAIL` Requested address is not available.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### connect

```c
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

Initiates a connection to a remote address. For TCP sockets, performs the three-way handshake; for UDP sockets, sets the default destination address.

**Parameters**:

- `sockfd` Socket file descriptor.
- `addr` Remote address structure.
- `addrlen` Size of the address structure.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `ECONNREFUSED` Remote host refused the connection.
- `ETIMEDOUT` Connection timed out.
- `ENETUNREACH` Network is unreachable.
- `EINPROGRESS` Connection is in progress in non-blocking mode.
- `EISCONN` Socket is already connected.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### listen

```c
int listen(int sockfd, int backlog);
```

Marks a socket as a passive listening socket, ready to accept connection requests.

**Parameters**:

- `sockfd` Bound socket file descriptor.
- `backlog` Maximum length of the pending connection queue.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `EOPNOTSUPP` Socket type does not support `listen()`.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### accept

```c
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

Extracts the first connection request from the listening socket's pending queue, creates and returns a new connected socket. Blocks if the queue is empty.

**Parameters**:

- `sockfd` Listening socket file descriptor.
- `addr` If non-`NULL`, stores the client address.
- `addrlen` On input, the size of the `addr` buffer; on output, the actual address size.

**Returns**:

Returns a new connected socket descriptor on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `EMFILE` File descriptor limit reached.
- `ECONNABORTED` Connection was aborted.
- `EINTR` Interrupted by a signal.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### accept4

```c
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
```

Same as `accept()`, but allows setting attributes on the new socket via `flags`.

**Parameters**:

- `sockfd` Listening socket file descriptor.
- `addr` Client address (can be `NULL`).
- `addrlen` Address length.
- `flags` Flags:
  - `SOCK_NONBLOCK` Set the new socket to non-blocking.
  - `SOCK_CLOEXEC` Set the new socket to close-on-exec.

**Returns**:

Same as `accept()`.

**POSIX Compatibility**: Compatible with Linux extension (non-POSIX).

## Data Sending

### send

```c
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

Sends data on a connected socket. Equivalent to `sendto()` without specifying a destination address.

**Parameters**:

- `sockfd` Connected socket file descriptor.
- `buf` Data buffer to send.
- `len` Data length (bytes).
- `flags` Send flags:
  - `MSG_DONTWAIT` Non-blocking send.
  - `MSG_NOSIGNAL` Do not generate `SIGPIPE` when the peer closes.
  - `0` Default behavior.

**Returns**:

Returns the number of bytes sent on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `ENOTCONN` Socket is not connected.
- `EAGAIN` / `EWOULDBLOCK` Send buffer is full in non-blocking mode.
- `EPIPE` Peer has closed the connection.
- `EINTR` Interrupted by a signal.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### sendto

```c
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *to, socklen_t tolen);
```

Sends data to a specified address. Primarily used for UDP sockets; can also be used on connected TCP sockets (destination address is ignored in that case).

**Parameters**:

- `sockfd` Socket file descriptor.
- `buf` Data buffer.
- `len` Data length.
- `flags` Send flags (same as `send()`).
- `to` Destination address. Can be `NULL` for connected sockets.
- `tolen` Destination address length.

**Returns**:

Returns the number of bytes sent on success, or -1 on failure and sets `errno` (same as `send()`, plus):

- `EDESTADDRREQ` Unconnected socket and no destination address specified.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### sendmsg

```c
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
```

Sends data via an `msghdr` structure, supporting scatter/gather I/O and ancillary data (e.g., file descriptor passing).

**Parameters**:

- `sockfd` Socket file descriptor.
- `msg` Message header structure containing destination address, I/O vectors, ancillary data, etc.
- `flags` Send flags.

**Returns**:

Returns the number of bytes sent on success, or -1 on failure and sets `errno`.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

## Data Receiving

### recv

```c
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

Receives data from a connected socket.

**Parameters**:

- `sockfd` Connected socket file descriptor.
- `buf` Receive buffer.
- `len` Buffer size (bytes).
- `flags` Receive flags:
  - `MSG_DONTWAIT` Non-blocking receive.
  - `MSG_PEEK` Peek at data without removing it.
  - `MSG_WAITALL` Wait to receive the full `len` bytes.
  - `0` Default behavior.

**Returns**:

Returns the number of bytes received on success (0 indicates the peer closed the connection), or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `ENOTCONN` Socket is not connected.
- `EAGAIN` / `EWOULDBLOCK` No data available in non-blocking mode.
- `EINTR` Interrupted by a signal.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### recvfrom

```c
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *from, socklen_t *fromlen);
```

Receives data and obtains the sender's address. Primarily used for UDP sockets.

**Parameters**:

- `sockfd` Socket file descriptor.
- `buf` Receive buffer.
- `len` Buffer size.
- `flags` Receive flags (same as `recv()`).
- `from` If non-`NULL`, stores the sender's address.
- `fromlen` On input, the size of the `from` buffer; on output, the actual address size.

**Returns**:

Returns the number of bytes received on success, or -1 on failure and sets `errno` (same as `recv()`).

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### recvmsg

```c
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
```

Receives data via an `msghdr` structure, supporting scatter/gather I/O and ancillary data.

**Parameters**:

- `sockfd` Socket file descriptor.
- `msg` Message header structure.
- `flags` Receive flags.

**Returns**:

Returns the number of bytes received on success, or -1 on failure and sets `errno`.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

## Socket Options

### setsockopt

```c
int setsockopt(int sockfd, int level, int option, const void *value, socklen_t value_len);
```

Sets socket options.

**Parameters**:

- `sockfd` Socket file descriptor.
- `level` Protocol layer of the option:
  - `SOL_SOCKET` Socket-level options.
  - `IPPROTO_TCP` TCP-level options.
  - `IPPROTO_IP` IP-level options.
  - `IPPROTO_IPV6` IPv6-level options.
- `option` Option name (e.g., `SO_REUSEADDR`, `SO_KEEPALIVE`, `TCP_NODELAY`, etc.).
- `value` Option value.
- `value_len` Size of the option value.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `ENOPROTOOPT` Unsupported option.
- `EINVAL` Invalid option value.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### getsockopt

```c
int getsockopt(int sockfd, int level, int option, void *value, socklen_t *value_len);
```

Gets socket options.

**Parameters**:

- `sockfd` Socket file descriptor.
- `level` Protocol layer (same as `setsockopt()`).
- `option` Option name.
- `value` Buffer to store the option value.
- `value_len` On input, the buffer size; on output, the actual value size.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno` (same as `setsockopt()`).

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

## Address Query

### getsockname

```c
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

Gets the local address bound to a socket.

**Parameters**:

- `sockfd` Socket file descriptor.
- `addr` Buffer to store the local address.
- `addrlen` On input, the buffer size; on output, the actual address size.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `EINVAL` Invalid `addrlen`.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

### getpeername

```c
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

Gets the remote address of a connected socket.

**Parameters**:

- `sockfd` Connected socket file descriptor.
- `addr` Buffer to store the remote address.
- `addrlen` On input, the buffer size; on output, the actual address size.

**Returns**:

Returns 0 on success, or -1 on failure and sets `errno`:

- `EBADF` Invalid file descriptor.
- `ENOTCONN` Socket is not connected.

**POSIX Compatibility**: Compatible with the POSIX/BSD interface of the same name.

## DNS Interface

Header file: `#include <nuttx/net/dns.h>`

### dns_add_nameserver

```c
int dns_add_nameserver(const struct sockaddr *addr, socklen_t addrlen);
```

Adds a DNS name server.

**Parameters**:

- `addr` DNS server address (`struct sockaddr_in` or `struct sockaddr_in6`).
- `addrlen` Size of the address structure.

**Returns**:

Returns 0 on success, or a negative error code on failure.

**POSIX Compatibility**: openvela/NuttX extension.

### dns_default_nameserver

```c
int dns_default_nameserver(void);
```

Resets the DNS resolver to use only the default DNS server.

**Returns**:

Returns 0 on success, or a negative error code on failure.

**POSIX Compatibility**: openvela/NuttX extension.

### dns_foreach_nameserver

```c
int dns_foreach_nameserver(dns_callback_t callback, void *arg);
```

Iterates over all configured DNS servers, calling the callback function for each server.

**Parameters**:

- `callback` Callback function, called for each DNS server.
- `arg` User argument passed to the callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.

**POSIX Compatibility**: openvela/NuttX extension.

### dns_register_notify

```c
int dns_register_notify(dns_callback_t callback, void *arg);
```

Registers a DNS server change notification. The callback function is called when the DNS server list changes.

**Parameters**:

- `callback` Change notification callback function.
- `arg` User argument passed to the callback function.

**Returns**:

Returns 0 on success, or a negative error code on failure.

**POSIX Compatibility**: openvela/NuttX extension.

### dns_unregister_notify

```c
int dns_unregister_notify(dns_callback_t callback, void *arg);
```

Unregisters a DNS server change notification.

**Parameters**:

- `callback` Previously registered callback function.
- `arg` User argument provided during registration.

**Returns**:

Returns 0 on success, or a negative error code on failure.

**POSIX Compatibility**: openvela/NuttX extension.

### dns_set_queryfamily

```c
int dns_set_queryfamily(sa_family_t family);
```

Sets the address family used for DNS queries.

**Parameters**:

- `family` Address family (`AF_INET`, `AF_INET6`, or `AF_UNSPEC`).

**Returns**:

Returns 0 on success, or a negative error code on failure.

**POSIX Compatibility**: openvela/NuttX extension.
