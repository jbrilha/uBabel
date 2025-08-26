// proto_discovery.c
#include "proto_iot_control.h"
#include "event_dispatcher.h"
#include "proto_manager.h"
#include "comm_manager.h"
#include "common_events.h"

#define IOT_CONTROL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define IOT_CONTROL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define PROTO_QUEUE_SIZE 5

#define TAG "IoT Control Protocol"

/**************** MESSAGE CODES TO INTERACT WITH BABEL ON RASPBERRY *******************/
#define MSG_INIT 17001
#define MSG_CMD 17002

typedef struct device {
  uint8_t device_type;
  struct device* next;
} device_t;

typedef struct device_node {
  uint8_t id[UUID_SIZE];
  device_t* devices;
  struct device_node* next;
} device_node_t;

static uint8_t id[UUID_SIZE];
static QueueHandle_t iot_control_queue;

static device_node_t* new;
static device_node_t* established;

static device_node_t* register_new_participant(event_t* neighbor_up_event) {
  device_node_t* device = (device_node_t*) malloc(sizeof(device_t));
  if(device != NULL) {
    memcpy(device->id, (uint8_t*) neighbor_up_event->payload, UUID_SIZE);
    device->devices = NULL;
    device->next = new;
    new = device;
    return device;
  }
  return NULL;
} 

static bool remove_participant(event_t* neighbor_down_event) {
  bool found = false;
  device_node_t** current = &established;
  while(found == false && *current != NULL) {
    if(memcmp((*current)->id, neighbor_down_event->payload, UUID_SIZE) == 0) {
      //Found correct device_node_t
      found = true;
      device_t* aux = (*current)->devices;
      while(aux != NULL) {
        (*current)->devices = aux->next;
        aux->next = NULL;
        free(aux);
        aux = (*current)->devices;
      }
      break;
    }
    current = &((*current)->next);
  }
  if(!found) 
  {
    current = &new;
    while(found == false && *current != NULL) {
      if(memcmp((*current)->id, neighbor_down_event->payload, UUID_SIZE) == 0) {
        //Found correct device_node_t
        found = true;
        device_t* aux = (*current)->devices;
        while(aux != NULL) {
          (*current)->devices = aux->next;
          aux->next = NULL;
          free(aux);
          aux = (*current)->devices;
        }
        break;
      }
      current = &((*current)->next);
    }
  }
  return found;
}

static void send_init_request(device_node_t* d) {
  message_t* init_msg = create_empty_message(MSG_INIT, id, IOT_CONTROL_PROTO_ID, d->id, IOT_CONTROL_PROTO_ID);
  if(init_msg != NULL) {
    event_t * ev = create_event(EVENT_TYPE_MESSAGE, EVENT_MESSAGE_SEND, init_msg, sizeof(message_t));
    if(ev != NULL)
      send_message(ev, d->id);
    else
      free_message(init_msg);
  }
}

static void iot_control_protocol_task() {
  event_t *event;

  while (true)
  {
    LOG_INFO(TAG, "main cycle activity");
    if (xQueueReceive(iot_control_queue, &event, portMAX_DELAY) == pdPASS)
    {
      if (event->type == EVENT_TYPE_NOTIFICATION)
      {
        if (event->subtype == NOTIFICATION_NEIGHBOR_UP)
        {
          device_node_t* d = register_new_participant(event);
          send_init_request(d);
          
        } else if (event->subtype == NOTIFICATION_NEIGHBOR_DOWN) {
          bool removed = remove_participant(event);
          if(removed)
            LOG_INFO(TAG, "Removed node device with id %s", uuid_to_string(event->payload));
          else
            LOG_INFO(TAG, "Failed to remove device node with id %s", uuid_to_string(event->payload)); 
        } 
      }

      free_event(event);
      event = NULL;
    }
  }
}

void iot_control_protocol_init() {
  get_local_identifier(id);

  LOG_INFO(TAG, "Initializing iot control protocol with id %s", uuid_to_string(id));
  
  iot_control_queue = xQueueCreate(PROTO_QUEUE_SIZE, sizeof(event_t*));

  if(!iot_control_queue) {
    LOG_ERROR(TAG, "Failed to initialize the queue for the simple overlay protocol");
    return;
  }

  new = NULL;
  established = NULL;
  
  proto_manager_register_protocol(iot_control_queue, IOT_CONTROL_PROTO_ID);
  event_dispatcher_register(iot_control_queue, EVENT_TYPE_NOTIFICATION, NOTIFICATION_NEIGHBOR_UP);
  event_dispatcher_register(iot_control_queue, EVENT_TYPE_NOTIFICATION, NOTIFICATION_NEIGHBOR_DOWN);

  xTaskCreate(iot_control_protocol_task , "iot_control_protocol", IOT_CONTROL_TASK_STACK_SIZE, NULL, IOT_CONTROL_TASK_PRIORITY, NULL);

  LOG_INFO(TAG, "Initialized the task");
}