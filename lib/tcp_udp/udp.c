#include "udp.h"

#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <stdio.h>
#include <string.h>

#define UDP_PORT 8080

#define UDP_RECEIVER_TASK_STACK_SIZE 2048
#define UDP_SENDER_TASK_STACK_SIZE 2048

// higher numbers are higher priority
#define UDP_RECEIVER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)
#define UDP_SENDER_TASK_PRIORITY (tskIDLE_PRIORITY + 3UL)

#define ESP32_IP "192.168.4.1" // default ESP32 IP

#define MAX_PEERS 5
#define MAX_MSG_SIZE 128

typedef struct {
    struct sockaddr_in addr;
    uint32_t last_seen;
} peer_t;

typedef struct {
    peer_t peers[MAX_PEERS];
    uint8_t count;
    SemaphoreHandle_t mutex;
} active_peers_t;

active_peers_t active_peers;

// this one should only (!!) be called in betwen taking and giving the mutex
static bool peer_exists_unsafe(const struct sockaddr_in *peer_addr) {
    peer_t peer;
    for (int i = 0; i < active_peers.count; i++) {
        peer = active_peers.peers[i];

        if (peer.addr.sin_addr.s_addr == peer_addr->sin_addr.s_addr &&
            peer.addr.sin_port == peer_addr->sin_port) {
            return true;
        }
    }

    return false;
}

bool peer_exists(const struct sockaddr_in *peer_addr) {
    bool exists = false;

    if (xSemaphoreTake(active_peers.mutex, pdMS_TO_TICKS(100))) { // 100 ms
        exists = peer_exists_unsafe(peer_addr);
        xSemaphoreGive(active_peers.mutex);
    }

    return exists;
}

bool add_peer(const struct sockaddr_in *peer_addr) {
    bool res = false;

    if (xSemaphoreTake(active_peers.mutex, pdMS_TO_TICKS(100))) { // 100 ms
        if (peer_exists_unsafe(peer_addr)) {
            res = true;
            LOG_INFO(TAG, "peer already added");
        } else if (active_peers.count < MAX_PEERS) {

            active_peers.peers[active_peers.count] = (peer_t){
                .addr = *peer_addr,
                .last_seen = xTaskGetTickCount(),
            };

            active_peers.count++;
            res = true;
            LOG_INFO(TAG, "added peer");
        }

        xSemaphoreGive(active_peers.mutex);
    }

    return res;
}

