// proto_discovery.c
#include "proto_discovery.h"
#include "udp.h"
#include "event.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "task.h"

#include <string.h>
#include <stdio.h>

#define DISCOVERY_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define DISCOVERY_UDP_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define DISCOVERY_MULTICAST_ADDR "233.138.122.123"
#define DISCOVERY_PORT 1025

typedef struct discoverable_protocol {
  char protocol_name[MAX_PROTOCOL_NAME_SIZE + 1]; //Null terminated protocol name
  uint32_t ip;
  uint16_t port;
  QueueHandle_t queue;
  struct discoverable_protocol* next;
} discoverable_proto_t;

static QueueHandle_t proto_discovery_queue;
static discoverable_proto_t* registered_interest;

static int socket;
static discovery_message_t* msg = NULL;



static bool proto_discovery_register_interest(char* proto_name, uint32_t ip, uint16_t port, QueueHandle_t queue) {
    discoverable_proto_t* newRegister = malloc(sizeof(discoverable_proto_t));
    if(newRegister = NULL) {
      return false;
    }
    
    memset(newRegister->protocol_name, 0, MAX_PROTOCOL_NAME_SIZE+1);
    strncpy(newRegister->protocol_name, proto_name, MAX_PROTOCOL_NAME_SIZE);
    newRegister->ip = ip;
    newRegister->port = port;
    newRegister->queue = queue;
    newRegister->next = registered_interest;
    registered_interest = newRegister;


    return true;
}

static bool proto_discovery_unregister_interest(char* proto_name, uint32_t ip, uint16_t port, QueueHandle_t queue) {  
  discoverable_proto_t** current = &registered_interest;
  while(*current != NULL) {
    if(
      strcmp((*current)->protocol_name, proto_name) == 0 &&
      (*current)->ip == ip && (*current)->port == port &&
      (*current)->queue == queue
     ) {
      discoverable_proto_t* toRelease = *current;
      *current = (*current)->next;
      free(toRelease);
      return true;
     } else 
      current = &((*current)->next);
  }
  return false;
}


static int initialize_udp_socket(char* local_ip) {
    int sock;
    struct sockaddr_in bind_addr;
    socklen_t slen = sizeof(bind_addr);

    sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_INFO(TAG, "failed to create UDP socket\n");
        return -1;
    }

    memset(&bind_addr, 0, slen);
    bind_addr.sin_family = AF_INET;
    ip_addr_t ip;
    if(local_ip != NULL) {
      ipaddr_aton(local_ip, &ip);
      bind_addr.sin_addr.s_addr = ip.addr;
    } else {
      bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    bind_addr.sin_port = htons(DISCOVERY_PORT);

    if (lwip_bind(sock, (struct sockaddr *)&bind_addr, slen) < 0) {
        LOG_INFO(TAG, "failed to bind UDP socket\n");
        lwip_close(sock);
        return -1;
    }

    struct ip_mreq mreq;
    ipaddr_aton(DISCOVERY_MULTICAST_ADDR, &ip);  
    bind_addr.sin_addr.s_addr = ip.addr;   // Multicast group
    mreq.imr_interface.s_addr = ip4_addr_get_u32(netif_ip4_addr(netif_default)); // Use default netif IP

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt IP_ADD_MEMBERSHIP");
        close(sock);
        return -1;
    }   

    return sock;
}

static void udp_forwarder_task(void* params) {
    printf("[proto_discovery:] starting to receive multicast messages in IP: %s\n", (char*) params);
    
    //Start by cleaning up from a previous execution:
    if(socket != 0) {
      close(socket);
      socket = 0;
    }

    if(msg != NULL) {
      free(msg);
      msg = NULL;
    }

    socket = initialize_udp_socket((char*) params);
    socklen_t slen;

    while (true) {
        msg = malloc(sizeof(discovery_message_t));
        if(msg == NULL) {
          printf("[proto_discovery:] Could not allocate memory for a message buffer.");
          vTaskDelete(NULL);
        }
        slen = sizeof(msg->sender_addr);
        msg->messagelenght = lwip_recvfrom(socket, msg->buffer, MAX_UDP_PACKET_SIZE, 0,
                            (struct sockaddr *)&(msg->sender_addr), &slen);
        event_t* ev = create_event(EVENT_TYPE_MESSAGE, EVENT_MESSAGE_DISCOVERY, msg, sizeof(msg));
        ev->reference_counter++;
        xQueueSend(proto_discovery_queue, &ev, portMAX_DELAY);
    }
}

