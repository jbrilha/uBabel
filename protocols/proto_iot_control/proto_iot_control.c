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
#define MSG_DEVICE_UPDATE 17003

static SemaphoreHandle_t iot_control_protocol_mutex;

typedef struct device_node {
  uint8_t id[UUID_SIZE];
  uint16_t n_devices;
  uint16_t* devices;
  struct device_node* next;
} device_node_t;

static uint8_t id[UUID_SIZE];
static QueueHandle_t iot_control_queue;

static device_node_t* peers;

static device_node_t* register_new_participant(event_t* neighbor_up_event) {
  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* node = (device_node_t*) malloc(sizeof(device_node_t));
  if(node != NULL) {
    memcpy(node->id, (uint8_t*) neighbor_up_event->payload, UUID_SIZE);
    node->n_devices = 0;
    node->devices = NULL;
    node->next = peers;
    peers = node;
  }

  xSemaphoreGive(iot_control_protocol_mutex);
  return node;
} 

static device_node_t* find_participant(int8_t* id) {
  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;
  while(current != NULL) {
    if(memcpy(current->id, id, UUID_SIZE) == 0) {
      xSemaphoreGive(iot_control_protocol_mutex);
      return current;
    }
    current = current->next;
  }

  xSemaphoreGive(iot_control_protocol_mutex);
  return NULL;
}

static void update_node_devices(message_t* update_message) {
  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* node = find_participant(update_message->payload);
  if(node != NULL) {
    uint16_t* msgptr = (uint16_t*) (update_message->payload + UUID_SIZE);
    node->n_devices = *msgptr;
    if(node->devices != NULL) {
      free(node->devices);
      node->devices = NULL;
    }
    node->devices = (uint16_t*) malloc(sizeof(uint16_t) * node->n_devices);
    if(node->devices != NULL) {
      memcpy(node->devices, (msgptr++) , sizeof(uint16_t) * node->n_devices);
    } else {
      node->n_devices = 0;
    }
  }

  xSemaphoreGive(iot_control_protocol_mutex);
}

static bool remove_participant(event_t* neighbor_down_event) {
  bool found = false;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t** current = &peers;
  while(found == false && *current != NULL) {
    if(memcmp((*current)->id, neighbor_down_event->payload, UUID_SIZE) == 0) {
      //Found correct device_node_t
      found = true;
      device_node_t* rem = (*current);
      (*current) = (*current)->next;
      if(rem->devices != NULL)
        free(rem->devices);
      free(rem);
      break;
    }
    current = &((*current)->next);
  }

  xSemaphoreGive(iot_control_protocol_mutex);
  return found;
}

static void send_init_request(device_node_t* d) {
  message_t* init_msg = create_empty_message(MSG_INIT, id, IOT_CONTROL_PROTO_ID, d->id, IOT_CONTROL_PROTO_ID);
  if(init_msg != NULL) {
    event_t * ev = create_event(EVENT_TYPE_MESSAGE, EVENT_MESSAGE_SEND, init_msg, sizeof(message_t));
    if(ev != NULL) {
      send_message(ev, init_msg->destId);
      LOG_INFO(TAG, "Sent the init request to %s", uuid_to_string(d->id));
    } else {
      free_message(init_msg);
    }
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
          LOG_INFO(TAG, "Neighbor up notification for %s, will register the participant.", uuid_to_string(event->payload));
          device_node_t* d = register_new_participant(event);
          if(d != NULL) {
            LOG_INFO(TAG, "Sending the init request to the new candidate");
            send_init_request(d);
          } else {
            LOG_INFO(TAG, "Could not sent the request to the candadiate.");
          }
          
        } else if (event->subtype == NOTIFICATION_NEIGHBOR_DOWN) {
          bool removed = remove_participant(event);
          if(removed)
            LOG_INFO(TAG, "Removed node device with id %s", uuid_to_string(event->payload));
          else
            LOG_INFO(TAG, "Failed to remove device node with id %s", uuid_to_string(event->payload)); 
        } 
      }
      else if(event->type == EVENT_TYPE_MESSAGE) {
        if(event->subtype == MSG_DEVICE_UPDATE) {
          LOG_INFO(TAG, "Received an MSG_DEVICE_UPDATE from %s:%d", uuid_to_string(((message_t* )event->payload)->sourceId), ((message_t* )event->payload)->sourceProto);
          update_node_devices((message_t*) event->payload);
        } else {
          LOG_INFO(TAG, "Received unknown message type %d from %s:%d", event->subtype, uuid_to_string(((message_t* )event->payload)->sourceId), ((message_t* )event->payload)->sourceProto);
        }
      }

      free_event(event);
      event = NULL;
    }
  }
}

