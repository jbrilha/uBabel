// proto_discovery.c
#include "comm_manager.h"
#include "udp.h"
#include "proto_manager.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "task.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"

#include <lwip/netif.h>
#include <lwip/init.h>
#include <lwip/etharp.h>

#include "pico/rand.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "message_parse.h"

#define DISCOVERY_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define DISCOVERY_UDP_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define DISCOVERY_MULTICAST_ADDR "239.255.255.250"
#define DISCOVERY_PORT 9100

#define TAG "comm_manager"

#define SUSPECT_NUMBER_OF_PERIODS 3
#define FAILED_NUMBER_OF_PERIODS 6
#define FORGET_NUMBER_OF_PERIODS 10

#define DISCOVERY_SIGNATURE_SIZE 4
#define DISCOVERY_SIGNATURE "mbma"

#define MAX_PROTOCOL_NAME_SIZE 38
#define MAX_UDP_PACKET_SIZE 1024 // 65535

#define EVENT_MESSAGE_ADDRESS_BOOK_QUERY 301
#define EVENT_MESSAGE_ADDRESS_BOOK_REPLY 302
#define EVENT_MESSAGE_DISCOVERY 304

#define INITIAL_PROTOCOL_CAPACITY 3

typedef enum peer_source
{
  DISCOVERY = 0,
  PROTOCOL = 1,
  MANUAL = 2
} peer_source_t;

typedef enum connection_status
{
  DISCONNECTED = 0,
  CONNECTING = 1,
  CONNECTED = 2,
  SUSPECT = 3,
  FAILED = 4,
  DEAD = 5
} connection_status_t;

typedef struct discovery_message
{
  struct sockaddr_in sender_addr;
  int messagelenght;
  char buffer[MAX_UDP_PACKET_SIZE];
} discovery_message_t;

typedef struct message
{
  uint8_t sender_id[UUID_SIZE];
  uint8_t destination_id[UUID_SIZE];
  uint16_t message_type;
  uint16_t message_size;
  void *payload;
  struct message *next;
} message_t;

static inline char *print_status(connection_status_t status)
{
  switch (status)
  {
  case DISCONNECTED:
    return "DISCONNECTED";
  case CONNECTING:
    return "CONNECTING";
  case CONNECTED:
    return "CONNECTED";
  case SUSPECT:
    return "SUSPECT";
  case FAILED:
    return "FAILED";
  case DEAD:
    return "DEAD";
  default:
    return "unknown";
  }
}

typedef enum device_type
{
  FULL_HOST = 0,
  RASPBERRY = 1,
  ZERO = 2,
  PICO = 3,
  ESP = 4,
  UNKNOWN = 5
} device_type_t;

typedef struct node_register
{
  uint8_t id[16]; // Storing an UUID
  uint16_t ips;
  ip4_addr_t *address;
  ip4_addr_t *active_ip;
  uint16_t port;
  uint32_t announce_period;
  uint32_t last_announce;
  uint32_t next_timeout;
  connection_status_t status;
  device_type_t device;
  int tcp_socket;
  uint16_t *protocols;
  uint16_t n_protocols;
  uint16_t n_protocols_capacity;
  struct node_register *next;
} node_register_t;

static node_register_t *address_book;

static QueueHandle_t proto_discovery_queue;
static int socket;
static discovery_message_t *msg = NULL;

static uint8_t my_id[16];

static SemaphoreHandle_t comm_mutex;

static int setup_tcp_socket()
{
  int socket = lwip_socket(AF_INET, SOCK_STREAM, 0);
  if (socket >= 0)
  {
    // Enable keepalive
    int keepalive = 1;
    if (lwip_setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0)
    {
      LOG_INFO(TAG, "Failed to set SO_KEEPALIVE");
    }

    // Optional: fine-tune keepalive behavior
    int idle = 5;  // idle time before sending first keepalive probe (seconds)
    int intvl = 5; // interval between probes (seconds)
    int cnt = 3;   // number of failed probes before considering dead

    lwip_setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    lwip_setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    lwip_setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));

    // Initialize the setup of non-blocking socket
    /*****************************************************
    int flags = lwip_fcntl(socket, F_GETFL, 0);
    if (flags < 0) {
      LOG_ERROR(TAG, "fcntl(F_GETFL) failed, errno=%d\n", errno);
    } else {
      // Set non-blocking
      if (lwip_fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR(TAG,"fcntl(F_SETFL) failed, errno=%d\n", errno);
      } else {
        LOG_INFO(TAG, "fcntl(F_SETFL) was executed with success to set socket behabiour as non-blocking");
      }
    }
    ******************************************************/
  }
  return socket;
}

static inline bool discovery_check_signature(const uint8_t *buf)
{
  return memcmp(buf, DISCOVERY_SIGNATURE, DISCOVERY_SIGNATURE_SIZE) == 0;
}

