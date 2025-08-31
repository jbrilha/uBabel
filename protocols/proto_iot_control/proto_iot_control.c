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
  uint8_t* access_point;
  uint16_t n_devices;
  uint16_t* devices;
  struct device_node* next;
} device_node_t;

static uint8_t id[UUID_SIZE];
static QueueHandle_t iot_control_queue;

static device_node_t* peers;

static device_node_t* register_new_participant(event_t* neighbor_up_event) {
  device_node_t* node = (device_node_t*) malloc(sizeof(device_node_t));

  if(node != NULL) {
    memcpy(node->id, (uint8_t*) neighbor_up_event->payload, UUID_SIZE);
    node->access_point = NULL;
    node->n_devices = 0;
    node->devices = NULL;
    node->next = peers;
    peers = node;
  }

  return node;
} 

static device_node_t* register_new_indirect_participant(uint8_t* participant_id, uint8_t* access_point) {
  device_node_t* node = (device_node_t*) malloc(sizeof(device_node_t));

  if(node != NULL) {
    memcpy(node->id, participant_id, UUID_SIZE);
    node->access_point = (uint8_t*) malloc(UUID_SIZE);
    if(node->access_point != NULL) {
      memcpy(node->access_point, access_point, UUID_SIZE);
      node->n_devices = 0;
      node->devices = NULL;
      node->next = peers;
      peers = node;
    } else {
      free(node);
      node = NULL;
    }
  } 

  return node;
}

static device_node_t* find_participant(int8_t* id) {
  device_node_t* current = peers;
  while(current != NULL) {
    if(memcmp(current->id, id, UUID_SIZE) == 0) {
      xSemaphoreGive(iot_control_protocol_mutex);
      return current;
    }
    current = current->next;
  }

  return NULL;
}

static void update_node_devices(message_t* update_message) {
  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  LOG_INFO(TAG, "received an update device message with payload%s null and size %d", update_message->payload == NULL ? "":" not", update_message->payload_size);

  if(update_message->payload != NULL && update_message->payload_size >= UUID_SIZE + sizeof(uint16_t)) {

    uint8_t* message_ptr = (uint8_t*) update_message->payload;

    device_node_t* node = find_participant(message_ptr);
    if(node == NULL && find_participant(update_message->sourceId) != NULL) {
      LOG_INFO(TAG, "Creating an indirect node with id: %s", uuid_to_string(message_ptr));
      node = register_new_indirect_participant(message_ptr, update_message->sourceId);
    } else {
      LOG_INFO(TAG, "Node being updated already exists: %s", uuid_to_string(message_ptr));
    }

    if(node != NULL) {
      message_ptr += UUID_SIZE; 
      node->n_devices =  ( message_ptr[0]<<8 | message_ptr[1] );
      LOG_INFO(TAG, "Number of devices that we are adding to this node is: %d", node->n_devices);
      if(node->devices != NULL) {
        free(node->devices);
        node->devices = NULL;
      }
      node->devices = (uint16_t*) malloc(sizeof(uint16_t) * node->n_devices);
      if(node->devices != NULL) {
        message_ptr+=2;
        for(int i = 0; i < node->n_devices; i++) {
          node->devices[i] = ( message_ptr[i*2] << 8 | message_ptr[i*2 + 1] );
        }
      } else {
        node->n_devices = 0;
      }
    }

  }

  xSemaphoreGive(iot_control_protocol_mutex);
}

