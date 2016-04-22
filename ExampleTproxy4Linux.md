# Introduction #

This is from my local lab. It should work in production.

# Overview #

  * Debian unstable - includes a tproxy4 enabled kernel
  * Lusca-head
  * Cisco 3750 running advanced IP services
  * clients on Vlan10 - IP 192.168.10.0/24
  * internet on vlan11- IP 192.168.11.0/24
  * lusca/proxy on vlan9 - IP 192.168.9.0/24

The Cisco 3750 has limited WCCPv2 support. It only supports "in" redirection on an interface, not "out". It also does not support "deny" in the redirect lists in hardware. The redirect lists are ALLOW only with an implicit "deny any" at the end.

The redirect lists used here are to redirect traffic to/from the client ranges to the proxy whilst leaving other traffic untouched.

Note that the proxy servers live on a separate VLAN to the clients and internet - this way the Cisco can redirect traffic as appropriate to and from the proxy.

Redirection is done via L2 redirect, not GRE. The assignment method is "mask", not hash.

# Details #

## Cisco Config ##

```
ip wccp 80 redirect-list IP_WCCP_REDIRECT_INT
ip wccp 90 redirect-list IP_WCCP_REDIRECT_EXT
!
interface vlan10
  description Cache Clients
  ip address 192.168.10.1 255.255.255.0
  ip wccp 80 redirect in
!
interface vlan11
  description Cache Servers/Internet
  ip address 192.168.11.1 255.255.255.0
  ip wccp 90 redirect in
!
ip access-list extended IP_WCCP_REDIRECT_EXT
  permit ip any 192.168.10.0 0.0.0.255
!
ip access-list extended IP_WCCP_REDIRECT_INT
  permit ip 192.168.10.0 0.0.0.255 any
!
```

## Linux Config ##


### Extra packages ###

  * libcap2
  * libcap-dev

### /root/tproxy.sh ###
```
#!/bin/sh

IPTABLES=/sbin/iptables
${IPTABLES} -v -t mangle -N DIVERT
${IPTABLES} -v -t mangle -A DIVERT -j MARK --set-mark 1
${IPTABLES} -v -t mangle -A DIVERT -j ACCEPT

${IPTABLES} -v -t mangle -A PREROUTING -p tcp -m socket -j DIVERT
${IPTABLES} -v -t mangle -A PREROUTING -p tcp --dport 80 -j TPROXY --tproxy-mark 0x1/0x1 --on-port 3129

ip rule add fwmark 1 lookup 100
ip route add local 0.0.0.0/0 dev lo table 100

sysctl net.ipv4.ip_nonlocal_bind=1
sysctl net.ipv4.ip_forward=1
```

## Lusca Config ##

### Compile Options ###

```
  configure options: '--prefix="/home/adrian/work/lusca/run' '--enable-storeio=aufs null' '--with-large-files' '--enable-large-cache-files' '--enable-snmp' '--enable-linux-tproxy4'
```

### Configuration ###
visible_hostname 192.168.9.13
http_port 0.0.0.0:3129 transparent tproxy
http_port localhost:3128

wccp2_router 192.168.9.1

wccp2_service dynamic 80
wccp2_service_info 80 protocol=tcp flags=src_ip_hash priority=240 ports=80

wccp2_service dynamic 90
wccp2_service_info 90 protocol=tcp flags=dst_ip_hash,ports_source priority=240 ports=80

# L2 forwarding
wccp2_forwarding_method 2
# L2 return
wccp2_return_method 2
# Mask assignment method
wccp2_assignment_method 2```