// Returns milliseconds since scheduler start
static inline uint32_t now_ms(void)
{
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

const char *uuid_to_string(uint8_t uuid[UUID_SIZE])
{
  static char out[16 * 2 + 5];
  memset(out, 0, 16 * 2 + 5);

  sprintf(out,
          "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
          uuid[0], uuid[1], uuid[2], uuid[3],
          uuid[4], uuid[5],
          uuid[6], uuid[7],
          uuid[8], uuid[9],
          uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

  return out;
}

const char *ipv4_to_str(const ip4_addr_t *ip)
{
  static char buf[16]; // "255.255.255.255" + '\0'
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
           ip4_addr1(ip),
           ip4_addr2(ip),
           ip4_addr3(ip),
           ip4_addr4(ip));
  return buf; // WARNING: Not thread-safe
}

static node_register_t *find_participant_by_id(void *buf_ptr)
{
  node_register_t *current = address_book;
  while (current != NULL)
  {
    if (memcmp(current->id, buf_ptr, UUID_SIZE) == 0)
    {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static node_register_t *register_new_participant_info(discovery_msg_t *msg)
{
  node_register_t *participant = (node_register_t *)malloc(sizeof(node_register_t));
  if (participant == NULL)
  {
    return NULL;
  }

  participant->address = (ip4_addr_t *)malloc(sizeof(ip4_addr_t) * msg->addr_count);
  if (participant->address == NULL)
  {
    free(participant);
    return NULL;
  }

  participant->protocols = malloc(sizeof(uint16_t) * INITIAL_PROTOCOL_CAPACITY);
  if (participant->protocols == NULL)
  {
    free(participant->address);
    free(participant);
    return NULL;
  }

  memcpy(participant->id, msg->uuid, UUID_SIZE);
  participant->ips = msg->addr_count;
  memcpy(participant->address, msg->addrs, sizeof(ip4_addr_t) * msg->addr_count);
  participant->active_ip = NULL;
  participant->port = msg->unicast_port;
  participant->announce_period = (uint32_t)msg->announce_period;
  participant->last_announce = now_ms();
  participant->next_timeout = participant->last_announce + participant->announce_period * SUSPECT_NUMBER_OF_PERIODS;
  participant->status = DISCONNECTED;
  participant->device = UNKNOWN;
  participant->tcp_socket = 0;
  participant->n_protocols = 0;
  participant->n_protocols_capacity = INITIAL_PROTOCOL_CAPACITY;
  participant->next = address_book;

  address_book = participant;
  return participant;
}

static void remove_participan_info(node_register_t *target)
{
  node_register_t **container = &address_book;

  while (*container != NULL)
  {
    if (*container == target)
    {
      *container = (*container)->next;
      if (target->address != NULL)
        free(target->address);
      if (target->protocols != NULL)
        free(target->protocols);
      if (target->tcp_socket >= 0)
        lwip_close(target->tcp_socket);
      free(target);
      return;
    }
    container = &((*container)->next);
  }
}

void print_participants_info()
{
  xSemaphoreTake(comm_mutex, portMAX_DELAY);
  node_register_t *current = address_book;

  u_int8_t count = 0;

  while (current != NULL)
  {
    count++;
    LOG_INFO(TAG, "Participant %d: %s :: Status: %s Active IP: %s", count, uuid_to_string(current->id), print_status(current->status), current->active_ip == NULL ? "null" : ipv4_to_str(current->active_ip));
    current = current->next;
  }
  xSemaphoreGive(comm_mutex);
}

static int initialize_udp_socket()
{
  int sock;
  struct sockaddr_in bind_addr;
  socklen_t slen = sizeof(bind_addr);

  sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
  {
    LOG_INFO(TAG, "failed to create UDP socket\n");
    return -1;
  }

  memset(&bind_addr, 0, slen);
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind_addr.sin_port = htons(DISCOVERY_PORT);

  if (lwip_bind(sock, (struct sockaddr *)&bind_addr, slen) < 0)
  {
    LOG_INFO(TAG, "failed to bind UDP socket\n");
    lwip_close(sock);
    return -1;
  }

  // Set multicast TTL
  int ttl = 1;
  if (lwip_setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
  {
    LOG_INFO(TAG, "Failed to set multicast TTL\n");
  }

  // Select outgoing interface for multicast
  if (netif_default != NULL)
  {
    struct in_addr ifaddr;
#if LWIP_IPV4
    ifaddr.s_addr = ip4_addr_get_u32(netif_ip4_addr(netif_default));
#else
    ifaddr.s_addr = htonl(INADDR_ANY);
#endif
    lwip_setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &ifaddr, sizeof(ifaddr));
  }

  struct ip_mreq mreq;
  memset(&mreq, 0, sizeof(mreq));
  ip_addr_t group_ip;

  if (!ipaddr_aton(DISCOVERY_MULTICAST_ADDR, &group_ip))
  {
    LOG_INFO(TAG, "invalid multicast addr\n");
    lwip_close(sock);
    return -1;
  }

  mreq.imr_multiaddr.s_addr = ip4_addr_get_u32(ip_2_ip4(&group_ip));
  // Use default netif’s IP if available; otherwise INADDR_ANY
  if (netif_default)
  {
    mreq.imr_interface.s_addr = ip4_addr_get_u32(netif_ip4_addr(netif_default));
  }
  else
  {
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  }

  if (lwip_setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                      &mreq, sizeof(mreq)) < 0)
  {
    // Don't trust perror; print SO_ERROR instead:
    int soerr = 0;
    socklen_t sl = sizeof(soerr);
    lwip_getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl);
    LOG_INFO(TAG, "IP_ADD_MEMBERSHIP failed, so_error=%d\n", soerr);
    lwip_close(sock);
    return -1;
  }
  else
  {
    char gbuf[16];
    const ip4_addr_t *g4 = ip_2_ip4(&group_ip);
    snprintf(gbuf, sizeof gbuf, "%u.%u.%u.%u",
             ip4_addr1(g4), ip4_addr2(g4), ip4_addr3(g4), ip4_addr4(g4));
    LOG_INFO(TAG, "Multicast joined on %s", gbuf);
  }

  return sock;
}

static bool receive_from_tcp(node_register_t *participant, uint8_t *ptr, uint16_t *to_receive)
{
  int r = 0;
  bool ret = true;

  while (*to_receive > 0)
  {
    r = lwip_recv(participant->tcp_socket, ptr, *to_receive, MSG_DONTWAIT);
    if (r == 0)
    {
      // Peer performed an orderly shutdown (FIN received)
      LOG_INFO(TAG, "Socket %d closed by peer\n", participant->tcp_socket);
      participant->status = DISCONNECTED;
      participant->active_ip = NULL;
      lwip_close(participant->tcp_socket);
      participant->tcp_socket = -1;
      ret = false;
      break;
    }
    else if (r < 0)
    {
      if (errno != EWOULDBLOCK && errno != EAGAIN)
      {
        // Real error
        LOG_INFO(TAG, "Socket %d error %d\n", participant->tcp_socket, errno);
        participant->status = DISCONNECTED;
        participant->active_ip = NULL;
        lwip_close(participant->tcp_socket);
        participant->tcp_socket = -1;
        ret = false;
        break;
      }
    }
    else
    {
      *to_receive -= r;
      ptr += r;
    }
  }

  if (ret == false)
  {
    uint8_t *id = (uint8_t *)malloc(UUID_SIZE);
    if (id != NULL)
    {
      memcpy(id, participant->id, UUID_SIZE);
      event_t *ev = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_FAILED, id, UUID_SIZE);
      if (ev != NULL)
      {
        ev->reference_counter++;
        for (uint16_t i = 0; i < participant->n_protocols; i++)
        {
          QueueHandle_t proto = find_protocol(participant->protocols[i]);
          if (proto != NULL && xQueueSend(proto, &ev, portMAX_DELAY) == pdPASS)
          {
            ev->reference_counter++;
          }
        }
        free_event(ev);
      }
    }
  }

  return ret;
}

static void handle_tcp_client(node_register_t *participant)
{
  uint16_t to_receive = 2;
  uint16_t msg_size = 0;
  uint16_t msg_code = 0;
  uint8_t *buffer;
  if (receive_from_tcp(participant, (uint8_t *)&msg_size, &to_receive))
  {
    LOG_INFO(TAG, "Received message with a total of %d bytes", msg_size);
    to_receive = 2;
    if (msg_size >= 2 && receive_from_tcp(participant, (uint8_t *)&msg_code, &to_receive))
    {
      LOG_INFO(TAG, "Received message with identifier: %d", msg_code);
      msg_size -= 2;
      if (msg_size > 0)
      {
        buffer = malloc(msg_size);
        if (buffer == 0)
        {
          // malloc failed
          return;
        }
        to_receive = msg_size;
        if (receive_from_tcp(participant, buffer, &to_receive))
        {
          event_t *event = create_event(EVENT_TYPE_MESSAGE, msg_code, buffer, msg_size);
          if (event == NULL)
          {
            free(buffer);
            return;
          }
          else
          {
            event_dispatcher_post(event);
          }
        }
        else
        {
          free(buffer);
        }
      }
    }
  }
  else
  {
  }
}

// Assumes mutex was acquird previously
static void attempt_to_extablish_connection(uint8_t *node_id)
{

  LOG_INFO(TAG, "Request to open connection to %s", uuid_to_string(node_id));
  node_register_t *target = find_participant_by_id(node_id);

  if (target != NULL)
  {

    if (target->status == CONNECTED)
    {
      return;
    }

    event_t *notification = NULL;
    uint8_t *id = (uint8_t *)malloc(UUID_SIZE);

    if (id == NULL)
    {
      return;
    }

    memcpy(id, target->id, UUID_SIZE);

    if (target->status == DISCONNECTED)
    {
      target->active_ip = target->address;
      target->status = CONNECTING;
    }
    else if (target->status == CONNECTING)
    {
      target->active_ip = target->active_ip + 1;
      lwip_close(target->tcp_socket);
      target->tcp_socket = -1;

      if (target->active_ip > target->address + (target->ips - 1))
      {
        // No more IPs addresses, move to disconnected and try again later
        target->active_ip = NULL;
        target->status = DISCONNECTED;
        notification = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_FAILED, id, UUID_SIZE);
      }
    }
    else
    {
      notification = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_FAILED, id, UUID_SIZE);
    }

    if (target->status == CONNECTING)
    {
      target->tcp_socket = setup_tcp_socket();

      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(target->port);
      server_addr.sin_addr.s_addr = ip4_addr_get_u32(target->active_ip);

      LOG_INFO(TAG, "attempting TCP connection to %s (%s:%d)", uuid_to_string(target->id), ipv4_to_str(target->active_ip), target->port);
      int result = lwip_connect(target->tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
      if (result == 0) // IMMEDIATLY CONNECTED
      {
        target->status = CONNECTED;
        notification = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_CONNECTED, id, UUID_SIZE);
      }
    }

    if (target->status != CONNECTED && target->status != CONNECTING)
    {
      notification = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_FAILED, node_id, UUID_SIZE);
    }

    // Idepenndely of the outcome, send notification if one exists :)
    if (notification != NULL)
    {
      notification->reference_counter++;
      for (uint16_t j = 0; j < target->n_protocols; j++)
      {
        QueueHandle_t proto = find_protocol(target->protocols[j]);
        if (proto != NULL && xQueueSend(proto, &notification, portMAX_DELAY) == pdPASS)
        {
          notification->reference_counter++;
        }
      }
      free_event(notification);
    }
  }
}

