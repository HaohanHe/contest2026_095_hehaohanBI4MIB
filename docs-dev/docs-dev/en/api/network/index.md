\[ English | [简体中文](../../../zh-cn/api/network/index.md) \]

# Network Interface

openvela includes a comprehensive network subsystem that provides standard BSD socket interfaces and DNS resolution capabilities. The network subsystem is optional and can be configured according to application requirements. The network interfaces provided by openvela follow the POSIX standard, making it easy to port existing network applications to openvela.

## Core Interfaces

- **[Network Interface](net.md)** — BSD socket interface (socket/bind/connect/send/recv) and DNS resolution

## Address Management and Configuration

- **[DHCP](net_dhcp.md)** — DHCP client (IPv4), DHCPv6 client, and DHCP server
- **[Network Utility Library netlib](netlib.md)** — IPv4/IPv6 address, routing, MAC, MTU, iptables, connectivity check, and other utility interfaces

## Wireless Networking

- **[WAPI Wireless Interface](wapi.md)** — Wi-Fi interface configuration, scanning, association, and power management (based on Linux Wireless Extensions)

## File Services

- **[FTP Server](net_ftp.md)** — Lightweight FTP server interface
