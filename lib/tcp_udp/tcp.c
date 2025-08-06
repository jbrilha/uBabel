#include "tcp.h"

#include <stdio.h>
#include <string.h>

#include "lwip/sockets.h"

#define TCP_PORT 8081
#define ESP32_IP "192.168.4.1" // default

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define SCROLL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define WORKER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define MAX_MSG_SIZE 256

#define TCP_RECEIVER_TASK_STACK_SIZE 4096
#define TCP_SENDER_TASK_STACK_SIZE 2048
#define TCP_RECEIVER_TASK_PRIORITY 4
#define TCP_SENDER_TASK_PRIORITY 3

void tcp_receiver_task(void *pvParameters) {
    LOG_INFO(TAG, "TCP receiver task started\n");

    tcp_context_t *ctx = (tcp_context_t *)pvParameters;
    int sock = ctx->sock;
    char rx_buf[MAX_MSG_SIZE];
    int len;

    while (true) {
        len = lwip_recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
        if (len > 0) {
            rx_buf[len] = '\0';
            LOG_INFO(TAG, "TCP RX: %s\n", rx_buf);
        } else if (len == 0) {
            LOG_INFO(TAG, "TCP connection closed\n");
            break;
        } else {
            LOG_INFO(TAG, "TCP RX error: %d\n", len);
            break;
        }
    }

    lwip_close(sock);
    free(ctx);
    vTaskDelete(NULL);
}

void tcp_sender_task(void *pvParameters) {
    LOG_INFO(TAG, "TCP sender task started\n");

    tcp_context_t *ctx = (tcp_context_t *)pvParameters;
    int sock = ctx->sock;

    int msg_count = 0;
    while (true) {
        char msg[MAX_MSG_SIZE];
        snprintf(msg, sizeof(msg), "TCP Hello from %s #%d", TAG, msg_count++);

        if (lwip_send(sock, msg, strlen(msg), 0) < 0) {
            LOG_INFO(TAG, "TCP send failed\n");
            break;
        }

        LOG_INFO(TAG, "TCP sent: %s\n", msg);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    free(ctx);
    vTaskDelete(NULL);
}

void tcp_client_task(void *pvParameters) {
    LOG_INFO(TAG, "TCP client task started\n");

    int sock = -1;
    bool connected = false;

    while (true) {
        if (sock < 0 || !connected) {
            sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                LOG_INFO(TAG, "failed to create TCP socket\n");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }

            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(TCP_PORT);
            inet_aton(ESP32_IP, &server_addr.sin_addr);

            LOG_INFO(TAG, "attempting TCP connection to ESP32...\n");
            if (lwip_connect(sock, (struct sockaddr *)&server_addr,
                             sizeof(server_addr)) < 0) {
                LOG_INFO(TAG, "TCP connection failed\n");

                lwip_close(sock);
                sock = -1;
                connected = false;

                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }

            LOG_INFO(TAG, "TCP connected to ESP32\n");

            tcp_context_t *rx_ctx =
                (tcp_context_t *)malloc(sizeof(tcp_context_t));
            tcp_context_t *tx_ctx =
                (tcp_context_t *)malloc(sizeof(tcp_context_t));

            if (!rx_ctx || !tx_ctx) {
                LOG_INFO(TAG, "failed to allocate TCP context\n");
                if (rx_ctx)
                    free(rx_ctx);
                if (tx_ctx)
                    free(tx_ctx);

                lwip_close(sock);
                sock = -1;
                connected = false;

                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }

            connected = true;

            rx_ctx->sock = sock;
            tx_ctx->sock = sock;

            xTaskCreate(tcp_receiver_task, "tcp_rx", 1024, rx_ctx,
                        WORKER_TASK_PRIORITY, NULL);
            xTaskCreate(tcp_sender_task, "tcp_tx", 1024, tx_ctx,
                        WORKER_TASK_PRIORITY, NULL);
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    vTaskDelete(NULL);
}

void tcp_server_task(void *pvParameters) {
    int listen_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    listen_sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        LOG_ERROR(TAG, "failed to create TCP socket");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        LOG_ERROR(TAG, "failed to bind TCP socket");
        lwip_close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 1) < 0) {
        LOG_ERROR(TAG, "failed to listen on TCP socket");
        lwip_close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    LOG_INFO(TAG, "TCP server listening on port 8081");

    while (true) {
        client_sock = lwip_accept(listen_sock, (struct sockaddr *)&client_addr,
                                  &client_len);
        if (client_sock < 0) {
            LOG_ERROR(TAG, "TCP accept failed");
            continue;
        }

        tcp_client_context_t *rx_ctx =
            (tcp_client_context_t *)malloc(sizeof(tcp_client_context_t));
        tcp_client_context_t *tx_ctx =
            (tcp_client_context_t *)malloc(sizeof(tcp_client_context_t));

        if (!rx_ctx || !tx_ctx) {
            LOG_ERROR(TAG, "Failed to allocate client context");
            if (rx_ctx)
                free(rx_ctx);
            if (tx_ctx)
                free(tx_ctx);
            close(client_sock);
            continue;
        }

        rx_ctx->sock = client_sock;
        rx_ctx->client_addr = client_addr;
        tx_ctx->sock = client_sock;
        tx_ctx->client_addr = client_addr;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        LOG_INFO(TAG, "TCP client connected: %s:%d", client_ip,
                 ntohs(client_addr.sin_port));

        xTaskCreate(tcp_receiver_task, "tcp_rx", TCP_RECEIVER_TASK_STACK_SIZE,
                    rx_ctx, TCP_RECEIVER_TASK_PRIORITY, NULL);
        xTaskCreate(tcp_sender_task, "tcp_tx", TCP_RECEIVER_TASK_STACK_SIZE,
                    tx_ctx, TCP_SENDER_TASK_PRIORITY, NULL);
    }
}