// Assumes mutex was acquird previously
static void attempt_to_close_connection(uint8_t *node_id)
{
  LOG_INFO(TAG, "Request to close connection to %s", uuid_to_string(node_id));

  node_register_t *target = find_participant_by_id(node_id);

  if (target != NULL && target->status == CONNECTED && target->n_protocols == 0)
  {
    lwip_close(target->tcp_socket);
    target->tcp_socket = -1;
    target->active_ip = NULL;
    target->status = DISCONNECTED;
  }
}

static void socket_manager_task(void *params)
{
  printf("[proto_discovery:] starting to receive multicast messages in IP: %s\n", (char *)params);

  // Start by cleaning up from a previous execution:
  if (socket >= 0)
  {
    lwip_close(socket);
    socket = -1;
  }

  if (msg != NULL)
  {
    free(msg);
    msg = NULL;
  }

  socket = initialize_udp_socket((char *)params);

  if (socket < 0)
  {
    printf("Failed to open socker (value is %d)\n", socket);
    vTaskDelete(NULL);
  }
  socklen_t slen;

  while (true)
  {

    print_participants_info();

    uint32_t now = now_ms();
    uint32_t next_deadline = UINT32_MAX;
    int highest_socket = socket;
    fd_set rfds;
    FD_ZERO(&rfds);
    xSemaphoreTake(comm_mutex, portMAX_DELAY);
    node_register_t *current = address_book;
    while (current != NULL)
    {
      if (current->status == CONNECTED)
      {
        FD_SET(current->tcp_socket, &rfds);
        if (current->tcp_socket > highest_socket)
          highest_socket = current->tcp_socket;
        LOG_INFO(TAG, "Added the tcp socket of %s:%d to rfds (highest socket: %d)", ipv4_to_str(current->active_ip), current->port, highest_socket);
      }
      current = current->next;
    }
    xSemaphoreGive(comm_mutex);

    FD_SET(socket, &rfds);
    LOG_INFO(TAG, "Added the udp socket to rfds (highest socket: %d)", highest_socket);

    highest_socket++;

    int n = 0;

    now = now_ms(); // current time (ms)
    uint32_t remaining = (next_deadline > now) ? (next_deadline - now) : 0;
    LOG_INFO(TAG, "Time now: %d; Next action: %d; Difference: %d", now, next_deadline, remaining);

    if (remaining > 1000)
    {
      remaining = 1000;
    }

    struct timeval tv;

    tv.tv_sec = remaining / 1000;
    tv.tv_usec = (remaining % 1000) * 1000;
    LOG_INFO(TAG, "Entering Select :: Timeout for select is of %d miliseconds (%d seconds and %d useconds)", remaining, tv.tv_sec, tv.tv_usec);
    n = lwip_select(highest_socket, &rfds, NULL, NULL, &tv);
    LOG_INFO(TAG, "Got out of select with %d active sockets", n);

    if (n > 0)
    {
      xSemaphoreTake(comm_mutex, portMAX_DELAY);
      // Check TCP connections
      current = address_book;
      ;
      while (current != NULL)
      {
        if (current->status == CONNECTED && FD_ISSET(current->tcp_socket, &rfds))
        {
          // Data was received in this RCP connection
          LOG_INFO(TAG, "processing message from %s:%d", ipv4_to_str(current->active_ip), current->port);
          handle_tcp_client(current);
        }
        current = current->next;
      }
      xSemaphoreGive(comm_mutex);

      // Check the UDP Socket
      if (FD_ISSET(socket, &rfds))
      {
        LOG_INFO(TAG, "processing message from the udp port");
        // UDP SOCKET RECEIVED SOMETHING - Potencially a multicast announcement.
        msg = malloc(sizeof(discovery_message_t));
        if (msg == NULL)
        {
          printf("[proto_discovery:] Could not allocate memory for a message buffer.");
          vTaskDelete(NULL);
        }
        slen = sizeof(msg->sender_addr);
        msg->messagelenght = lwip_recvfrom(socket, msg->buffer, MAX_UDP_PACKET_SIZE, 0,
                                           (struct sockaddr *)&(msg->sender_addr), &slen);
        if (msg->messagelenght >= 0)
        {
          event_t *ev = create_event(EVENT_TYPE_MESSAGE, EVENT_MESSAGE_DISCOVERY, msg, sizeof(discovery_message_t));
          ev->reference_counter++;
          xQueueSend(proto_discovery_queue, &ev, portMAX_DELAY);
        }
        else
        {
          int e = errno;
          printf("Recvfrom error: %d\n", e);
          free(msg);
        }
      }
    }

    // Now perform update of status for all active connections
    xSemaphoreTake(comm_mutex, portMAX_DELAY);
    now = now_ms();
    current = address_book;
    while (current != NULL)
    {
      switch (current->status)
      {
        case CONNECTED:
          //Nothing to be done, there is a connection active
          break;
        case CONNECTING:
          LOG_INFO(TAG, "Trying to resume the connection attempt to %s", uuid_to_string(&(current->id[0])));
          attempt_to_extablish_connection(&(current->id[0]));
        break;
        case DISCONNECTED:
          if(now > current->last_announce + SUSPECT_NUMBER_OF_PERIODS * current->announce_period)
          {
            current->status = SUSPECT;
            uint8_t* id = malloc(UUID_SIZE);
            if(id == NULL) 
            {
              LOG_ERROR(TAG, "Unable to reserve memory for node id for NOTIFICATION_NODE_SUSPECT");
            }
            else
            {
              memcpy(id, current->id, UUID_SIZE);
              event_t *e = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_SUSPECTED, id, UUID_SIZE);
              if (e == NULL)
              {
                LOG_ERROR(TAG, "Unable to allocate memory for event NOTIFICATION_NODE_SUSPECTED");
                free(id);
              }
              else
              {
                event_dispatcher_post(e);
              }
            }
          }
        break;
        case SUSPECT:
          if (now > current->last_announce + FAILED_NUMBER_OF_PERIODS * current->announce_period)
          {
            current->status = FAILED;
            uint8_t *id = malloc(UUID_SIZE);
            if (id == NULL)
            {
              LOG_ERROR(TAG, "Unable to reserve memory for node id for NOTIFICATION_NODE_FAILED");
            }
            else
            {
              memcpy(id, current->id, UUID_SIZE);
              event_t *e = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_FAILED, id, UUID_SIZE);
              if (e == NULL)
              {
                LOG_ERROR(TAG, "Unable to allocate memory for event NOTIFICATION_NODE_FAILED");
                free(id);
              }
              else
              {
                event_dispatcher_post(e);
              }
            }
          }
          break;
        case FAILED:
          if (now > current->last_announce + FORGET_NUMBER_OF_PERIODS * current->announce_period)
            current->status = DEAD;
        break;
        case DEAD:
          remove_participan_info(current);
        default:
          break;
      }

      current = current->next;
    }

    xSemaphoreGive(comm_mutex);
  }
}

