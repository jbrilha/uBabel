// proto_discovery.c
#include "proto_simple_overlay.h"
#include "event_dispatcher.h"
#include "proto_manager.h"
#include "comm_manager.h"
#include "common_events.h"

#define SIMPLE_OVERLAY_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define SIMPLE_OVERLAY_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define TARGET_NEIGHBORS 5
#define PROTO_QUEUE_SIZE 5

#define DELTA_PERIOD 3000

#define TAG "Simple Overlay Protocol"

typedef struct candidate_node {
  uint8_t id[UUID_SIZE];
  struct candidate_node* next;
} candidate_node_t;

static uint8_t id[UUID_SIZE];
static QueueHandle_t simple_overlay_queue;

static candidate_node_t* neighbors;
static uint8_t n_neighbors;
static candidate_node_t* connecting;
static uint8_t n_connecting;
static candidate_node_t* candidates;
static uint8_t n_candidates;


static candidate_node_t* find_node(uint8_t* id, candidate_node_t* list) {
  candidate_node_t* current = list;
  while(current != NULL) {
    if(memcmp(id, current->id, UUID_SIZE) == 0) 
      return current;
    current = current->next;
  }
  return NULL;
}

static candidate_node_t* detach_first(candidate_node_t** head_of_list, uint8_t* counter) {
  candidate_node_t* target = NULL;
  if(*head_of_list != NULL) {
    target = *head_of_list;
    *head_of_list = target->next;
    target->next = NULL;
    *counter--;
  }
  return target;
}

static candidate_node_t* detach_node(uint8_t* id, candidate_node_t** head_of_list, uint8_t* counter) {
  candidate_node_t** current = head_of_list;

  while(*current != NULL) {
    if(memcmp((*current)->id, id, UUID_SIZE) == 0) {
      candidate_node_t* target = *current;
      *current = (*current)->next;
      *counter--;
      return target;
    }
    current = &((*current)->next);
  }

  return NULL;
}

static void add_node(candidate_node_t* node, candidate_node_t** head_of_list, uint8_t* counter) {
  node->next = *head_of_list;
  *head_of_list = node;
  *counter++;
}

static bool check_node_exists(uint8_t* id) {
  return find_node(id, neighbors) || find_node(id, connecting) || find_node(id, candidates);
}

static void simple_overlay_task() {
  event_t *event;

  while (true)
  {
    LOG_INFO(TAG, "main cycle activity");
    if (xQueueReceive(simple_overlay_queue, &event, pdMS_TO_TICKS(DELTA_PERIOD)) == pdPASS)
    {
      if (event->type == EVENT_TYPE_NOTIFICATION)
      {
        if (event->subtype == EVENT_NOTIFICATION_NODE_DISCOVERED)
        {
          if(!check_node_exists((uint8_t*) event->payload)) {  

            candidate_node_t* node = (candidate_node_t*) malloc(sizeof(candidate_node_t));
            if(node != NULL) {
              memcpy(node->id, event->payload, UUID_SIZE);
              node->next = NULL;
              add_node(node, &candidates, &n_candidates);
             }
          }
        } else if (event->subtype == EVENT_NOTIFICATION_NODE_CONNECTED) {
          candidate_node_t* t = detach_node((uint8_t*) event->payload, &connecting, &n_connecting);
          if(t != NULL) {
            add_node(t, &neighbors, &n_neighbors);
            event_t* nup = create_neighbor_up_notification(t->id);
            if(nup != NULL) {
              event_dispatcher_post(nup);
            }
          }
        } else if (event->subtype == EVENT_NOTIFICATION_NODE_FAILED) {
          candidate_node_t* f = detach_node((uint8_t*) event->payload, &neighbors, &n_neighbors);
          if(f == NULL) {
            f = detach_node((uint8_t*) event->payload, &connecting, &n_connecting);
            if(f == NULL) f = detach_node((uint8_t*) event->payload, &candidates, &n_candidates);

            if(f != NULL) {
              free(f);
            }
          } else {
            //Was on neighbors
            event_t* ndown = create_neighbor_down_notification(f->id);
            if(ndown != NULL) {
              event_dispatcher_post(ndown);
            }
            free(f);
          }
        }
      }
    }

    free_event(event);
    event = NULL;

    while(n_neighbors + n_connecting < TARGET_NEIGHBORS && n_candidates > 0) {
      LOG_INFO(TAG, "trying to fill in neighbors (have %d candidates)");
      candidate_node_t* c = detach_first(&candidates, &n_candidates);
      if(c != NULL) {
        open_conection(c->id, SIMPLE_OVERLAY_PROTO_ID);
        add_node(c, &connecting, &n_connecting);
      }
    }
  }
}


void simple_overlay_network_init() {
  get_local_identifier(id);

  LOG_INFO(TAG, "Initializing protocol with id %s", uuid_to_string(id));
  
  simple_overlay_queue = xQueueCreate(PROTO_QUEUE_SIZE, sizeof(event_t*));
  neighbors = NULL;
  connecting = NULL;
  candidates = NULL;

  //proto_manager_register_protocol(simple_overlay_queue, SIMPLE_OVERLAY_PROTO_ID);
  //event_dispatcher_register(simple_overlay_queue, EVENT_TYPE_NOTIFICATION, EVENT_NOTIFICATION_NODE_DISCOVERED);

  //xTaskCreate(simple_overlay_task, "simple_overlay_protocol", SIMPLE_OVERLAY_TASK_STACK_SIZE, NULL, SIMPLE_OVERLAY_TASK_PRIORITY, NULL);

  LOG_INFO(TAG, "Initialized the task");
}