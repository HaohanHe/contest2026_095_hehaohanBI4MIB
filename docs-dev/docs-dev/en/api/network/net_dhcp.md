\[ English | [简体中文](../../../zh-cn/api/network/net_dhcp.md) \]

# DHCP API

DHCP (Dynamic Host Configuration Protocol) client and server interfaces, covering both IPv4 (`dhcpc_*` / `dhcpd_*`) and IPv6 (`dhcp6c_*`) address allocation protocols.

Header files: `#include <netutils/dhcpc.h>`, `#include <netutils/dhcp6c.h>`, `#include <netutils/dhcpd.h>`

## openvela Implementation Notes

- **IPv4 client**: The `dhcpc_*` series encapsulates the complete DHCP client state machine (DISCOVER/OFFER/REQUEST/ACK)
- **IPv6 client**: The `dhcp6c_*` series implements the DHCPv6 client protocol
- **Server**: The `dhcpd_*` series provides simple DHCP server capabilities for IP allocation in hotspot/AP mode
- **Asynchronous calls**: The `*_request_async` interfaces provide callback-based invocation to avoid blocking the current thread
- **Configuration dependency**: Requires enabling `CONFIG_NETUTILS_DHCPC` / `CONFIG_NETUTILS_DHCP6C` / `CONFIG_NETUTILS_DHCPD`

## DHCP Client

Header file: `#include <netutils/dhcpc.h>`

### dhcpc_open

```c
void *dhcpc_open(const char *interface, const void *mac_addr, int mac_len);
```

Creates a DHCP client session.

**Parameters**:

- `interface` Network interface name (e.g., `"eth0"`).
- `mac_addr` MAC address.
- `mac_len` MAC address length.

**Returns**:

Returns a session handle on success, or `NULL` on failure.

### dhcpc_request

```c
int dhcpc_request(void *handle, struct dhcpc_state *presult);
```

Performs DHCP negotiation to obtain an IP address (blocking call).

**Parameters**:

- `handle` Session handle returned by `dhcpc_open()`.
- `presult` Stores the obtained network configuration (IP, subnet mask, gateway, DNS, lease time).

**Returns**:

Returns 0 on success, or -1 on failure.

### dhcpc_request_async

```c
int dhcpc_request_async(void *handle, dhcpc_callback_t callback);
```

Asynchronously performs DHCP negotiation, running in a background thread and returning results via callback.

**Parameters**:

- `handle` Session handle.
- `callback` Result callback function.

**Returns**:

Returns 0 on successful start, or -1 on failure.

### dhcpc_cancel

```c
void dhcpc_cancel(void *handle);
```

Cancels an ongoing DHCP negotiation.

### dhcpc_close

```c
void dhcpc_close(void *handle);
```

Closes the DHCP client session and releases all resources. Internally calls `dhcpc_cancel()` first.


## DHCPv6 Client

Header file: `#include <netutils/dhcp6c.h>`

### dhcp6c_open

```c
void *dhcp6c_open(const char *interface);
```

Creates a DHCPv6 client session.

**Parameters**:

- `interface` Network interface name.

**Returns**:

Returns a session handle on success, or `NULL` on failure.

### dhcp6c_request

```c
int dhcp6c_request(void *handle, struct dhcp6c_state *presult);
```

Performs DHCPv6 negotiation to obtain an address (blocking call).

### dhcp6c_request_async

```c
int dhcp6c_request_async(void *handle, dhcp6c_callback_t callback);
```

Asynchronously performs DHCPv6 negotiation.

### dhcp6c_cancel

```c
void dhcp6c_cancel(void *handle);
```

Cancels an ongoing DHCPv6 negotiation.

### dhcp6c_close

```c
void dhcp6c_close(void *handle);
```

Closes the DHCPv6 client session.


## DHCP Server

Header file: `#include <netutils/dhcpd.h>`

### dhcpd_run

```c
int dhcpd_run(const char *interface);
```

Runs the DHCP server on the current thread (blocking, returns only on error).

### dhcpd_start

```c
int dhcpd_start(const char *interface);
```

Starts the DHCP server daemon as a background task.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### dhcpd_stop

```c
int dhcpd_stop(void);
```

Stops the running DHCP server daemon.

### dhcpd_set_startip

```c
int dhcpd_set_startip(in_addr_t startip);
```

Configures the starting IP address of the DHCP server address pool.

**Parameters**:

- `startip` Starting IP address (network byte order).

**Returns**:

Always returns `0`.

### dhcpd_set_routerip

```c
int dhcpd_set_routerip(in_addr_t routerip);
```

Configures the default gateway address distributed by the DHCP server to clients.

**Parameters**:

- `routerip` Default gateway IP (network byte order).

**Returns**:

Always returns `0`.

### dhcpd_set_netmask

```c
int dhcpd_set_netmask(in_addr_t netmask);
```

Configures the subnet mask distributed by the DHCP server to clients.

**Parameters**:

- `netmask` Subnet mask (network byte order).

**Returns**:

Always returns `0`.

### dhcpd_set_dnsip

```c
int dhcpd_set_dnsip(in_addr_t dnsip);
```

Configures the DNS server address distributed by the DHCP server to clients.

**Parameters**:

- `dnsip` DNS server IP (network byte order).

**Returns**:

Always returns `0`.
