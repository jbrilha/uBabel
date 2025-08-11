// proto_discovery.c
#include "proto_discovery.h"
#include "udp.h"
#include "event.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "task.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "discovery_parse.h"

#define DISCOVERY_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define DISCOVERY_UDP_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define DISCOVERY_MULTICAST_ADDR "239.255.255.250"
#define DISCOVERY_PORT 9100

#define TAG "proto_discovery"

#define SUSPECT_NUMBER_OF_PERIODS 3
#define FAILED_NUMBER_OF_PERIODS 6
#define FORGET_NUMBER_OF_PERIODS 10

#define DISCOVERY_SIGNATURE_SIZE 4
#define DISCOVERY_SIGNATURE "mbma"
#define UUID_SIZE 16

typedef enum connection_status
{
  DISCONNECTED = 0,
  CONNECTING = 1,
  CONNECTED = 2,
  SUSPECT = 3,
  FAILED = 4,
  DEAD = 5
} connection_status_t;

static inline char* print_status(connection_status_t status) {
  switch(status) {
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

typedef struct participant_register
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
  bool connected;
  struct participant_register *next;
} participant_register_t;

static participant_register_t *detected_nodes;

static QueueHandle_t proto_discovery_queue;
static int socket;
static discovery_message_t *msg = NULL;


static int setup_tcp_socket() {
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

static const char *uuid_to_string(uint8_t uuid[16])
{
  static char out[16 * 2 + 5];

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

static participant_register_t *find_participant_by_id(void *buf_ptr)
{
  participant_register_t *current = detected_nodes;
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

static void update_participant_info(participant_register_t *p, discovery_msg_t *msg)
{
  // TODO: Maybe check if the IP addreses have chenged
  p->last_announce = now_ms();
  if(p->status != CONNECTED && p->status != CONNECTING && p->status != DISCONNECTED) {
    if(p->connected) {
      p->status = CONNECTED;
    } else {
      p->status = DISCONNECTED;
      p->active_ip = NULL;
    }
  }
}

static participant_register_t *register_new_participant_info(discovery_msg_t *msg)
{
  participant_register_t *participant = (participant_register_t *)malloc(sizeof(participant_register_t));
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
  participant->connected = false;
  participant->next = detected_nodes;

  detected_nodes = participant;
  return participant;
}

static void remove_participan_info(participant_register_t* target) {
  participant_register_t** container = &detected_nodes;

  while(*container != NULL) {
    if(*container == target) {
      *container = (*container)->next;
      if(target->address != NULL)
        free(target->address);
      if(target->tcp_socket != 0 || target->connected)
        lwip_close(target->tcp_socket);
      free(target);
      return;
    }
    container = &((*container)->next);
  }
}

void print_participants_info() {
    participant_register_t* current = detected_nodes;
    
    u_int8_t count = 0;

    while(current != NULL) {
      count++;
      LOG_INFO(TAG, "Participant %d: %s :: Status: %s Connected %s Active IP: %s", count, uuid_to_string(current->id), print_status(current->status), current->connected?"true":"false",current->active_ip==NULL?"null":ipv4_to_str(current->active_ip));
      current = current->next;
    }
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

static bool receive_from_tcp(participant_register_t *participant, uint8_t *ptr, uint16_t *to_receive)
{
  int r = 0;

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
      participant->tcp_socket = 0;
      participant->connected = false;
      return false;
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
        participant->tcp_socket = 0;
        participant->connected = false;
        return false;
      }
    }
    else
    {
      *to_receive -= r;
      ptr+=r;
    }
  }

  return true;
}

static void handle_tcp_client(participant_register_t *participant)
{
  uint16_t to_receive = 2;
  uint16_t msg_size = 0;
  uint16_t msg_code = 0;
  uint8_t* buffer;
  if(receive_from_tcp(participant, (uint8_t*) &msg_size, &to_receive)) {
    LOG_INFO(TAG, "Received message with a total of %d bytes", msg_size);
    to_receive = 2;
    if(msg_size >= 2 && receive_from_tcp(participant, (uint8_t*) &msg_code, &to_receive)) {
      LOG_INFO(TAG, "Received message with identifier: %d", msg_code);
      msg_size -= 2;
      if(msg_size > 0) {
        buffer = malloc(msg_size);
        if(buffer == 0) {
          //malloc failed
          return;
        }
        to_receive = msg_size;
        if(receive_from_tcp(participant, buffer, &to_receive)) {
          event_t* event = create_event(EVENT_TYPE_MESSAGE, msg_code, buffer, msg_size);
          if(event == NULL) {
            free(buffer);
            return;
          } else {
            event_dispatcher_post(event);
          }
        } else {
          free(buffer);
        }
      }
    }

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
    participant_register_t *current = detected_nodes;
    while (current != NULL)
    {
      if ((current->status == DISCONNECTED)) {
        next_deadline = now; //We should try to extablish a connection to that node as soon as possible.
      }
      else if (current->status == CONNECTED)
      {
        FD_SET(current->tcp_socket, &rfds);
        if (current->tcp_socket > highest_socket)
          highest_socket = current->tcp_socket;
        LOG_INFO(TAG, "Added the tcp socket of %s:%d to rfds (highest socket: %d)", ipv4_to_str(current->active_ip), current->port, highest_socket);
      }
      else if (current->status == FAILED)
      {
        next_deadline = MIN(next_deadline, now + (current->announce_period * FORGET_NUMBER_OF_PERIODS - (now - current->last_announce)));
      }
      else if (current->status == SUSPECT)
      {
        next_deadline = MIN(next_deadline, now + (current->announce_period * FAILED_NUMBER_OF_PERIODS - (now - current->last_announce)));
      }
      else if (current->status != DEAD)
      {
        next_deadline = MIN(next_deadline, now + (current->announce_period * SUSPECT_NUMBER_OF_PERIODS - (now - current->last_announce)));
      }
      
      current = current->next;
    }

    FD_SET(socket, &rfds);
    LOG_INFO(TAG, "Added the udp socket to rfds (highest socket: %d)", highest_socket);

    highest_socket++;

    int n = 0;

    now = now_ms(); // current time (ms)
    uint32_t remaining = (next_deadline > now) ? (next_deadline - now) : 0;
    LOG_INFO(TAG, "Time now: %d; Next action: %d; Difference: %d", now, next_deadline, remaining);
    
    if(remaining > 1000) {
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
      // Check TCP connections
      current = detected_nodes;
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
    now = now_ms();
    current = detected_nodes;
    while(current != NULL) {
      switch(current->status) {
        case DISCONNECTED:
        case CONNECTING:
          if(now > current->last_announce + SUSPECT_NUMBER_OF_PERIODS * current->announce_period)
            current->status = SUSPECT;
        break;
        case SUSPECT:
          if(now > current->last_announce + FAILED_NUMBER_OF_PERIODS * current->announce_period)
            current->status = FAILED;
        break;
        case FAILED:
          if(now > current->last_announce + FORGET_NUMBER_OF_PERIODS * current->announce_period)
            current->status = DEAD;
        break;
        default:
        break;
      }

      current = current->next;
    }


    // Now perform mainteance on the several discovered devices
    current = detected_nodes;
    while (current != NULL)
    {
      switch (current->status)
      {
      case DISCONNECTED:
        if (current->ips > 0)
        {
          current->active_ip = current->address;
          current->tcp_socket = setup_tcp_socket();

          struct sockaddr_in server_addr;
          memset(&server_addr, 0, sizeof(server_addr));
          server_addr.sin_family = AF_INET;
          server_addr.sin_port = htons(current->port);
          server_addr.sin_addr.s_addr = ip4_addr_get_u32(current->active_ip);

          LOG_INFO(TAG, "attempting TCP connection to %s (%s:%d)", uuid_to_string(current->id), ipv4_to_str(current->active_ip), current->port);
          if (lwip_connect(current->tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
          {
            lwip_close(current->tcp_socket);
            current->tcp_socket = 0;
            LOG_INFO(TAG, "TCP connection failed to %s (%s:%d)", uuid_to_string(current->id), ipv4_to_str(current->active_ip), current->port);
            current->status = CONNECTING;
          }
          else
          {
            current->status = CONNECTED;
            current->connected = true;
          }
        }
        else
        {
          current->status = DEAD;
          lwip_close(current->tcp_socket);
        }
        break;
      case CONNECTING:
        if (current->active_ip == NULL)
        {
          current->active_ip = current->address;
        }
        else if (current->active_ip == (current->address + (current->ips - 1)))
        {
          current->active_ip = current->address;
        }
        else
        {
          current->active_ip++;
        }
        current->tcp_socket = setup_tcp_socket();

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(current->port);
        server_addr.sin_addr.s_addr = ip4_addr_get_u32(current->active_ip);

        LOG_INFO(TAG, "attempting TCP connection to %s (%s:%d)", uuid_to_string(current->id), ipv4_to_str(current->active_ip), current->port);
        if (lwip_connect(current->tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
          lwip_close(current->tcp_socket);
          current->tcp_socket = 0;
          LOG_INFO(TAG, "TCP connection failed to %s (%s:%d)", uuid_to_string(current->id), ipv4_to_str(current->active_ip), current->port);
        }
        else
        {
          current->status = CONNECTED;
          current->connected = true;
        }
        break;
      case CONNECTED:
        LOG_INFO(TAG, "TCP connection active to %s (%s:%d)\n", uuid_to_string(current->id), ipv4_to_str(current->active_ip), current->port);
        break;
      case SUSPECT:

        break;
      case FAILED:

        break;
      case DEAD:
      default:
        // Nothing to be done here except remove him
        remove_participan_info(current);
        break;
      }
      current = current->next;
    }
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
  TaskHandle_t udpTask = xTaskGetHandle("proto_discovery_udp");
  if (udpTask != NULL)
    vTaskDelete(udpTask);
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

      participant_register_t *p = find_participant_by_id(msg.uuid);

      if (p == NULL)
      {
        p = register_new_participant_info(&msg);
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

static void proto_discovery_task(void *params)
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
    }

    free_event(event);
    event = NULL;
  }
}

bool proto_discovery_init(void)
{
  socket = -1;

  // Initialize the discovery queue
  proto_discovery_queue = xQueueCreate(10, sizeof(event_t *));

  if (!proto_discovery_queue)
  {
    printf("[proto_discovery] Failed to create discovery queue.\n");
    return false;
  }

  // Initialize the registered interest linked list
  detected_nodes = NULL;

  if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP))
  {
    printf("[proto_discovery] Failed to register discovery task with event dispatcher.\n");
    return false;
  }

  if (!event_dispatcher_register(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN))
  {
    printf("[proto_discovery] Failed to register discovery task with event dispatcher.\n");
    event_dispatcher_unregister(proto_discovery_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN);
    return false;
  }

  xTaskCreate(proto_discovery_task, "proto_discovery_task", DISCOVERY_TASK_STACK_SIZE, NULL, 2, NULL);
  printf("[proto_discovery] Initialized");
  return true;
}