static void handle_network_up_event(event_t *ev)
{
  if (ev->payload != NULL && ev->payload_size > 0)
    xTaskCreate(socket_manager_task, "proto_discovery_udp", DISCOVERY_UDP_TASK_STACK_SIZE, ((network_event_t *)ev->payload)->ip, 2, NULL);
  else
    xTaskCreate(socket_manager_task, "proto_discovery_udp", DISCOVERY_UDP_TASK_STACK_SIZE, NULL, 2, NULL);
}

static void handle_network_down_event(event_t *ev)
{
  xSemaphoreTake(comm_mutex, portMAX_DELAY);

  TaskHandle_t udpTask = xTaskGetHandle("proto_discovery_udp");
  if (udpTask != NULL)
    vTaskDelete(udpTask);

  // Destroy UDP socket
  lwip_close(socket);
  socket = -1;

  // Close TCP connections and remove all information about participants
  while (address_book != NULL)
  {
    node_register_t *current = address_book;
    if (current->status == CONNECTED || current->tcp_socket >= 0)
    {
      lwip_close(current->tcp_socket);
      current->tcp_socket = 0;
    }

    // Send notification for protocols of failed connections
    uint8_t *failed_node = NULL;
    event_t *failed_notification = NULL;

    failed_node = (uint8_t *)malloc(UUID_SIZE);
    if (failed_node != NULL)
    {
      memcpy(failed_node, current->id, UUID_SIZE);
      failed_notification = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_FAILED, failed_node, UUID_SIZE);

      if (failed_notification != NULL)
      {
        failed_notification->reference_counter++;

        for (uint16_t i = 0; i < current->n_protocols; i++)
        {
          QueueHandle_t proto = find_protocol(current->protocols[i]);
          if (proto != NULL)
          {
            failed_notification->reference_counter++;
            if (xQueueSend(proto, &failed_notification, portMAX_DELAY) != pdPASS)
              free_event(failed_notification);
          }
        }

        free_event(failed_notification);
      }
    }

    address_book = address_book->next;
    remove_participan_info(current);
  }

  xSemaphoreGive(comm_mutex);
}

