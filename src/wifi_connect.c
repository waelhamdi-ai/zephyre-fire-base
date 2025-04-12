#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_connect, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>

#include "wifi_connect.h"

/* Wi-Fi connection parameters */
#define WIFI_SSID "TT_2748"
#define WIFI_PSK "gdblpss7zc"

int wifi_connect(void)
{
    struct net_if *iface;
    int ret = 0;

    LOG_INF("Setting up WiFi connection");

    /* Get network interface */
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return -ENODEV;
    }

    /* Force DHCP to get IP address */
    net_dhcpv4_start(iface);

    /* Wait for connection and IP address (DHCP) */
    LOG_INF("Waiting for IP address assignment...");
    k_sleep(K_SECONDS(15));

    /* At this point, the ESP-AT module should be connected to WiFi via its config */
    struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
    if (ipv4 && ipv4->unicast[0].is_used) {
        char ip_str[NET_IPV4_ADDR_LEN];
        LOG_INF("IPv4 address: %s",
                net_addr_ntop(AF_INET, &ipv4->unicast[0].address.in_addr,
                              ip_str, sizeof(ip_str)));
        return 0;
    } else {
        LOG_ERR("Failed to get IP address");
        return -ETIMEDOUT;
    }
}