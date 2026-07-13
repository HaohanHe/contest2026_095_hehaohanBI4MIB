\[ English | [简体中文](../../../zh-cn/api/network/wapi.md) \]

# Wireless Network Interface (WAPI) API

The `wapi_*` series of interfaces is a wrapper around Linux Wireless Extensions (WEXT), providing wireless network configuration, scanning, association, power management, country code, and PMKSA cache capabilities.

Header: `#include <wireless/wapi.h>`

## openvela Implementation Notes

- **Underlying mechanism**: Communicates with Wi-Fi drivers via the ioctl protocol of Linux Wireless Extensions (WEXT)
- **Supported scenarios**: Station (client) mode, AP mode, and promiscuous mode (depending on driver support)
- **Configuration dependency**: Requires `CONFIG_WIRELESS_WAPI` and the corresponding Wi-Fi chip driver to be enabled
- **Typical usage**:
    - `wapi_set_ifup`/`wapi_set_ifdown` to bring the interface up or down
    - `wapi_set_essid` + `wapi_set_mode` to configure the connection target
    - `wapi_scan_*` / `wapi_escan_*` to scan surrounding APs
    - `wapi_load_config` / `wapi_save_config` to persist configuration
- **Extended capabilities**: Interact with driver-specific features via interfaces such as `wapi_extend_params` / `wapi_set_pmksa`

## Wireless Network Interface

Header: `#include <wireless/wapi.h>`

wapi provides wireless network configuration interfaces, including SSID scanning, connection, and frequency setting.

**Connection Management**

### wapi_get_ifup

```c
int wapi_get_ifup(int sock, const char *ifname, int *is_up);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `is_up` Interface status; 0 means enabled, 1 means disabled.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_ifup

```c
int wapi_set_ifup(int sock, const char *ifname);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_ifdown

```c
int wapi_set_ifdown(int sock, const char *ifname);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name to be brought down.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_ip

```c
int wapi_get_ip(int sock, const char *ifname, struct in_addr *addr);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `addr` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_ip

```c
int wapi_set_ip(int sock, const char *ifname, const struct in_addr *addr);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name whose IP address is set.
- `addr` Pointer to the structure containing the new IP address.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_netmask

```c
int wapi_get_netmask(int sock, const char *ifname, struct in_addr *addr);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `addr` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_netmask

```c
int wapi_set_netmask(int sock, const char *ifname, const struct in_addr *addr);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `addr` Pointer to the structure containing the new subnet mask.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_add_route_gw

```c
int wapi_add_route_gw(int sock, enum wapi_route_target_e targettype, const struct in_addr *target, const struct in_addr *netmask, const struct in_addr *gw);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `targettype` Target type.
- `target` Pointer to the target IP address.
- `netmask` Pointer to the subnet mask corresponding to the target address.
- `gw` Pointer to the corresponding gateway (router) IP address, used for routing.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_del_route_gw

```c
int wapi_del_route_gw(int sock, enum wapi_route_target_e targettype, const struct in_addr *target, const struct in_addr *netmask, const struct in_addr *gw);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `targettype` Target type.
- `target` Pointer to the target IP address of the route.
- `netmask` Pointer to the subnet mask corresponding to the target address.
- `gw` Pointer to the corresponding gateway (router) IP address, used for routing.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_freq

```c
int wapi_get_freq(int sock, const char *ifname, double *freq, enum wapi_freq_flag_e *flag);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `freq` Output parameter.
- `flag` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_freq

```c
int wapi_set_freq(int sock, const char *ifname, double freq, enum wapi_freq_flag_e flag);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `freq` Frequency value.
- `flag` Frequency value.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_freq2chan

```c
int wapi_freq2chan(int sock, const char *ifname, double freq, int *chan);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `freq` Frequency, in Hz, to be converted to a channel number.
- `chan` Output parameter.

### wapi_chan2freq

```c
int wapi_chan2freq(int sock, const char *ifname, int chan, double *freq);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Channel.
- `chan` Channel number to be converted to a frequency.
- `freq` Output parameter.

### wapi_get_essid

```c
int wapi_get_essid(int sock, const char *ifname, char *essid, enum wapi_essid_flag_e *flag);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `essid` Used to store the result.
- `flag` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_essid

```c
int wapi_set_essid(int sock, const char *ifname, const char *essid, enum wapi_essid_flag_e flag);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `essid` Pointer to a `\0`-terminated ESSID string.
- `flag` Control flag.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_mode

```c
int wapi_get_mode(int sock, const char *ifname, enum wapi_mode_e *mode);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `mode` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_mode

```c
int wapi_set_mode(int sock, const char *ifname, enum wapi_mode_e mode);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `mode` Output parameter.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_make_broad_ether

```c
int wapi_make_broad_ether(struct ether_addr *sa);
```

**Parameters**:

- `sa` Output parameter.

**Returns**:

Returns the result of the underlying `wapi_make_ether()` call.

### wapi_make_null_ether

```c
int wapi_make_null_ether(struct ether_addr *sa);
```

**Parameters**:

- `sa` Output parameter.

**Returns**:

Returns the result of the underlying `wapi_make_ether()` call.

### wapi_get_ap

```c
int wapi_get_ap(int sock, const char *ifname, struct ether_addr *ap);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `ap` Address to set.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_ap

```c
int wapi_set_ap(int sock, const char *ifname, const struct ether_addr *ap);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `ap` MAC address of the access point.

**Returns**:

Returns 0 on success, or a negative error code on failure.


### wapi_get_bitrate

```c
int wapi_get_bitrate(int sock, const char *ifname, int *bitrate, enum wapi_bitrate_flag_e *flag);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `bitrate` Output parameter for the retrieved bitrate.
- `flag` Output parameter for the bitrate flag bits.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_bitrate