void iot_control_protocol_init() {
  get_local_identifier(id);
  iot_control_protocol_mutex = xSemaphoreCreateMutex();

  LOG_INFO(TAG, "Initializing iot control protocol with id %s", uuid_to_string(id));
  
  iot_control_queue = xQueueCreate(PROTO_QUEUE_SIZE, sizeof(event_t*));

  if(!iot_control_queue) {
    LOG_ERROR(TAG, "Failed to initialize the queue for the simple overlay protocol");
    return;
  }

  peers = NULL;
  
  proto_manager_register_protocol(iot_control_queue, IOT_CONTROL_PROTO_ID);
  event_dispatcher_register(iot_control_queue, EVENT_TYPE_NOTIFICATION, NOTIFICATION_NEIGHBOR_UP);
  event_dispatcher_register(iot_control_queue, EVENT_TYPE_NOTIFICATION, NOTIFICATION_NEIGHBOR_DOWN);

  xTaskCreate(iot_control_protocol_task , "iot_control_protocol", IOT_CONTROL_TASK_STACK_SIZE, NULL, IOT_CONTROL_TASK_PRIORITY, NULL);

  LOG_INFO(TAG, "Initialized the task");
}


//******** API FUNCTION ***********/ 

iot_node_handler_t initialize_device_iterator() {
  return peers;
}

iot_node_handler_t next_device(iot_node_handler_t device) {
  if(device == NULL)
    return (iot_node_handler_t) peers;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;
  while(current != NULL) {
    if ( current == (device_node_t*) device )
      break;
    current = current->next;
  }

  if(current == NULL || current->next == NULL) {
    xSemaphoreGive(iot_control_protocol_mutex);
    return (iot_node_handler_t) peers;
  }
  
  xSemaphoreGive(iot_control_protocol_mutex);
  return current->next;
}

iot_node_handler_t previous_device(iot_node_handler_t device) {
  if(device == NULL)
    return (iot_node_handler_t) peers;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;    
  device_node_t* previous = NULL;

  if(peers == (device_node_t*) device) {
    while(current->next != NULL)
      current = current->next;

    xSemaphoreGive(iot_control_protocol_mutex);
    return (iot_node_handler_t) current;
  }

  previous = current;
  current = current->next;

  while(current != NULL && current != (device_node_t*) device) {
    previous = current;
    current = current->next;
  }

  if(current != NULL) {
    xSemaphoreGive(iot_control_protocol_mutex);
    return (iot_node_handler_t) previous;
  } else {
    xSemaphoreGive(iot_control_protocol_mutex);
    return (iot_node_handler_t) peers;
  }


}

bool print_device_identifier(iot_node_handler_t device, const char* str) {
  if(device == NULL)
    return false;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;
  while(current != NULL) {
    if ( current == (device_node_t*) device )
      break;
    current = current->next;
  }

  if(current != NULL) {
    char* s = uuid_to_string(current->id);
    memcpy(str, s, strlen(s));
    xSemaphoreGive(iot_control_protocol_mutex);
    return true;
  } else {
    xSemaphoreGive(iot_control_protocol_mutex);
    return false;
  }  
}

bool device_has_led(iot_node_handler_t device) {
  if(device == NULL)
    return false;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);  

  device_node_t* current = peers;
  while(current != NULL) {
    if ( current == (device_node_t*) device )
      break;
    current = current->next;
  }

  if(current != NULL) { 
    for(int i = 0; i < current->n_devices; i++) {
      if(current->devices[i] == DEVICE_TYPE_LED_RGB) {
        xSemaphoreGive(iot_control_protocol_mutex);
        return true;
      }
    }
  }
  xSemaphoreGive(iot_control_protocol_mutex);
  return false;
}

bool activate_led(iot_node_handler_t device) {
  if(device == NULL)
    return false;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  

  xSemaphoreGive(iot_control_protocol_mutex);
}