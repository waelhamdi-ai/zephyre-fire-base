#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <zephyr/kernel.h>

/* 
 * Connect to Wi-Fi network
 * Returns 0 on success, negative error code on failure
 */
int wifi_connect(void);

#endif /* WIFI_CONNECT_H */