```c
int wapi_set_bitrate(int sock, const char *ifname, int bitrate, enum wapi_bitrate_flag_e flag);
```

**Parameters**:

- `sock` Socket descriptor (used for ioctl operations).
- `ifname` Network interface name.
- `bitrate` Bitrate.
- `flag` Bitrate flag.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_dbm2mwatt

```c
int wapi_dbm2mwatt(int dbm);
```

**Parameters**:

- `dbm` dBm value to convert.

**Returns**:

The converted milliwatt value.

### wapi_mwatt2dbm

```c
int wapi_mwatt2dbm(int mwatt);
```

**Parameters**:

- `mwatt` Milliwatt value.

**Returns**:

The converted dBm value.

### wapi_get_txpower

```c
int wapi_get_txpower(int sock, const char *ifname, int *power, enum wapi_txpower_flag_e *flag);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `power` Output parameter for the transmit power value.
- `flag` Output parameter for the unit of the transmit power.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_txpower

```c
int wapi_set_txpower(int sock, const char *ifname, int power, enum wapi_txpower_flag_e flag);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `power` Transmit power.
- `flag` Transmit power.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_make_socket

```c
int wapi_make_socket(void);
```

### wapi_scan_init

```c
int wapi_scan_init(int sock, const char *ifname, const char *essid);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `essid` ESSID to scan.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_scan_channel_init

```c
int wapi_scan_channel_init(int sock, const char *ifname, const char *essid, uint8_t *channels, int num_channels);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `essid` ESSID to scan.
- `channels` Pointer to an array of channel numbers to scan.
- `num_channels` Channel count.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_escan_init

```c
int wapi_escan_init(int sock, const char *ifname, uint8_t scan_type, const char *essid);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `scan_type` Scan type.
- `essid` ESSID to scan.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_escan_channel_init

```c
int wapi_escan_channel_init(int sock, const char *ifname, uint8_t scan_type, const char *essid, uint8_t *channels, int num_channels);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `scan_type` Scan type.
- `essid` ESSID to scan.
- `channels` Pointer to an array of channel numbers to scan.
- `num_channels` Channel count.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_scan_stat

```c
int wapi_scan_stat(int sock, const char *ifname);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.

### wapi_scan_coll

```c
int wapi_scan_coll(int sock, const char *ifname, struct wapi_list_s *aps);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `aps` List of collected scan results.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_scan_coll_free

```c
void wapi_scan_coll_free(struct wapi_list_s *aps);
```

**Parameters**:

- `aps` Scan result list to free.

### wapi_set_country

```c
int wapi_set_country(int sock, const char *ifname, const char *country);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `country` Pointer to a two-character string indicating the country code to set.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_country

```c
int wapi_get_country(int sock, const char *ifname, char *country);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `country` Pointer to the caller-provided buffer to receive the country code.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_sensitivity

```c
int wapi_get_sensitivity(int sock, const char *ifname, int *sense);
```

**Parameters**:

- `sock` Socket descriptor.
- `ifname` Network interface name.
- `sense` Pointer to the caller-provided integer variable to receive the sensitivity value.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_load_config

```c
void *wapi_load_config(const char *ifname, const char *confname, struct wpa_wconfig_s *conf);
```

**Parameters**:

- `ifname` Network interface name.
- `confname` Path.
- `conf` Pointer to the caller-provided structure to be filled with configuration data.

### wapi_unload_config

```c
void wapi_unload_config(void *load);
```

**Parameters**:

- `load` Configuration resource handle.

### wapi_save_config

```c
int wapi_save_config(const char *ifname, const char *confname, const struct wpa_wconfig_s *conf);
```

**Parameters**:

- `ifname` Network interface name.
- `confname` Path.
- `conf` Pointer to the structure containing the configuration information.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_pta_prio

```c
int wapi_set_pta_prio(int sock, const char *ifname, enum wapi_pta_prio_e pta_prio);
```

**Parameters**:

- `sock` File descriptor.
- `ifname` Network interface name.
- `pta_prio` PTA priority.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_pta_prio

```c
int wapi_get_pta_prio(int sock, const char *ifname, enum wapi_pta_prio_e *pta_prio);
```

**Parameters**:

- `sock` File descriptor.
- `ifname` Network interface name.
- `pta_prio` Pointer to the variable that receives the current PTA priority.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_pmksa

```c
int wapi_set_pmksa(int sock, const char *ifname, const uint8_t *pmk, int len);
```

**Parameters**:

- `sock` File descriptor.
- `ifname` Network interface name.
- `pmk` Pointer to the buffer containing the PMKSA data.
- `len` Length.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_pmksa

```c
int wapi_get_pmksa(int sock, const char *ifname, uint8_t *pmk, int len);
```

**Parameters**:

- `sock` File descriptor.
- `ifname` Network interface name.
- `pmk` Pointer to the buffer that receives the retrieved PMKSA data.
- `len` Buffer size.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_extend_params

```c
int wapi_extend_params(int sock, int cmd, struct iwreq *wrq);
```

**Parameters**:

- `sock` File descriptor.
- `cmd` Private ioctl command code.
- `wrq` Pointer to an `iwreq` structure that the caller must populate in advance.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_set_power_save

```c
int wapi_set_power_save(int sock, const char *ifname, bool on);
```

**Parameters**:

- `sock` File descriptor.
- `ifname` Network interface name.
- `on` Control flag.

**Returns**:

Returns 0 on success, or a negative error code on failure.

### wapi_get_power_save

```c
int wapi_get_power_save(int sock, const char *ifname, bool *on);
```

**Parameters**:

- `sock` File descriptor.
- `ifname` Network interface name.
- `on` Pointer to a boolean variable that receives the current state.

**Returns**:

Returns 0 on success, or a negative error code on failure.