static void update_participant_info(node_register_t *p, discovery_msg_t *msg)
{
  p->last_announce = now_ms();

  if (p->status == SUSPECT || p->status == FAILED || p->status == DEAD)
  {
    p->status = DISCONNECTED;
  }
}

static void processDiscoveryMessage(event_t *ev)
{
  char ip_str[INET_ADDRSTRLEN];

  // Convert IP to human-readable string
  inet_ntop(AF_INET, &(((discovery_message_t *)ev->payload)->sender_addr.sin_addr), ip_str, sizeof(ip_str));

  printf("Message from %s:%d | Length: %u bytes\n",
         ip_str,
         ntohs(((discovery_message_t *)ev->payload)->sender_addr.sin_port),
         (((discovery_message_t *)ev->payload)->messagelenght));

  if (((discovery_message_t *)ev->payload)->messagelenght > 0)
  {
    void *buf_ptr = ((discovery_message_t *)ev->payload)->buffer;
    short buf_remaining = ((discovery_message_t *)ev->payload)->messagelenght;

    discovery_msg_t msg;

    LOG_INFO(TAG, "Going to parse a message with %d bytes", buf_remaining);

    if (parse_discovery_message(buf_ptr, buf_remaining, &msg))
    {
      LOG_INFO(TAG, "The message is a propoer multicast annoucement: %s", discovery_check_signature(msg.sig) ? "true" : "false");
      LOG_INFO(TAG, "Received announcement from %s", uuid_to_string(msg.uuid));
      LOG_INFO(TAG, "Number of networks: %d", msg.addr_count);
      for (int i = 0; i < msg.addr_count; i++)
      {
        LOG_INFO(TAG, "IP %d: %s", i, ipv4_to_str(&msg.addrs[i]));
      }
      LOG_INFO(TAG, "Port value: %d", msg.unicast_port);
      LOG_INFO(TAG, "Periodicity in announcementes: %d\n", msg.announce_period);

      node_register_t *p = find_participant_by_id(msg.uuid);

      if (p == NULL)
      {
        p = register_new_participant_info(&msg);
        if (p != NULL)
        {
          uint8_t *id = malloc(UUID_SIZE);
          if (id == NULL)
          {
            LOG_ERROR(TAG, "Could not allocate space for node id on EVENT_NOTIFICAITON_NODE_DISCOVERED");
            return;
          }
          memcpy(id, p->id, UUID_SIZE);
          event_t *e = create_event(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_DISCOVERED, id, UUID_SIZE);
          if (e == NULL)
          {
            free(id);
            LOG_ERROR(TAG, "Could not allocate space for event EVENT_NOTIFICAITON_NODE_DISCOVERED");
            return;
          }
          LOG_INFO(TAG, "Emitting NOTIFICATION NODE DISCOVERED for %s", uuid_to_string(id));
          event_dispatcher_post(e);
        }
      }
      else
      {
        update_participant_info(p, &msg);
      }
    }
    else
    {
      LOG_INFO(TAG, "Failed to parse the message.");
    }
  }
  else
  {
    LOG_INFO(TAG, "Received message did not had the correct initial signature.");
  }
}

