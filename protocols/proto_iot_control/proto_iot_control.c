// proto_discovery.c
#include "proto_iot_control.h"
#include "event_dispatcher.h"
#include "platform.h"
#include "proto_manager.h"
#include "comm_manager.h"
#include "common_events.h"
#include <string.h>


#define IOT_CONTROL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define IOT_CONTROL_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

#define PROTO_QUEUE_SIZE 5

#define TAG "IoT Control Protocol"

static SemaphoreHandle_t iot_control_protocol_mutex;

static uint8_t id[UUID_SIZE];
static QueueHandle_t iot_control_queue;
static device_node_t* peers;

static device_t* device_info;

device_t* get_device_info_data() {
  return device_info;
}

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

    LOG_INFO(TAG, "Processing the device update message for device %s", uuid_to_string(update_message->payload));

    uint8_t* message_ptr = (uint8_t*) update_message->payload;

    device_node_t* node = find_participant((int8_t*)message_ptr);
    if(node == NULL && find_participant((int8_t*)update_message->sourceId) != NULL) {
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
  iot_control_protocol_mutex = xSemaphoreCreateMutex();

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
        
            event_t *ui_refresh = create_event(EVENT_TYPE_REQUEST, REQUEST_REFRESH_MENU, NULL, 0);
            if(ui_refresh) {

            event_dispatcher_post(ui_refresh);
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

action_t* initialize_action(uint16_t code, const char* name, action_t* previous) {
  action_t* action = (action_t*) malloc(sizeof(action_t));
  if(action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    return NULL;
  }

  action->action_name = strdup(name);
  if(action->action_name == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action name");
    free(action);
    return NULL;
  }

  action->action_code = code;
  action->next = NULL;
  action->prev = previous;
  action->parameters = NULL;

  return action;
}

parameter_t* initialize_parameter(uint16_t code, const char* name, parameter_t* previous) {
  parameter_t* parameter = (parameter_t*) malloc(sizeof(parameter_t));
  if(parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    return NULL;
  }

  parameter->parameter_name = strdup(name);

  parameter->parameter_value = code;
  parameter->next = NULL;
  parameter->prev = previous;

  return parameter;
}

device_t* initialize_device_type_led_rgb(uint8_t type, const char* name) {
  device_t* device = (device_t*) malloc(sizeof(device_t));
  if(device == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for device");
    return NULL;
  }

  device->device_type = type;
  device->device_name = strdup(name);
  device->actions = NULL;

  action_t** action = &device->actions;
  action_t* previous_action = NULL;

  parameter_t * master_parameter = NULL;
  parameter_t** parameter = &master_parameter;
  parameter_t* previous_parameter = NULL;

  *parameter = initialize_parameter(COLOR_RED, "Color Red", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free(device->device_name);
    free(device);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  *parameter = initialize_parameter(COLOR_GREEN, "Color Green", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free(device->device_name);
    free(master_parameter);
    free(device);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  *parameter = initialize_parameter(COLOR_BLUE, "Color Blue", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free(device->device_name);
    free(master_parameter->next);
    free(master_parameter);
    free(device);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  *parameter = initialize_parameter(COLOR_WHITE, "Color White", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free(device->device_name);
    free(previous_parameter->next->next);
    free(master_parameter->next);
    free(master_parameter);
    free(device);
    return NULL;
  }

  (*parameter)->next = master_parameter;
  master_parameter->prev = *parameter;

  *action = initialize_action(DEVICE_ACTION_ON, "Turn On", previous_action);
  if(*action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    free(device->device_name);
    free(device);
    return NULL;
  } 
  (*action)->parameters = master_parameter;

  previous_action = *action;
  action = &((*action)->next);
  *action = initialize_action(DEVICE_ACTION_BLINK, "Blink", previous_action);
  if(*action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    free(device->device_name);
    free(previous_action);
    free(device);
    return NULL;
  }
  (*action)->parameters = master_parameter;

  previous_action = *action;
  action = &((*action)->next);
  *action = initialize_action(DEVICE_ACTION_OFF, "Turn Off", previous_action);
  if(*action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    free(device->device_name);
    free(previous_action->prev);
    free(previous_action);
    free(device);
    return NULL;
  }
  (*action)->parameters = initialize_parameter(DEVICE_ACTION_OFF, "Turn Off", NULL);
  if((*action)->parameters == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action parameters");
    free(device->device_name);
    free(previous_action->prev);
    free(previous_action);
    free(device);
    return NULL;
  }

  (*action)->parameters->next = (*action)->parameters;
  (*action)->parameters->prev = (*action)->parameters;

  (*action)->next = device->actions;
  device->actions->prev = *action;

  return device;
}

void free_parameters(parameter_t* parameter) {
  if(parameter->prev != NULL)
    parameter->prev->next = NULL;

  while (parameter != NULL) {
    parameter_t* next = parameter->next;
    free(parameter);
    parameter = next;
  }
}

parameter_t* generate_parameters_emoji() {
  parameter_t * master_parameter = NULL;
  parameter_t** parameter = &master_parameter;
  parameter_t* previous_parameter = NULL;

  (*parameter) = initialize_parameter(EMOJI_Smile, "Smile", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Laught, "Laught", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Sad, "Sad", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Mad, "Mad", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Angry, "Angry", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Cry, "Cry", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Greedy, "Greedy", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Cood, "Cood", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Shy, "Shy", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Awkward, "Awkward", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }
    
  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Heart, "Heart", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }
/*
  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_SmallHeart, "SmallHeart", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_BrokenHeart, "BrokenHeart", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Waterdrop, "Waterdrop", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Flame, "Flame", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Creeper, "Creeper", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_MadCreeper, "MadCreeper", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Sword, "Sword", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_WoodenSword, "Wooden Sword", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_CrystalSword, "Crystal Sword", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_House, "House", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Tree, "Tree", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Flower, "Flower", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Umbrella, "Umbrella", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Rain, "Rain", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Monster, "Monster", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Crab, "Crab", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Duck, "Duck", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Rabbit, "Rabbit", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  } */

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(EMOJI_Cat, "Cat", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  (*parameter)->next = master_parameter;
  master_parameter->prev = *parameter;

  return master_parameter;
}

parameter_t* generate_parameters_animation() {
  parameter_t * master_parameter = NULL;
  parameter_t** parameter = &master_parameter;
  parameter_t* previous_parameter = NULL;

  (*parameter) = initialize_parameter(ANIMATION_BigClock, "Big Clock", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(ANIMATION_SmallClock, "Small Clock", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(ANIMATION_Rainbow, "Rainbow", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(ANIMATION_Fire, "Fire", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(ANIMATION_WalkingChild, "Walking Child", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  previous_parameter = *parameter;  
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(ANIMATION_BrokenHeart, "Broken Heart", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free_parameters(master_parameter);
    return NULL;
  }

  (*parameter)->next = master_parameter;
  master_parameter->prev = *parameter;

  return master_parameter;
}

device_t*  initialize_device_type_led_matrix(uint8_t type, const char* name) {
  device_t* device = (device_t*) malloc(sizeof(device_t));
  if(device == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for device");
    return NULL;
  }

  device->device_type = type;
  device->device_name = strdup(name);
  device->actions = NULL;

  action_t** action = &device->actions;
  action_t* previous_action = NULL;

  (*action) = initialize_action(DEVICE_ACTION_SHOW_EMOJI, "Show Emoji", previous_action);
  if(*action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    free(device->device_name);
    free(device);
    return NULL;
  }
  (*action)->parameters = generate_parameters_emoji();
  if((*action)->parameters == NULL)
  {
    LOG_ERROR(TAG, "Failed to allocate memory for action parameters");
    free(device->device_name);
    free(*action);
    free(device);
    return NULL;
  }

  previous_action = *action;
  action = &((*action)->next);
  (*action) = initialize_action(DEVICE_ACTION_SHOW_ANIMATION, "Show Animation", previous_action);
  if(*action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    free(device->device_name);
    free(previous_action);
    free(device);
    return NULL;
  }
  (*action)->parameters = generate_parameters_animation();
  if((*action)->parameters == NULL)
  {
    LOG_ERROR(TAG, "Failed to allocate memory for action parameters");
    free(device->device_name);
    free(previous_action);
    free(*action);
    free(device);
    return NULL;  
  }

  previous_action = *action;
  action = &((*action)->next);
  (*action) = initialize_action(DEVICE_ACTION_CLEAR, "Clear", previous_action);
  if(*action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    free(device->device_name);
    free(previous_action);
    free(device->actions);
    free(device);
    return NULL;
  }
  (*action)->parameters = initialize_parameter(DEVICE_ACTION_CLEAR, "Clear", NULL);

  if((*action)->parameters == NULL)
  {
    LOG_ERROR(TAG, "Failed to allocate memory for action parameters");
    free(device->device_name);
    free(previous_action);
    free(*action);
    free(device->actions);
    free(device);
    return NULL;  
  }
  (*action)->parameters->next = (*action)->parameters;
  (*action)->parameters->prev = (*action)->parameters;  
  
  (*action)->next = device->actions;
  device->actions->prev = *action;
  
  return device;
}

device_t*  initialize_device_type_lcd_display(uint8_t type, const char* name) {
  device_t* device = (device_t*) malloc(sizeof(device_t));
  if(device == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for device");
    return NULL;
  }

  device->device_type = type;
  device->device_name = strdup(name);
  if(device->device_name == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for device name");
    free(device);
    return NULL;
  }
  device->actions = NULL;

  parameter_t * master_parameter = NULL;
  parameter_t** parameter = &master_parameter;
  parameter_t* previous_parameter = NULL;

  (*parameter) = initialize_parameter(TEXT_HELLO, "Hello", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free(device->device_name);
    free(device);
    return NULL;
  }

  previous_parameter = *parameter;
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(TEXT_YES, "Yes", previous_parameter);
  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free(device->device_name);
    free(master_parameter);
    free(device);
    return NULL;
  }
  
  previous_parameter = *parameter;
  parameter = &((*parameter)->next);
  (*parameter) = initialize_parameter(TEXT_NO, "No", previous_parameter);   

  if(*parameter == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for parameter");
    free(device->device_name);
    free(master_parameter->next);
    free(master_parameter);
    free(device);
    return NULL;
  }

  (*parameter)->next = master_parameter;
  master_parameter->prev = *parameter;

  action_t** action = &device->actions;
  action_t* previous_action = NULL;

  (*action) = initialize_action(DEVICE_ACTION_SHOW_TEXT, "Show Text", previous_action);
  if(*action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    free(device->device_name);
    free(master_parameter->next);
    free(master_parameter);
    free(device);
    return NULL;
  }
  (*action)->parameters = master_parameter;

  previous_action = *action;
  action = &((*action)->next);
  (*action) = initialize_action(DEVICE_ACTION_CLEAR, "Clear", previous_action);
  if(*action == NULL) {
    LOG_ERROR(TAG, "Failed to allocate memory for action");
    free(device->device_name);
    free(master_parameter->next);
    free(master_parameter);
    free(device);
    return NULL;
  }

  (*action)->parameters = initialize_parameter(DEVICE_ACTION_CLEAR, "Clear", NULL);
  (*action)->parameters->next = (*action)->parameters;
  (*action)->parameters->prev = (*action)->parameters;

  (*action)->next = device->actions;
  device->actions->prev = *action;  

  return device;
}

void iot_control_protocol_init() {
  get_local_identifier(id);

  LOG_INFO(TAG, "Initializing iot control protocol with id %s", uuid_to_string(id));
  
  iot_control_queue = xQueueCreate(PROTO_QUEUE_SIZE, sizeof(event_t*));

  if(!iot_control_queue) {
    LOG_ERROR(TAG, "Failed to initialize the queue for the simple overlay protocol");
    return;
  }

  peers = NULL;
  //We need to initialize the structures with all types of supported devices, associated actions, and associated parameters

  device_t** devicetype =(device_t**) &device_info;
  *devicetype = initialize_device_type_led_rgb(DEVICE_TYPE_LED_RGB, "Led RGB");
  if(*devicetype != NULL) {
    devicetype = &((*devicetype)->next);
    *devicetype = initialize_device_type_led_matrix(DEVICE_TYPE_LED_MATRIX, "Led Matrix");
    if(*devicetype != NULL) {
      devicetype = &((*devicetype)->next);
      *devicetype = initialize_device_type_lcd_display(DEVICE_TYPE_LCD_DISPLAY, "LCD Display");
      if(*devicetype != NULL) {
        devicetype = &((*devicetype)->next);
        *devicetype = (device_t*) device_info;
      }
    }
  }

  //Create the meta node that represents all nodes
  device_node_t* node = (device_node_t*) malloc(sizeof(device_node_t));

  if(node != NULL) {
    memset(node->id, 0, UUID_SIZE);
    node->access_point = NULL;
    node->n_devices = 3;
    node->devices = (malloc(sizeof(uint16_t) * 3));
    node->devices[0] = DEVICE_TYPE_LED_RGB;
    node->devices[1] = DEVICE_TYPE_LED_MATRIX;
    node->devices[2] = DEVICE_TYPE_LCD_DISPLAY; 
    node->next = peers;
    peers = node;
  }

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

  if(memcmp(((device_node_t*) node)->id, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", UUID_SIZE) == 0) {
    sprintf(str, "ALL NODES");
    return true;
  }

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

bool device_action(iot_node_handle_t node, iot_device_handle_t device, device_t* d, action_t* a, parameter_t* p) {
   LOG_INFO(TAG, "Going to send a request for activating a device");

  if(node == NULL || d == NULL || a == NULL)
    return false;

  xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);

  LOG_INFO(TAG, "Entered protected region to send device request");

  device_node_t* current = peers;
  while(current != NULL && current != (device_node_t*) node) {
    current = current->next;
  }

  if(current != NULL && device < current->n_devices) {

    if(current->n_devices == 0) {
      xSemaphoreGive(iot_control_protocol_mutex);
      LOG_INFO(TAG, "The selected node has no devices");
      return false;
    }

    uint16_t* payload = malloc(sizeof(uint16_t) * 4); 
         
    if(payload != NULL) {
      payload[0] = htons(d->device_type);
      payload[1] = htons(a->action_code);
      payload[2] = p != NULL ? htons(p->parameter_value) : htons(0);
       
      device_node_t* proxy = NULL;

      if(memcmp(current->id, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", UUID_SIZE) == 0) {
         LOG_INFO(TAG, "Sending multicast request");
        payload[3] = htons(BROADCAST_OP); //Broadcast
        proxy = peers;
        while(proxy != NULL && (memcmp(proxy->id, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", UUID_SIZE) == 0 || proxy->access_point != NULL )) {
          proxy = proxy->next;
        }
        
        if(proxy == NULL) {
          free(payload);
          xSemaphoreGive(iot_control_protocol_mutex);
          LOG_INFO(TAG, "No proxy available to send the broadcast request");
          return false;
        }
      } else {
        LOG_INFO(TAG, "Sending unicast request");
        //This is a unicast... we need to check if the node is still present
        payload[3] = htons(UNICAST_OP); //Unicast
        proxy = current;
      }

      message_t* device_operation = create_message(MSG_CMD, id, IOT_CONTROL_PROTO_ID, current->id, IOT_CONTROL_PROTO_ID, (uint8_t*) payload, sizeof(uint16_t) * 4);

      if(device_operation != NULL) {
        event_t * ev = create_event(EVENT_TYPE_MESSAGE, EVENT_MESSAGE_SEND, device_operation, sizeof(message_t));
        if(ev != NULL && !send_message(ev, proxy->id)) {
          if(ev != NULL) {
            free_event(ev);
          } else {
            free_message(device_operation);
          }
          xSemaphoreGive(iot_control_protocol_mutex);
          LOG_INFO(TAG, "Attempted to prepare the request but failed to sent.");
          return false;
        } else {
          xSemaphoreGive(iot_control_protocol_mutex);
          LOG_INFO(TAG, "Sent the request");
          return true;
        }
      } else {
        free(payload);
        xSemaphoreGive(iot_control_protocol_mutex);
        LOG_INFO(TAG, "Failed to allocate the message_t instance");
        return false;
      }
    }
  }
   
  xSemaphoreGive(iot_control_protocol_mutex);
  LOG_INFO(TAG, "Not sent the request ");
  return false;
}

int get_nodes_snapshot(node_snapshot_t **out_snapshot) {
    if (out_snapshot == NULL) return 0;
    
    xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);
    
    int count = 0;
    device_node_t *node = peers;
    if (node == NULL) {
        xSemaphoreGive(iot_control_protocol_mutex);
        LOG_ERROR(TAG, "snapshot: no peers at all :(");
        return 0;
    }
    
    do {
        LOG_DEBUG(TAG, "snapshot: node id=%s, n_devices=%d", 
                 uuid_to_string(node->id), node->n_devices);
        count++;
        node = node->next;
    } while (node != NULL && node != peers);
    
    LOG_DEBUG(TAG, "snapshot: total count = %d", count);
    
    node_snapshot_t *snapshot = malloc(sizeof(node_snapshot_t) * count);
    if (snapshot == NULL) {
        xSemaphoreGive(iot_control_protocol_mutex);
        return 0;
    }
    
    int idx = 0;
    node = peers;
    do {
        LOG_DEBUG(TAG, "snapshot: copying node %d", idx);
        memcpy(snapshot[idx].id, node->id, UUID_SIZE);
        snapshot[idx].n_devices = node->n_devices;
        snapshot[idx].devices = malloc(sizeof(uint16_t) * node->n_devices);
        if (snapshot[idx].devices && node->n_devices > 0) {
            memcpy(snapshot[idx].devices, node->devices, sizeof(uint16_t) * node->n_devices);
        }
        idx++;
        node = node->next;
    } while (node != NULL && node != peers);
    
    xSemaphoreGive(iot_control_protocol_mutex);
    
    *out_snapshot = snapshot;
    LOG_INFO(TAG, "snapshot: returning %d nodes", count);
    return count;
}

iot_node_handle_t find_node_by_id(uint8_t *id) {
    xSemaphoreTake(iot_control_protocol_mutex, portMAX_DELAY);
    
    device_node_t *current = peers;
    while (current != NULL) {
        if (memcmp(current->id, id, UUID_SIZE) == 0) {
            xSemaphoreGive(iot_control_protocol_mutex);
            return (iot_node_handle_t)current;
        }
        current = current->next;
        if (current == peers) break;
    }
    
    xSemaphoreGive(iot_control_protocol_mutex);
    return NULL;
}
