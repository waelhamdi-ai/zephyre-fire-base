#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_connect, LOG_LEVEL_DBG);

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include "wifi_connect.h"

/* Wi-Fi connection parameters - REPLACE WITH YOUR OWN! */
#define WIFI_SSID "TT_2748"
#define WIFI_PSK "gdblpss7zc"

static K_SEM_DEFINE(wifi_connected, 0, 1);

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                  uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        LOG_INF("Wi-Fi connected!");
        k_sem_give(&wifi_connected);
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        LOG_INF("Wi-Fi disconnected - trying to reconnect");
    }
}

int wifi_connect(void)
{
    struct net_if *iface;
    static struct net_mgmt_event_callback wifi_cb;
    struct wifi_connect_req_params wifi_params = {0};
    int ret;

    /* Find Wi-Fi network interface */
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return -ENODEV;
    }

    /* Register for Wi-Fi management events */
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                                NET_EVENT_WIFI_CONNECT_RESULT |
                                NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    /* Configure Wi-Fi parameters */
    wifi_params.ssid = WIFI_SSID;
    wifi_params.ssid_length = strlen(WIFI_SSID);
    wifi_params.psk = WIFI_PSK;
    wifi_params.psk_length = strlen(WIFI_PSK);
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK;

    /* Connect to Wi-Fi network */
    LOG_INF("Connecting to SSID: %s", wifi_params.ssid);
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));
    
    if (ret) {
        LOG_ERR("Failed to request Wi-Fi connect: %d", ret);
        return ret;
    }

    /* Wait for connection */
    ret = k_sem_take(&wifi_connected, K_SECONDS(20));
    if (ret) {
        LOG_ERR("Failed to connect to Wi-Fi: timeout");
        return -ETIMEDOUT;
    }

    return 0;
}