static void comm_manager_task(void *params)
{
  event_t *event;

  while (true)
  {
    if (xQueueReceive(proto_discovery_queue, &event, portMAX_DELAY) == pdPASS)
    {
      if (event->type == EVENT_TYPE_NOTIFICATION)
      {
        if (event->subtype == EVENT_SUBTYPE_NETWORK_UP)
        {
          handle_network_up_event(event);
        }
        else if (event->subtype == EVENT_SUBTYPE_NETWORK_DOWN)
        {
          handle_network_down_event(event);
        }
      }
      else if (event->type == EVENT_TYPE_MESSAGE && event->subtype == EVENT_MESSAGE_DISCOVERY)
      {
        processDiscoveryMessage(event);
      }
      else if (event->type == EVENT_TYPE_REQUEST)
      {
        if (event->subtype == EVENT_REQUEST_OPEN_CONNECTION)
        {
          LOG_INFO(TAG, "Trying to establish a connection to %s, pottentially for the first time.", uuid_to_string((uint8_t *)event->payload));
          xSemaphoreTake(comm_mutex, portMAX_DELAY);
          attempt_to_extablish_connection((uint8_t *)event->payload);
          xSemaphoreGive(comm_mutex);
        }
        else if (event->subtype == EVENT_REQUEST_CLOSE_CONNECTION)
        {
          xSemaphoreTake(comm_mutex, portMAX_DELAY);
          attempt_to_close_connection((uint8_t *)event->payload);
          xSemaphoreGive(comm_mutex);
        }
        else if (event->subtype == EVENT_REQUEST_ADD_CONNECTION)
        {
          LOG_INFO(TAG, "Request to add connection to %s", uuid_to_string(event->payload));
        }
      }
    }

    free_event(event);
    event = NULL;
  }
}