static void handle_network_up_event(event_t * ev) {
  if(ev->payload != NULL && ev->payload_size > 0)
    xTaskCreate(udp_forwarder_task, "proto_discovery_udp", DISCOVERY_UDP_TASK_STACK_SIZE, ((network_event_t*)ev->payload)->ip, 2, NULL);
  else
    xTaskCreate(udp_forwarder_task, "proto_discovery_udp", DISCOVERY_UDP_TASK_STACK_SIZE, NULL, 2, NULL);
}

static void handle_network_down_event(event_t* ev) {
  TaskHandle_t udpTask = xTaskGetHandle("proto_discovery_udp");
  if(udpTask != NULL)
    vTaskDelete(udpTask);
}

static void processDiscoveryMessage(event_t* ev) {

}

static void proto_discovery_task(void* params) {
    event_t* event;

    while (true) {
        if (xQueueReceive(proto_discovery_queue, &event, portMAX_DELAY) == pdPASS) {
            if(event->type == EVENT_TYPE_NOTIFICATION) {
              if(event->subtype == EVENT_SUBTYPE_NETWORK_UP) {
                handle_network_up_event(event);
              } else if (event->subtype == EVENT_SUBTYPE_NETWORK_DOWN) {
                handle_network_down_event(event);
              }
            } else if (event->type == EVENT_TYPE_REQUEST) {
              if(event->subtype == EVENT_REQUEST_DISCOVERY_REGISTER) {
                if(event->payload_size > 0 && event->payload != NULL) {
                  register_proto_info_t* info = (register_proto_info_t*) event->payload;
                  proto_discovery_register_interest(info->protocol_name, info->ip, info->port, info->queue);
                }
              } else if(event->subtype == EVENT_REQUEST_DISCOVERY_UNREGISTER) {
                if(event->payload_size > 0 && event->payload != NULL) {
                  register_proto_info_t* info = (register_proto_info_t*) event->payload;
                  proto_discovery_unregister_interest(info->protocol_name, info->ip, info->port, info->queue);
                }
              }
            } else if (event->type == EVENT_TYPE_MESSAGE && event->subtype == EVENT_MESSAGE_DISCOVERY) {
                processDiscoveryMessage(event);
                free_event(event);
            }
        }

        free_event(event);
        event = NULL;
    }
}

bool proto_discovery_init(void) {

    // Initialize the discovery queue
    proto_discovery_queue = xQueueCreate(10, sizeof(event_t*));

    if (!proto_discovery_queue) {
        printf("[proto_discovery] Failed to create discovery queue.\n");
        return false;
    } 

    // Initialize the registered interest linked list
    registered_interest = NULL;

    // Register the discovery task with the event dispatcher
    if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_DISCOVERY_REGISTER)) {
        printf("[proto_discovery] Failed to register discovery task with event dispatcher.\n");
        return false;
    }
    if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_DISCOVERY_UNREGISTER)) {
        printf("[proto_discovery] Failed to register discovery task with event dispatcher.\n");
        event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_DISCOVERY_REGISTER);
        return false;
    }

    if(!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP)) {
        printf("[proto_discovery] Failed to register discovery task with event dispatcher.\n");
        event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_DISCOVERY_REGISTER);
        event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_DISCOVERY_UNREGISTER);
    }

    if(!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN)) {
        printf("[proto_discovery] Failed to register discovery task with event dispatcher.\n");
        event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_DISCOVERY_REGISTER);
        event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_DISCOVERY_UNREGISTER);
        event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN);
    }

    xTaskCreate(proto_discovery_task, "proto_discovery_task", DISCOVERY_TASK_STACK_SIZE, NULL, 2, NULL);
    printf("[proto_discovery] Initialized");
}
