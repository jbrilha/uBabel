// proto_discovery.c
#include "protop_iot_control.h"
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

typedef struct device_node {
  uint8_t id[UUID_SIZE];
  struct candidate_node* next;
} device_t;

static uint8_t id[UUID_SIZE];
static QueueHandle_t iot_control_queue;

static device_t* new;
static device_t* established;

static device_t* register_new_participant(event_t* neighbor_up_event) {
  device_t* device = (device_t*) malloc(sizeof(device_t));
  if(device_t != NULL) {
    memcpy(device->id, (uint8_t*) neighbor_up_event->payload, UUID_SIZE);
    device->next = new;
    new = device;
    return device;
  }
  return NULL;
} 

static void send_init_request(device_t* d) {
  message_t* init_msg = create_empty_event(MSG_INIT, id, IOT_CONTROL_PROTO_ID, d->id, 0);
  if(init_msg != NULL) {
    event_t * ev = create_event(EVENT_TYPE_MESSAGE, MSG_INIT, init_msg, sizeof(message_t));
    if(ev != NULL)
      send_message(ev);
    else
      free_message(init_msg);
  }
}

static void iot_control_protocol_task() {
  event_t *event;

  while (true)
  {
    LOG_INFO(TAG, "main cycle activity");
    if (xQueueReceive(simple_overlay_queue, &event, pdMS_TO_TICKS(DELTA_PERIOD)) == pdPASS)
    {
      if (event->type == EVENT_TYPE_NOTIFICATION)
      {
        if (event->subtype == NOTIFICATION_NEIGHBOR_UP)
        {
          device_t* d = register_new_participant(event);
          send_init_request(d);
          
        } else if (event->subtype == NOTIFICATION_NEIGHBOR_DOWN) {
          
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
  
  simple_overlay_queue = xQueueCreate(PROTO_QUEUE_SIZE, sizeof(event_t*));

  if(!simple_overlay_queue) {
    LOG_ERROR(TAG, "Failed to initialize the queue for the simple overlay protocol");
    return;
  }

  new = NULL;
  extablsihed = NULL;
  
  proto_manager_register_protocol(iot_control_queue, IOT_CONTROL_PROTO_ID);
  event_dispatcher_register(iot_control_queue, EVENT_TYPE_NOTIFICATION, NOTIFICATION_NEIGHBOR_UP);
  event_dispatcher_register(iot_control_queue, EVENT_TYPE_NOTIFICATION, NOTIFICATION_NEIGHBOR_DOWN);

  xTaskCreate(iot_control_protocol_task() , "iot_control_protocol", IOT_CONTORL_TASK_STACK_SIZE, NULL, IOT_CONTROL_TASK_PRIORITY, NULL);

  LOG_INFO(TAG, "Initialized the task");
}