void get_mac_address(uint8_t mac[6])
{
  struct netif *interface = netif_list;

  while (interface != NULL)
  {
    if (netif_is_link_up(interface) && (interface->flags & NETIF_FLAG_ETHARP))
    {
      memcpy(mac, interface->hwaddr, interface->hwaddr_len);
      return;
    }
    interface = interface->next;
  }
}

void generate_random_uuid(uint8_t uuid[16])
{
  uint8_t mac[6];
  get_mac_address(mac);

  // First 6 bytes from MAC
  memcpy(uuid, mac, 6);

  // Remaining 10 bytes from hardware RNG
  for (int i = 6; i < 16; i += 4)
  {
    uint32_t r = get_rand_32(); // from hardware_random.h
    memcpy(&uuid[i], &r, (i + 4 <= 16) ? 4 : (16 - i));
  }

  // Make it RFC 4122 version 4 UUID:
  uuid[6] = (uuid[6] & 0x0F) | 0x40; // version 4
  uuid[8] = (uuid[8] & 0x3F) | 0x80; // variant RFC 4122
}

bool comm_manager_init(void)
{
  socket = -1;

  generate_random_uuid(my_id);

  comm_mutex = xSemaphoreCreateMutex();

  // Initialize the discovery queue
  proto_discovery_queue = xQueueCreate(10, sizeof(event_t *));

  if (!proto_discovery_queue)
  {
    LOG_INFO(TAG, "Failed to create discovery queue.\n");
    return false;
  }

  // Initialize the registered interest linked list
  address_book = NULL;

  if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP))
  {
    LOG_INFO(TAG, "Failed to register discovery task with event dispatcher.");
    return false;
  }

  if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN))
  {
    LOG_INFO(TAG, "Failed to register discovery task with event dispatcher.");
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP);
    return false;
  }

  if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_OPEN_CONNECTION))
  {
    LOG_INFO(TAG, "Failed to register event request open conneciton with event dispatcher.");
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP);
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN);
    return false;
  }

  if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_CLOSE_CONNECTION))
  {
    LOG_INFO(TAG, "Failed to register event request clone conneciton with event dispatcher.");
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP);
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN);
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_OPEN_CONNECTION);
    return false;
  }

  if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_ADD_CONNECTION))
  {
    LOG_INFO(TAG, "Failed to register event request clone conneciton with event dispatcher.");
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP);
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN);
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_OPEN_CONNECTION);
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_REQUEST, EVENT_REQUEST_CLOSE_CONNECTION);
    return false;
  }

  xTaskCreate(comm_manager_task, "comm_manager_task", DISCOVERY_TASK_STACK_SIZE, NULL, 2, NULL);
  LOG_INFO(TAG, "Initialized");
  return true;
}

