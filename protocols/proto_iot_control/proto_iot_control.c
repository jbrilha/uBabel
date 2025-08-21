// proto_discovery.c
#include "proto_simple_overlay.h"
#include "event_dispatcher.h"
#include "proto_manager.h"
#include "comm_manager.h"
#include "common_events.h"

#define IOT_CONTROL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define IOT_CONTROL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define PROTO_QUEUE_SIZE 5

#define TAG "IoT Control Protocol"

typedef struct device_node {
  uint8_t id[UUID_SIZE];
  struct candidate_node* next;
} device;

static uint8_t id[UUID_SIZE];
static QueueHandle_t iot_control_queue;

static candidate_node_t* new;
static candidate_node_t* established;

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
          event_t = 
          
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