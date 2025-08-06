#ifndef UDP_H
#define UDP_H

#include "platform.h"

void udp_client_task(void *pvParameters);
void udp_server_task(void *pvParameters);

void udp_sender_task(void *pvParameters);
void udp_receiver_task(void *pvParameters);

#endif // !UDP_H