static bool remove_participant(event_t* neighbor_down_event) {
  bool found = false;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* rem = NULL;

  device_node_t** current = &peers;
  while(found == false && *current != NULL) {
    if(memcmp((*current)->id, neighbor_down_event->payload, UUID_SIZE) == 0) {
      //Found correct device_node_t
      found = true;
      rem = (*current);
      (*current) = (*current)->next;
      if(rem->access_point != NULL) 
        free(rem->access_point);
      if(rem->devices != NULL)
        free(rem->devices);
      
      break;
    }
    current = &((*current)->next);
  }

  //Delete indirect
  if(rem != NULL) {
    current = &peers;
    while(*current != NULL) {
      if((*current)->access_point != NULL && memcmp((*current)->access_point, rem->id, UUID_SIZE) == 0) {
        if( (*current)->n_devices < 0 && (*current)->devices != NULL )
          free((*current)->devices);
        
        device_node_t* drop = (*current);
        *current = (*current)->next;
        free(drop);
      } else {
        current = &((*current)->next);
      }
    }
  
    free(rem);
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

iot_node_handle_t initialize_node_iterator() {
  return peers;
}

iot_node_handle_t next_node(iot_node_handle_t node) {
  if(node == NULL)
    return (iot_node_handle_t) peers;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;
  while(current != NULL && current != (device_node_t*) node) {
    current = current->next;
  }

  if(current == NULL || current->next == NULL) {
    xSemaphoreGive(iot_control_protocol_mutex);
    return (iot_node_handle_t) peers;
  }
  
  xSemaphoreGive(iot_control_protocol_mutex);
  return current->next;
}

iot_node_handle_t previous_node(iot_node_handle_t node) {
  if(node == NULL)
    return (iot_node_handle_t) peers;


  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;    
  device_node_t* previous = NULL;

  if(peers == (device_node_t*) node) {
    while(current->next != NULL)
      current = current->next;

    xSemaphoreGive(iot_control_protocol_mutex);
    return (iot_node_handle_t) current;
  }

  previous = current;
  current = current->next;

  while(current != NULL && current != (device_node_t*) node) {
    previous = current;
    current = current->next;
  }

  if(current != NULL) {
    xSemaphoreGive(iot_control_protocol_mutex);
    return (iot_node_handle_t) previous;
  } else {
    xSemaphoreGive(iot_control_protocol_mutex);
    return (iot_node_handle_t) peers;
  }
}

bool print_node_identifier(iot_node_handle_t node, char* str) {
  if(node == NULL)
    sprintf(str, "NO DEVICE SELECTED");

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;
  while(current != NULL && current != (device_node_t*) node) {
    current = current->next;
  }

  if(current != NULL) {
   sprintf(str,
          "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
          current->id[0], current->id[1], current->id[2], current->id[3],
          current->id[4], current->id[5],
          current->id[6], current->id[7],
          current->id[8], current->id[9],
          current->id[10], current->id[11], current->id[12], current->id[13], current->id[14], current->id[15]);
    xSemaphoreGive(iot_control_protocol_mutex);
    return true;
  } else {
    sprintf(str, "NO DEVICE SELECTED");
    xSemaphoreGive(iot_control_protocol_mutex);
    return false;
  }  
}

iot_device_handle_t initialize_device_iterator(iot_node_handle_t node) {
  if(node == NULL)
    return INVALID_NODE;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;
  while(current != NULL && current != (device_node_t*) node) {
    current = current->next;
  }

  if(current == NULL) {
    xSemaphoreGive(iot_control_protocol_mutex);
    return INVALID_NODE;
  } else {
    //device has been checked... you can proceed...
    if(current->n_devices == 0) {
      xSemaphoreGive(iot_control_protocol_mutex);
      return NO_DEVICE;
    }

    xSemaphoreGive(iot_control_protocol_mutex);
    return 0;
  }
}

iot_device_handle_t next_device(iot_node_handle_t node, iot_device_handle_t device) {
    if(node == NULL)
    return INVALID_NODE;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;
  while(current != NULL && current != (device_node_t*) node) {
    current = current->next;
  }

  if(current == NULL) {
    xSemaphoreGive(iot_control_protocol_mutex);
    return INVALID_NODE;
  } else {
    //device has been checked... you can proceed...
    if(current->n_devices == 0) {
      xSemaphoreGive(iot_control_protocol_mutex);
      return NO_DEVICE;
    }

    if(device == NO_DEVICE || device == INVALID_NODE ) {
      xSemaphoreGive(iot_control_protocol_mutex);
      return 0;
    }

    device++;

    if(device >= current->n_devices) {
      device = 0;
    }

    xSemaphoreGive(iot_control_protocol_mutex);
    return device;
  }
}

iot_device_handle_t previous_device(iot_node_handle_t node, iot_device_handle_t device) {
  if(node == NULL)
    return INVALID_NODE;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  device_node_t* current = peers;
  while(current != NULL && current != (device_node_t*) node) {
    current = current->next;
  }

  if(current == NULL) {
    xSemaphoreGive(iot_control_protocol_mutex);
    return INVALID_NODE;
  } else {
    //device has been checked... you can proceed...
    if(current->n_devices == 0) {
      xSemaphoreGive(iot_control_protocol_mutex);
      return NO_DEVICE;
    }

    if(device == NO_DEVICE || device == INVALID_NODE ) {
      xSemaphoreGive(iot_control_protocol_mutex);
      return 0;
    }

    device--;

    if(device < 0) {
      device = current->n_devices - 1;
    }

    xSemaphoreGive(iot_control_protocol_mutex);
    return device;
  }
}

uint8_t get_device_type(iot_node_handle_t node, iot_device_handle_t device) {
    if(node == NULL || device < 0)
      return 0;

    xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

    device_node_t* current = peers;
    while(current != NULL && current != (device_node_t*) node) {
      current = current->next;
    }

    if(current == NULL) {
      xSemaphoreGive(iot_control_protocol_mutex);
      return 0;
    } else {
      //device has been checked... you can proceed...
      if(current->n_devices < device) {
        xSemaphoreGive(iot_control_protocol_mutex);
        return 0;
      }
    }

    xSemaphoreGive(iot_control_protocol_mutex);
    return current->devices[device];
} 

bool activate_led(iot_node_handle_t node) {
  if(node == NULL)
    return false;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  
  xSemaphoreGive(iot_control_protocol_mutex);
}