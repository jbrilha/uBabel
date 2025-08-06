#ifndef TCP_H
#define TCP_H

#include "platform.h"
#include <lwip/sockets.h>

typedef struct {
    int sock;
} tcp_context_t;

typedef struct {
    int sock;
    struct sockaddr_in client_addr;
} tcp_client_context_t;

void tcp_client_task(void *pvParameters);
void tcp_server_task(void *pvParameters);

void tcp_sender_task(void *pvParameters);
void tcp_receiver_task(void *pvParameters);

#endif // !TCP_H

