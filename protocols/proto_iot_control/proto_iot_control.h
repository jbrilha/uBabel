#pragma once

#include <stdbool.h>
#include <stdint.h>

#define IOT_CONTROL_PROTO_ID 17000

#define  DEVICE_TYPE_LED_RGB 1
#define  DEVICE_TYPE_LED_MATRIX 2
#define  DEVICE_TYPE_LCD_DISPLAY 3

/**************** MESSAGE CODES TO INTERACT WITH BABEL ON RASPBERRY *******************/
#define MSG_INIT 17001
#define MSG_CMD 17002
#define MSG_DEVICE_UPDATE 17003

/**************** COMMAND CODES TO INTERACT WITH BABEL ON RASPBERRY *******************/
#define DEVICE_ACTION_ON 1
#define DEVICE_ACTION_BLINK 2
#define DEVICE_ACTION_OFF 3
#define DEVICE_ACTION_SHOW_EMOJI 4
#define DEVICE_ACTION_CLEAR 5
#define DEVICE_ACTION_SHOW_ANIMATION 6
#define DEVICE_ACTION_SET_COLOR 7
#define DEVICE_ACTION_SHOW_TEXT 8
#define DEVICE_ACTION_SET_LETTER 9
#define DEVICE_ACTION_SET_SYMBOL 10

#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_BLUE 3
#define COLOR_WHITE 4

#define TEXT_HELLO 1
#define TEXT_YES 2
#define TEXT_NO 3

#define EMOJI_Smile 0
#define EMOJI_Laught 1
#define EMOJI_Sad 2
#define EMOJI_Mad 3
#define EMOJI_Angry 4
#define EMOJI_Cry 5
#define EMOJI_Greedy 6
#define EMOJI_Cood 7
#define EMOJI_Shy 8
#define EMOJI_Awkward 9
#define EMOJI_Heart 10
#define EMOJI_SmallHeart 11
#define EMOJI_BrokenHeart 12
#define EMOJI_Waterdrop 13
#define EMOJI_Flame 14
#define EMOJI_Creeper 15
#define EMOJI_MadCreeper 16
#define EMOJI_Sword 17
#define EMOJI_WoodenSword 18
#define EMOJI_CrystalSword 19
#define EMOJI_House 20
#define EMOJI_Tree 21
#define EMOJI_Flower 22
#define EMOJI_Umbrella 23
#define EMOJI_Rain 24
#define EMOJI_Monster 25
#define EMOJI_Crab 26
#define EMOJI_Duck 27
#define EMOJI_Rabbit 28
#define EMOJI_Cat 29

#define ANIMATION_BigClock 0
#define ANIMATION_SmallClock 1
#define ANIMATION_Rainbow 2
#define ANIMATION_Fire 3
#define ANIMATION_WalkingChild 4
#define ANIMATION_BrokenHeart 5

typedef struct parameter_node {
    char* parameter_name;
    uint16_t parameter_value;
    struct parameter_node* next;
    struct parameter_node* prev;
} parameter_t;

typedef struct action_node {
    char* action_name;
    uint16_t action_code;
    struct action_node* next;
    struct action_node* prev;
    parameter_t* parameters;
} action_t;

typedef struct device_node_spec {
    uint16_t device_type;
    char* device_name;
    struct device_node_spec* next;
    action_t* actions;
} device_t;

typedef void* iot_node_handle_t;
typedef signed char iot_device_handle_t;



#define INVALID_NODE -2
#define NO_DEVICE -1

#ifdef __cplusplus
extern "C" {
#endif  

void iot_control_protocol_init();

device_t* get_device_info_data();

iot_node_handle_t initialize_node_iterator();
iot_node_handle_t next_node(iot_node_handle_t node);
iot_node_handle_t previous_node(iot_node_handle_t node);
bool print_node_identifier(iot_node_handle_t node, char* str);

iot_device_handle_t initialize_device_iterator(iot_node_handle_t node);
iot_device_handle_t next_device(iot_node_handle_t node, iot_device_handle_t device);
iot_device_handle_t previous_device(iot_node_handle_t node, iot_device_handle_t device);

uint8_t get_device_type(iot_node_handle_t node, iot_device_handle_t device);

bool device_action(iot_node_handle_t node, iot_device_handle_t device, device_t* d, action_t* a, parameter_t* p);

#ifdef __cplusplus
}
#endif  
