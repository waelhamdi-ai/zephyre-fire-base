/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_http_client_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>

#include "ca_certificate.h"
#include "wifi_connect.h"
#include "firebase_config.h"

#define MAX_RECV_BUF_LEN 2048
static uint8_t recv_buf[MAX_RECV_BUF_LEN];

static void response_cb(struct http_response *rsp,
             enum http_final_call final_data,
             void *user_data)
{
    if (final_data == HTTP_DATA_MORE) {
        LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
    } else if (final_data == HTTP_DATA_FINAL) {
        LOG_INF("All the data received (%zd bytes)", rsp->data_len);
    }

    LOG_INF("Response to %s", (const char *)user_data);
    LOG_INF("Response status %s", rsp->http_status);
    
    // Fix: In Zephyr's HTTP client, response data is in the recv_buf, not in rsp->data
    if (rsp->data_len > 0) {
        LOG_INF("Payload: %.*s", (int)rsp->data_len, (char *)recv_buf);
    } else {
        LOG_INF("No payload received");
    }
}

static int connect_to_firebase(void)
{
    struct sockaddr_in addr4;
    int sock = -1;
    int32_t timeout = 10 * MSEC_PER_SEC;
    int ret;

    // Register certificates
    ret = tls_credential_add(CA_CERTIFICATE_TAG,
                 TLS_CREDENTIAL_CA_CERTIFICATE,
                 ca_certificate,
                 sizeof(ca_certificate));
    if (ret < 0) {
        LOG_ERR("Failed to register certificate: %d", ret);
        return ret;
    }

    // Setup socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
    if (sock < 0) {
        LOG_ERR("Failed to create HTTPS socket (%d)", -errno);
        return -errno;
    }

    // Configure TLS
    sec_tag_t sec_tag_list[] = {
        CA_CERTIFICATE_TAG,
    };

    ret = setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST,
                    sec_tag_list, sizeof(sec_tag_list));
    if (ret < 0) {
        LOG_ERR("Failed to set TLS_SEC_TAG_LIST option (%d)", -errno);
        close(sock);
        return -errno;
    }

    ret = setsockopt(sock, SOL_TLS, TLS_HOSTNAME,
                    TLS_PEER_HOSTNAME, sizeof(TLS_PEER_HOSTNAME));
    if (ret < 0) {
        LOG_ERR("Failed to set TLS_HOSTNAME option (%d)", -errno);
        close(sock);
        return -errno;
    }

    // Connect to Firebase
    memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(FIREBASE_PORT);
    
    ret = zsock_getaddrinfo(FIREBASE_HOST, NULL, NULL, (struct zsock_addrinfo **)&addr4.sin_addr);
    if (ret < 0) {
        LOG_ERR("Failed to resolve %s (%d)", FIREBASE_HOST, -errno);
        close(sock);
        return -errno;
    }

    ret = connect(sock, (struct sockaddr *)&addr4, sizeof(addr4));
    if (ret < 0) {
        LOG_ERR("Failed to connect to Firebase (%d)", -errno);
        close(sock);
        return -errno;
    }

    LOG_INF("Connected to Firebase");
    return sock;
}

static int send_data_to_firebase(void)
{
    int sock;
    int ret;
    int32_t timeout = 10 * MSEC_PER_SEC;
    
    // Connect to Firebase
    sock = connect_to_firebase();
    if (sock < 0) {
        return sock;
    }

    // Prepare JSON data to send
    const char *json_data = "{\"temperature\": 25.5, \"humidity\": 60}";
    
    // Prepare HTTP request
    struct http_request req;
    memset(&req, 0, sizeof(req));

    // For PUT request (to set data)
    req.method = HTTP_PUT;
    req.url = FIREBASE_DB_URL;
    req.host = FIREBASE_HOST;
    req.protocol = "HTTP/1.1";
    req.payload = json_data;
    req.payload_len = strlen(json_data);
    req.response = response_cb;
    req.recv_buf = recv_buf;
    req.recv_buf_len = sizeof(recv_buf);
    
    // Optional: Add authentication header if needed
    const char *headers[] = {
        "Content-Type: application/json\r\n",
        NULL
    };
    req.header_fields = headers;

    // Send request
    ret = http_client_req(sock, &req, timeout, "Firebase PUT");

    close(sock);
    return ret;
}

static int read_data_from_firebase(void)
{
    int sock;
    int ret;
    int32_t timeout = 10 * MSEC_PER_SEC;
    
    // Connect to Firebase
    sock = connect_to_firebase();
    if (sock < 0) {
        return sock;
    }

    // Prepare HTTP request for reading data
    struct http_request req;
    memset(&req, 0, sizeof(req));

    req.method = HTTP_GET;
    req.url = FIREBASE_DB_URL;
    req.host = FIREBASE_HOST;
    req.protocol = "HTTP/1.1";
    req.response = response_cb;
    req.recv_buf = recv_buf;
    req.recv_buf_len = sizeof(recv_buf);

    // Send request
    ret = http_client_req(sock, &req, timeout, "Firebase GET");

    close(sock);
    return ret;
}

int main(void)
{
    int ret;

    LOG_INF("HTTP Firebase Client Sample");
    
    // Connect to Wi-Fi
    LOG_INF("Connecting to Wi-Fi...");
    ret = wifi_connect();
    if (ret < 0) {
        LOG_ERR("Failed to connect to Wi-Fi: %d", ret);
        return ret;
    }
    LOG_INF("Connected to Wi-Fi");
    
    // Small delay to ensure network is ready
    k_sleep(K_SECONDS(2));

    // Send data to Firebase
    LOG_INF("Sending data to Firebase...");
    ret = send_data_to_firebase();
    if (ret < 0) {
        LOG_ERR("Failed to send data to Firebase: %d", ret);
        return ret;
    }
    
    // Read data from Firebase
    LOG_INF("Reading data from Firebase...");
    ret = read_data_from_firebase();
    if (ret < 0) {
        LOG_ERR("Failed to read data from Firebase: %d", ret);
        return ret;
    }
    
    LOG_INF("Firebase operations completed successfully");
    return 0;
}