bool open_conection(const uint8_t *destination_id, uint16_t proto_id)
{
  bool answer = false;
  LOG_INFO(TAG, "Request to open connection %s", uuid_to_string((uint8_t *)destination_id));

  xSemaphoreTake(comm_mutex, portMAX_DELAY);

  node_register_t *node = find_participant_by_id((uint8_t *)destination_id);
  if (node != NULL)
  {
    if (node->n_protocols == node->n_protocols_capacity)
    {
      uint16_t *new_protos = malloc(node->n_protocols_capacity + INITIAL_PROTOCOL_CAPACITY);
      if (new_protos == NULL)
      {
        LOG_ERROR(TAG, "Can not extend the list of protocolos connected to a node.");
        xSemaphoreGive(comm_mutex);
        return false;
      }

      memcpy(new_protos, node->protocols, sizeof(uint16_t) * node->n_protocols);
      free(node->protocols);
      node->protocols = new_protos;
    }

    node->protocols[node->n_protocols] = proto_id;
    node->n_protocols++;

    uint8_t *id = NULL;
    event_t *ev = NULL;

    if (node->status != CONNECTED)
    {
      id = (uint8_t *)malloc(UUID_SIZE);
      if (id != NULL)
      {
        memcpy(id, destination_id, UUID_SIZE);
        ev = create_event(EVENT_TYPE_REQUEST, EVENT_REQUEST_OPEN_CONNECTION, id, UUID_SIZE);
        ev->proto_source = proto_id;
        if (xQueueSend(proto_discovery_queue, &ev, portMAX_DELAY) != pdPASS)
        {
          free_event(ev);
          xSemaphoreGive(comm_mutex);
          return false;
        }
      }
    }
    else
    {
      QueueHandle_t requesterQueue = find_protocol(proto_id);
      if (requesterQueue != NULL)
      {
        id = (uint8_t *)malloc(UUID_SIZE);
        if (id != NULL)
        {
          memcpy(id, destination_id, UUID_SIZE);
          ev = create_event_with_destination(EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_CONNECTED, id, UUID_SIZE, proto_id);
          if (ev == NULL)
          {
            free(id);
          }
          else
          {
            ev->reference_counter++;
            ev->proto_source = proto_id;
            if (xQueueSend(requesterQueue, &ev, portMAX_DELAY) != pdPASS)
            {
              free_event(ev);
              xSemaphoreGive(comm_mutex);
              return true;
            }
          }
        }
      }
    }
  }

  xSemaphoreGive(comm_mutex);
  return false;
}

void close_conection(const uint8_t *destination_id, uint16_t proto_id)
{
  LOG_INFO(TAG, "Request to close connection %s", uuid_to_string((uint8_t *)destination_id));
  xSemaphoreTake(comm_mutex, portMAX_DELAY);

  node_register_t *node = find_participant_by_id((uint8_t *)destination_id);
  if (node != NULL)
  {
    event_t *ev = NULL;
    uint8_t *id = (uint8_t *)malloc(UUID_SIZE);
    if (id != NULL)
    {
      memcpy(id, destination_id, UUID_SIZE);
    }

    for (uint16_t p = 0; p < node->n_protocols; p++)
    {
      if (node->protocols[p] == proto_id)
      {
        for (; p < node->n_protocols - 1; p++)
        {
          node->protocols[p] = node->protocols[p + 1];
        }
        node->n_protocols--;
        break;
      }
    }

    if (node->n_protocols <= 00 && id != NULL)
    {
      ev = create_event(EVENT_TYPE_REQUEST, EVENT_REQUEST_CLOSE_CONNECTION, id, UUID_SIZE);
      if (ev != NULL && xQueueSend(proto_discovery_queue, &ev, portMAX_DELAY) != pdPASS)
      {
        ev->proto_source = proto_id;
        ev->reference_counter++;
        free_event(ev);
      }
      else
      {
        free(id);
      }
    }
  }
  xSemaphoreGive(comm_mutex);
}

bool send_message(event_t *msg, const uint8_t *destination_id)
{
  xSemaphoreTake(comm_mutex, portMAX_DELAY);
  LOG_INFO(TAG, "Request to send message to %s", uuid_to_string((uint8_t *)destination_id));
  msg->reference_counter++;
  if(xQueueSend(proto_discovery_queue, &msg, portMAX_DELAY) != pdPASS) {
    free_event(msg);
  }
  xSemaphoreGive(comm_mutex);
}

bool send_message_multiple(event_t *msg, const uint8_t **destinations, uint8_t n_destinations)
{
  xSemaphoreTake(comm_mutex, portMAX_DELAY);
  LOG_INFO(TAG, "Request to send multiple messages to:");
  for (uint8_t i = 0; i < n_destinations; i++)
  {
    LOG_INFO(TAG, "    %s", uuid_to_string((uint8_t *)destinations[i]));
  }

  xSemaphoreGive(comm_mutex);
}

void get_local_identifier(uint8_t *id)
{
  memcpy(id, my_id, UUID_SIZE);
}