#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_connect, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include "wifi_connect.h"

/* Wi-Fi connection parameters */
#define WIFI_SSID "TT_2748"
#define WIFI_PSK "gdblpss7zc"

static K_SEM_DEFINE(wifi_connected, 0, 1);

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                  uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        LOG_INF("WiFi connected!");
        k_sem_give(&wifi_connected);
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        LOG_INF("WiFi disconnected");
    }
}

int wifi_connect(void)
{
    struct net_if *iface;
    static struct net_mgmt_event_callback wifi_cb;
    struct wifi_connect_req_params wifi_params = {0};
    int ret;

    LOG_INF("Setting up WiFi connection");

    /* Get network interface */
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return -ENODEV;
    }

    /* Register for WiFi connect/disconnect events */
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                               NET_EVENT_WIFI_CONNECT_RESULT |
                               NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    /* Configure WiFi connection parameters */
    wifi_params.ssid = WIFI_SSID;
    wifi_params.ssid_length = strlen(WIFI_SSID);
    wifi_params.psk = WIFI_PSK;
    wifi_params.psk_length = strlen(WIFI_PSK);
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK;

    /* Connect to WiFi network */
    LOG_INF("Connecting to SSID: %s", WIFI_SSID);
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));
    if (ret) {
        LOG_ERR("Failed to request WiFi connect: %d", ret);
        return ret;
    }

    /* Wait for connection */
    ret = k_sem_take(&wifi_connected, K_SECONDS(30));
    if (ret) {
        LOG_ERR("Connection to WiFi timed out");
        return -ETIMEDOUT;
    }

    /* Start DHCP to get IP address */
    LOG_INF("Starting DHCP");
    net_dhcpv4_start(iface);

    /* Wait for IP address */
    k_sleep(K_SECONDS(5));

    /* Print IP address */
    struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
    if (ipv4 && ipv4->unicast[0].is_used) {
        char ip_str[NET_IPV4_ADDR_LEN];
        LOG_INF("IPv4 address: %s",
                net_addr_ntop(AF_INET, &ipv4->unicast[0].address.in_addr,
                            ip_str, sizeof(ip_str)));
    } else {
        LOG_WRN("No IPv4 address assigned");
    }

    return 0;
}