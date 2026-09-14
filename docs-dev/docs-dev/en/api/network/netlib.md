\[ English | [简体中文](../../../zh-cn/api/network/netlib.md) \]

# Network Utility Library (netlib) API

The openvela network utility library (`netlib_*`) provides a series of helper functions that simplify BSD socket operations, covering IPv4/IPv6 address management, routing, ARP, MAC addresses, MTU, firewall (iptables/ip6tables), network connectivity checks, and more.

Header: `#include <netutils/netlib.h>`

## openvela Implementation Notes

- **Purpose**: Convenience wrappers on top of the BSD socket API, hiding low-level details such as `ioctl` + `SIOCGIF*`
- **Coverage**:
    - IPv4/IPv6 addresses, gateways, subnet masks, DNS, routes
    - MAC address read/write, interface up/down, MTU setting
    - VLAN management, ARP table operations
    - iptables/ip6tables operations
    - Network connectivity checks (`ping` / HTTP / interface reachability)
    - URL parsing utilities
- **Configuration dependencies**: Requires `CONFIG_NETUTILS_NETLIB` to be enabled; some sub-interfaces additionally require the corresponding module configuration (such as `CONFIG_NET_ARP`, `CONFIG_NET_IPv6`, etc.)
- **Error handling**: Most interfaces return `0` or `OK` on success, or `ERROR` (`-1`) with `errno` set on failure

## Network Utility Library

Header: `#include <netutils/netlib.h>`

netlib provides network configuration utility functions, including interface address setting, route management, ARP operations, and more.

**IPv4 Address Management**

### netlib_get_ipv4addr

