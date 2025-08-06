#include "pico_buttons.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "event_dispatcher.h"


typedef enum pin {
  A     = 12,
  B     = 13,
  X     = 14,
  Y     = 15,
} pin_t;

void pico_buttons_init() {
  // setup button inputs
  gpio_set_function(A, GPIO_FUNC_SIO); gpio_set_dir(A, GPIO_IN); gpio_pull_up(A);
  gpio_set_function(B, GPIO_FUNC_SIO); gpio_set_dir(B, GPIO_IN); gpio_pull_up(B);
  gpio_set_function(X, GPIO_FUNC_SIO); gpio_set_dir(X, GPIO_IN); gpio_pull_up(X);
  gpio_set_function(Y, GPIO_FUNC_SIO); gpio_set_dir(Y, GPIO_IN); gpio_pull_up(Y);
}

bool is_pressed(uint8_t button) {
  return !gpio_get(button);
}


void pico_buttons_task(__unused void *params) {
  uint16_t debounceA = 0;
  uint16_t debounceB = 0;
  uint16_t debounceX = 0;
  uint16_t debounceY = 0;

  while(true) {
    if(debounceA == 0 && is_pressed(A)) {
      event_t *event = create_event(EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_A_PRESSED, NULL, 0);
      if(event) {
        event_dispatcher_post(event);
      }
      debounceA = 5;
    } else if(debounceA > 0) {
      debounceA--;
    }
    
    if(debounceB == 0 && is_pressed(B)) {
      event_t *event = create_event(EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_B_PRESSED, NULL, 0);
      if(event) {
        event_dispatcher_post(event);
      } 
      debounceB = 5;
    } else if(debounceB > 0) {
      debounceB--;
    }
    
    if(debounceX == 0 && is_pressed(X)) {
      event_t *event = create_event(EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_X_PRESSED, NULL, 0);
      if(event) {
        event_dispatcher_post(event);
      }
      debounceX = 5;
    } else if(debounceX > 0) {
      debounceX--;
    }

    if(debounceY == 0 && is_pressed(Y)) {
      event_t *event = create_event(EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_Y_PRESSED, NULL, 0);
      if(event) {
        event_dispatcher_post(event);
      } 
      debounceY = 5;
    } else if(debounceY > 0) {
      debounceY--;    
    }

    vTaskDelay(pdMS_TO_TICKS(15));
  }
}