void udp_receiver_task(void *pvParameters) {
    LOG_INFO(TAG, "UDP receiver task started\n");

    int sock = *(int *)pvParameters;

    struct sockaddr_in sender_addr;
    socklen_t slen = sizeof(sender_addr);

    char rx_buf[MAX_MSG_SIZE];

    int len;
    while (true) {
        len = lwip_recvfrom(sock, rx_buf, sizeof(rx_buf) - 1, 0,
                            (struct sockaddr *)&sender_addr, &slen);

        if (len > 0) {
            rx_buf[len] = '\0';
            char sender_ip[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip,
                      INET_ADDRSTRLEN);

            LOG_INFO(TAG, "UDP RX [%s:%d]: %s", sender_ip,
                     ntohs(sender_addr.sin_port), rx_buf);

            if (!add_peer(&sender_addr)) {
                LOG_WARN(TAG, "failed to add peer");
            }
        } else if (len < 0) {
            LOG_WARN(TAG, "UDP recvfrom error: %d\n", len);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    lwip_close(sock);
}

void udp_sender_task(void *pvParameters) {
    LOG_INFO(TAG, "UDP sender task started\n");

    int sock = *(int *)pvParameters;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_aton(ESP32_IP, &server_addr.sin_addr);

    int msg_count = 0;
    while (true) {
        char msg[MAX_MSG_SIZE];
        snprintf(msg, sizeof(msg), "UDP Hello from %s #%d", TAG, msg_count++);

        int sent =
            lwip_sendto(sock, msg, strlen(msg), 0,
                        (struct sockaddr *)&server_addr, sizeof(server_addr));

        if (sent > 0) {
            LOG_INFO(TAG, "UDP sent: %s\n", msg);
        } else {
            LOG_INFO(TAG, "UDP send failed: %d\n", sent);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    lwip_close(sock);
}

void udp_peer_sender_task(void *pvParameters) {
    LOG_INFO(TAG, "UDP peer sender task started\n");

    int sock = *(int *)pvParameters;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_aton(ESP32_IP, &server_addr.sin_addr);

    int msg_count = 0;
    while (true) {
        char msg[64];
        snprintf(msg, sizeof(msg), "UDP Hello from %s to peer #%d", TAG,
                 msg_count++);

        peer_t peers_copy[MAX_PEERS];
        int peer_count = 0;

        if (xSemaphoreTake(active_peers.mutex, pdMS_TO_TICKS(50))) {
            peer_count = active_peers.count;
            memcpy(peers_copy, active_peers.peers, sizeof(peer_t) * peer_count);
            xSemaphoreGive(active_peers.mutex);
        }

        // send without holding mutex
        for (int i = 0; i < peer_count; i++) {
            if (sendto(sock, msg, strlen(msg), 0,
                       (struct sockaddr *)&peers_copy[i].addr,
                       sizeof(peers_copy[i].addr)) > 0) {
                LOG_INFO(TAG, "UDP sent: %s", msg);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void udp_server_task(void *pvParameters) {
    LOG_INFO(TAG, "UDP server task started\n");

    int sock;
    struct sockaddr_in bind_addr;
    socklen_t slen = sizeof(bind_addr);

    sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_INFO(TAG, "failed to create UDP socket\n");
        vTaskDelete(NULL);
        return;
    }

    memset(&bind_addr, 0, slen);
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(UDP_PORT);

    if (lwip_bind(sock, (struct sockaddr *)&bind_addr, slen) < 0) {
        LOG_INFO(TAG, "failed to bind UDP socket\n");
        lwip_close(sock);
        vTaskDelete(NULL);
        return;
    }

    LOG_INFO(TAG, "UDP server bound to port %d\n", UDP_PORT);

    active_peers.count = 0;
    memset(active_peers.peers, 0, sizeof(active_peers.peers));
    active_peers.mutex = xSemaphoreCreateMutex();
    if (active_peers.mutex == NULL) {
        LOG_ERROR(TAG, "failed to create peers mutex");
        return;
    }

    xTaskCreate(udp_peer_sender_task, "udp_tx", UDP_SENDER_TASK_STACK_SIZE,
                &sock, UDP_SENDER_TASK_PRIORITY, NULL);
    xTaskCreate(udp_receiver_task, "udp_rx", UDP_RECEIVER_TASK_STACK_SIZE,
                &sock, UDP_RECEIVER_TASK_PRIORITY, NULL);

    vTaskDelete(NULL);
}

void udp_client_task(void *pvParameters) {
    LOG_INFO(TAG, "UDP client task started\n");

    int sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_INFO(TAG, "failed to create UDP socket\n");
        vTaskDelete(NULL);
        return;
    }

    active_peers.count = 0;
    memset(active_peers.peers, 0, sizeof(active_peers.peers));
    active_peers.mutex = xSemaphoreCreateMutex();
    if (active_peers.mutex == NULL) {
        LOG_ERROR(TAG, "failed to create peers mutex");
        return;
    }

    xTaskCreate(udp_sender_task, "udp_tx", UDP_SENDER_TASK_STACK_SIZE, &sock,
                UDP_SENDER_TASK_PRIORITY, NULL);

    xTaskCreate(udp_receiver_task, "udp_rx", UDP_RECEIVER_TASK_STACK_SIZE,
                &sock, UDP_RECEIVER_TASK_PRIORITY, NULL);

    vTaskDelete(NULL);
}