```c
int netlib_get_ipv4addr(const char *ifname, struct in_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Used to store the IP address.

### netlib_set_ipv4addr

```c
int netlib_set_ipv4addr(const char *ifname, const struct in_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Address to set.

### netlib_set_dripv4addr

```c
int netlib_set_dripv4addr(const char *ifname, const struct in_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Address to set.

### netlib_get_dripv4addr

```c
int netlib_get_dripv4addr(const char *ifname, struct in_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Used to store the default route address.

### netlib_set_ipv4netmask

```c
int netlib_set_ipv4netmask(const char *ifname, const struct in_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Address to set.

### netlib_get_ipv4netmask

```c
int netlib_get_ipv4netmask(const char *ifname, struct in_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Used to store the subnet mask.

### netlib_ipv4adaptor

```c
int netlib_ipv4adaptor(in_addr_t destipaddr, in_addr_t *srcipaddr);
```

**Parameters**:

- `destipaddr` Target IPv4 address.
- `srcipaddr` Used to store the adapter address.

### netlib_read_ipv4route

```c
ssize_t netlib_read_ipv4route(FILE *stream, struct netlib_ipv4_route_s *route);
```

**Parameters**:

- `fd` File descriptor for the procfs IPv4 routing table.
- `route` Used to store the next routing entry.

### netlib_ipv4router

```c
int netlib_ipv4router(const struct in_addr *destipaddr, struct in_addr *router);
```

**Parameters**:

- `destipaddr` Target IP address.
- `router` Used to store the IP address of the gateway router.

### netlib_obtain_ipv4addr

```c
int netlib_obtain_ipv4addr(const char *ifname);
```

**Parameters**:

- `ifname` Network interface name.

### netlib_set_ipv4dnsaddr

```c
int netlib_set_ipv4dnsaddr(const struct in_addr *inaddr);
```

**Parameters**:

- `inaddr` Address to set.

**IPv6 Address Management**

### netlib_add_ipv6addr

```c
int netlib_add_ipv6addr(const char *ifname, const struct in6_addr *addr, uint8_t preflen);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Address to add.
- `preflen` Prefix length (in bits).

### netlib_del_ipv6addr

```c
int netlib_del_ipv6addr(const char *ifname, const struct in6_addr *addr, uint8_t preflen);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Address to delete.
- `preflen` Prefix length (in bits).

### netlib_get_ipv6addr

```c
int netlib_get_ipv6addr(const char *ifname, struct in6_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Used to store the IP address.

### netlib_set_ipv6addr

```c
int netlib_set_ipv6addr(const char *ifname, const struct in6_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Address to set.

### netlib_set_dripv6addr

```c
int netlib_set_dripv6addr(const char *ifname, const struct in6_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Address to set.

### netlib_set_ipv6netmask

```c
int netlib_set_ipv6netmask(const char *ifname, const struct in6_addr *addr);
```

**Parameters**:

- `ifname` Network interface name.
- `ipaddr` Address to set.

### netlib_ipv6adaptor

```c
int netlib_ipv6adaptor(const struct in6_addr *destipaddr, struct in6_addr *srcipaddr);
```

**Parameters**:

- `destipaddr` Target IP address.
- `srcipaddr` Used to store the adapter address.

### netlib_ipv6netmask2prefix

```c
uint8_t netlib_ipv6netmask2prefix(const uint16_t *mask);
```

**Parameters**:

- `mask` Subnet mask.

### netlib_prefix2ipv6netmask

```c
void netlib_prefix2ipv6netmask(uint8_t preflen, struct in6_addr *netmask);
```

**Parameters**:

- `preflen` Prefix length (in bits).
- `netmask` Used to store the subnet mask.

### netlib_read_ipv6route

```c
ssize_t netlib_read_ipv6route(FILE *stream, struct netlib_ipv6_route_s *route);
```

**Parameters**:

- `fd` Routing entry.
- `route` Used to store the next routing entry.

### netlib_ipv6router

```c
int netlib_ipv6router(const struct in6_addr *destipaddr, struct in6_addr *router);
```

**Parameters**:

- `destipaddr` Target IP address.
- `router` Used to store the IP address of the gateway router.

### netlib_obtain_ipv6addr

```c
int netlib_obtain_ipv6addr(const char *ifname);
```

**Parameters**:

- `ifname` Network interface name.

### netlib_set_ipv6dnsaddr

```c
int netlib_set_ipv6dnsaddr(const struct in6_addr *inaddr);
```

**Parameters**:

- `inaddr` Address to set.

**Interface Management**

### netlib_setmacaddr

```c
int netlib_setmacaddr(const char *ifname, const uint8_t *macaddr);
```

**Parameters**:

- `ifname` Network interface name.
- `macaddr` MAC address.

### netlib_getmacaddr

```c
int netlib_getmacaddr(const char *ifname, uint8_t *macaddr);
```

**Parameters**:

- `ifname` Network interface name.
- `macaddr` Used to store the MAC address.

### netlib_getessid

```c
int netlib_getessid(const char *ifname, char *essid, size_t idlen);
```

**Parameters**:

- `ifname` Network interface name.
- `essid` Used to store the result.
- `idlen` ESSID buffer size.

### netlib_setessid

```c
int netlib_setessid(const char *ifname, const char *essid);
```

**Parameters**:

- `ifname` Network interface name.
- `essid` ESSID (network name).

### netlib_getifstatus

```c
int netlib_getifstatus(const char *ifname, uint8_t *flags);
```

**Parameters**:

- `ifname` Network interface name.
- `flags` Interface flags.

### netlib_ifup

```c
int netlib_ifup(const char *ifname);
```

**Parameters**:

- `ifname` Network interface name.

### netlib_ifdown

```c
int netlib_ifdown(const char *ifname);
```

**Parameters**:

- `ifname` Network interface name.

### netlib_set_mtu

```c
int netlib_set_mtu(const char *ifname, int mtu);
```

**Parameters**:

- `ifname` Network interface name.
- `mtu` Maximum Transmission Unit (MTU).

**Returns**:

:

### netlib_getifstatistics

```c
int netlib_getifstatistics(const char *ifname, struct netdev_statistics_s *stat);
```

**Parameters**:

- `ifname` Network interface name.
- `stat` Used to store device statistics.

### netlib_check_ifconflict

```c
int netlib_check_ifconflict(const char *ifname);
```

**Parameters**:

- `ifname` Network interface name.

**Route Management**

### netlib_get_route

```c
ssize_t netlib_get_route(struct rtentry *rtelist, unsigned int nentries, sa_family_t family);
```

**Parameters**:

- `rtelist` Used to store the device list.
- `nentries` Array capacity (entry count).
- `family` Address family. See AF_* definitions in.

**ARP Management**

### netlib_del_arpmapping

```c
int netlib_del_arpmapping(const struct sockaddr_in *inaddr, const char *ifname);
```

**Parameters**:

- `inaddr` IPv4 address.
- `ifname` Network interface name.

### netlib_get_arpmapping

```c
int netlib_get_arpmapping(const struct sockaddr_in *inaddr, uint8_t *macaddr, const char *ifname);
```

**Parameters**:

- `inaddr` IPv4 address.
- `macaddr` Used to store the corresponding Ethernet MAC address.
- `ifname` Network interface name.

### netlib_set_arpmapping

```c
int netlib_set_arpmapping(const struct sockaddr_in *inaddr, const uint8_t *macaddr, const char *ifname);
```

**Parameters**:

- `inaddr` IPv4 address.
- `macaddr` MAC address.
- `ifname` Network interface name.

### netlib_get_arptable

```c
ssize_t netlib_get_arptable(struct arpreq *arptab, unsigned int nentries);
```

**Parameters**:

- `arptab` Used to store a copy of the ARP table.
- `nentries` Array capacity (entry count).

### netlib_ifarp

```c
int netlib_ifarp(const char *ifname);
```

**Parameters**:

- `ifname` Network interface name.

### netlib_ifnoarp

```c
int netlib_ifnoarp(const char *ifname);
```

**Parameters**:

- `ifname` Network interface name.

**DNS Management**

### netlib_clear_dnsaddr

```c
void netlib_clear_dnsaddr(void);
```

**VLAN Management**

### netlib_add_vlan

```c
int netlib_add_vlan(const char *ifname, int vlanid, int prio);
```

**Parameters**:

- `ifname` Network interface name.
- `vlanid` VLAN identifier.
- `prio` Default VLAN priority (PCP).

### netlib_del_vlan

```c
int netlib_del_vlan(const char *vlanif);
```


**iptables**

### netlib_ipt_commit

```c
int netlib_ipt_commit(const struct ipt_replace *repl);
```

**Parameters**:

- `repl` Configuration to commit.

### netlib_ipt_flush

```c
int netlib_ipt_flush(const char *table, enum nf_inet_hooks hook);
```

**Parameters**:

- `table` Table name.
- `hook` Hook point.

### netlib_ipt_policy

```c
int netlib_ipt_policy(const char *table, enum nf_inet_hooks hook, int verdict);
```

**Parameters**:

- `table` Policy.
- `hook` Hook point.
- `verdict` Verdict value.

### netlib_ipt_append

```c
int netlib_ipt_append(struct ipt_replace **repl, const struct ipt_entry *entry, enum nf_inet_hooks hook);
```

**Parameters**:

- `repl` Configuration to commit.
- `entry` Rule entry to append.
- `hook` Hook point.

### netlib_ipt_insert

```c
int netlib_ipt_insert(struct ipt_replace **repl, const struct ipt_entry *entry, enum nf_inet_hooks hook, int rulenum);
```

**Parameters**:

- `repl` Configuration to commit.
- `entry` Rule entry to insert.
- `hook` Hook point.
- `rulenum` Rule number.

### netlib_ipt_delete

```c
int netlib_ipt_delete(struct ipt_replace *repl, const struct ipt_entry *entry, enum nf_inet_hooks hook, int rulenum);
```

**Parameters**:

- `repl` Configuration to commit.
- `entry` Rule entry to delete.
- `hook` Hook point.
- `rulenum` Rule number.

### netlib_ipt_fillifname

```c
int netlib_ipt_fillifname(struct ipt_entry *entry, const char *inifname, const char *outifname);
```

**Parameters**:

- `entry` Rule entry to fill.
- `inifname` Input device name; `NULL` means unchanged.
- `outifname` Output device name; `NULL` means unchanged.

### netlib_ip6t_commit

```c
int netlib_ip6t_commit(const struct ip6t_replace *repl);
```

**Parameters**:

- `repl` Configuration to commit.

### netlib_ip6t_flush

```c
int netlib_ip6t_flush(const char *table, enum nf_inet_hooks hook);
```

**Parameters**:

- `table` Table name.
- `hook` Hook point.

### netlib_ip6t_policy

```c
int netlib_ip6t_policy(const char *table, enum nf_inet_hooks hook, int verdict);
```

**Parameters**:

- `table` Policy.
- `hook` Hook point.
- `verdict` Verdict value.

### netlib_ip6t_append

```c
int netlib_ip6t_append(struct ip6t_replace **repl, const struct ip6t_entry *entry, enum nf_inet_hooks hook);
```

**Parameters**:

- `repl` Configuration to commit.
- `entry` Rule entry to append.
- `hook` Hook point.

### netlib_ip6t_insert

```c
int netlib_ip6t_insert(struct ip6t_replace **repl, const struct ip6t_entry *entry, enum nf_inet_hooks hook, int rulenum);
```

**Parameters**:

- `repl` Configuration to commit.
- `entry` Rule entry to insert.
- `hook` Hook point.
- `rulenum` Rule number.

### netlib_ip6t_delete

```c
int netlib_ip6t_delete(struct ip6t_replace *repl, const struct ip6t_entry *entry, enum nf_inet_hooks hook, int rulenum);
```

**Parameters**:

- `repl` Configuration to commit.
- `entry` Rule entry to delete.
- `hook` Hook point.
- `rulenum` Rule number.

### netlib_ip6t_fillifname

```c
int netlib_ip6t_fillifname(struct ip6t_entry *entry, const char *inifname, const char *outifname);
```

**Parameters**:

- `entry` Rule entry to fill.
- `inifname` Input device name; `NULL` means unchanged.
- `outifname` Output device name; `NULL` means unchanged.

**Connectivity Checks**

### netlib_check_ipconnectivity

```c
int netlib_check_ipconnectivity(const char *ip, int timeout, int retry);
```

**Parameters**:

- `ip` IPv4 address to check.
- `timeout` Timeout.
- `retry` Retry count.

### netlib_check_ifconnectivity

```c
int netlib_check_ifconnectivity(const char *ifname, int timeout, int retry);
```

**Parameters**:

- `ifname` Network interface name.
- `timeout` Timeout.
- `retry` Retry count.

**URL Parsing**

### netlib_parsehttpurl

```c
int netlib_parsehttpurl(const char *url, uint16_t *port, char *hostname, int hostlen, char *filename, int namelen);
```

**Parameters**:

- `url` HTTP-related parameter.
- `port` Pointer to a `uint16_t` used to store the parsed port number.
- `hostname` Buffer used to store the result.
- `hostlen` Buffer size.
- `filename` Buffer used to store the result.
- `namelen` Buffer size.

### netlib_parseurl

```c
int netlib_parseurl(const char *str, struct url_s *url);
```

### netlib_check_httpconnectivity

```c
int netlib_check_httpconnectivity(const char *host, const char *getmsg, int port, int expect_code);
```

**Parameters**:

- `host` Remote host address.
- `getmsg` HTTP-related parameter.
- `port` Port number.
- `expect_code` HTTP-related parameter.

**Others**

### netlib_get_devices

```c
ssize_t netlib_get_devices(struct netlib_device_s *devlist, unsigned int nentries, sa_family_t family);
```

**Parameters**:

- `devlist` Used to store the device list.
- `nentries` Array capacity (entry count).
- `family` Address family. See AF_* definitions in.

### netlib_seteaddr

```c
int netlib_seteaddr(const char *ifname, const uint8_t *eaddr);
```

**Parameters**:

- `ifname` Network interface name.
- `eaddr` New address.

### netlib_getpanid

```c
int netlib_getpanid(const char *ifname, uint8_t *panid);
```

**Parameters**:

- `ifname` Network interface name.
- `panid` Used to store the current PAN ID.

### netlib_getproperties

```c
int netlib_getproperties(const char *ifname, struct pktradio_properties_s *properties);
```

**Parameters**:

- `ifname` Network interface name.
- `nodeadd` Used to store the node address.

### netlib_setnodeaddr

```c
int netlib_setnodeaddr(const char *ifname, const struct pktradio_addr_s *nodeaddr);
```

**Parameters**:

- `ifname` Network interface name.
- `nodeadd` New address.

### netlib_getnodnodeaddr

```c
int netlib_getnodnodeaddr(const char *ifname, struct pktradio_addr_s *nodeaddr);
```

**Parameters**:

- `ifname` Network interface name.
- `nodeadd` Used to store the node address.

### netlib_get_nbtable

```c
ssize_t netlib_get_nbtable(struct neighbor_entry_s *nbtab, unsigned int nentries);
```

**Parameters**:

- `nbtab` Used to store a copy of the neighbor table.
- `nentries` Array capacity (entry count).

### netlib_icmpv6_autoconfiguration

```c
int netlib_icmpv6_autoconfiguration(const char *ifname);
```

**Parameters**:

- `ifname` Network interface name.

### netlib_parse_conntrack

```c
int netlib_parse_conntrack(const struct nlmsghdr *nlh, size_t len, struct netlib_conntrack_s *ct);
```

**Parameters**:

- `nlh` Netlink message to parse.
- `ct` Connection tracking entry.

### netlib_get_conntrack

```c
int netlib_get_conntrack(sa_family_t family, netlib_conntrack_cb_t cb);
```

**Parameters**:

- `family` Address family, used to filter conntrack entries.
- `cb` Connection tracking entry.

### netlib_listenon

```c
int netlib_listenon(uint16_t portno);
```

**Parameters**:

- `portno` Port number.

### netlib_server

```c
void netlib_server(uint16_t portno, pthread_startroutine_t handler, int stacksize);
```

**Parameters**:

- `portno` Port number.
- `handler` Task entry function.
- `stacksize` Stack size.

### netlib_get_iobinfo

```c
int netlib_get_iobinfo(struct iob_stats_s *iob);
```

**Parameters**:

- `iob` IOB information structure.


### netlib_ipv4addrconv

```c
bool netlib_ipv4addrconv(const char *addrstr, uint8_t *addr);
```

Convert an IPv4 address string (e.g., `"192.168.1.1"`) to a 4-byte binary array.

**Parameters**:

- `addrstr` IPv4 address string.
- `addr` Output buffer (4 bytes).

**Returns**:

Returns `true` on successful conversion, or `false` if the format is invalid.

### netlib_ethaddrconv

```c
bool netlib_ethaddrconv(const char *hwstr, uint8_t *hw);
```

Convert an Ethernet MAC address string (e.g., `"aa:bb:cc:dd:ee:ff"`) to a 6-byte binary array.

**Parameters**:

- `hwstr` MAC address string.
- `hw` Output buffer (6 bytes).

**Returns**:

Returns `true` on successful conversion, or `false` if the format is invalid.

### netlib_saddrconv

```c
bool netlib_saddrconv(const char *hwstr, uint8_t *hw);
```

Convert an IEEE 802.15.4 short address (2 bytes) string to binary form.

**Parameters**:

- `hwstr` Address string.
- `hw` Output buffer (2 bytes).

**Returns**:

Returns `true` on successful conversion, or `false` if the format is invalid.

### netlib_eaddrconv

```c
bool netlib_eaddrconv(const char *hwstr, uint8_t *hw);
```

Convert an IEEE 802.15.4 extended address (8 bytes) string to binary form.

**Parameters**:

- `hwstr` Address string.
- `hw` Output buffer (8 bytes).

**Returns**:

Returns `true` on successful conversion, or `false` if the format is invalid.

### netlib_nodeaddrconv

```c
bool netlib_nodeaddrconv(const char *addrstr,
                         struct pktradio_addr_s *nodeaddr);
```

Convert a pktradio node address string to a `pktradio_addr_s` structure.

**Parameters**:

- `addrstr` Node address string.
- `nodeaddr` Output structure pointer.

**Returns**:

Returns `true` on successful conversion, or `false` if the format is invalid.

