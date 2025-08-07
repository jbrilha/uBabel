// event_dispatcher.c - Micro-Babel
#include "event_dispatcher.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define DISPATCHER_QUEUE_LENGTH 32  // Maximum number of events in the dispatcher queue

static QueueHandle_t dispatcher_queue = NULL;
static event_type_subscription_t subscriptions[MAX_EVENT_TYPES];
static SemaphoreHandle_t subscription_mutex;
static SemaphoreHandle_t dispatch_mutex;

event_type_subscription_t* find_type_subscriptions(event_type_t type) {
   return &subscriptions[type];
}

event_subtype_subscription_t* find_subtype_subscription(event_type_subscription_t* node, event_subtype_t subtype) {
    event_subtype_subscription_t* current = node->subtypes;
    while (current) {
        if (current->subtype == subtype) {
            return current; // Found the subtype subscription
        }
        current = current->next;
    }

    return current;
}

event_subtype_subscription_t* find_or_create_subtype_subscription(event_type_subscription_t* node, event_subtype_t subtype) {
    event_subtype_subscription_t* current = node->subtypes;
    while (current) {
        if (current->subtype == subtype) {
            return current; // Found the subtype subscription
        }
        current = current->next;
    }
    event_subtype_subscription_t* new_subtype = malloc(sizeof(event_subtype_subscription_t));
    if (!new_subtype) {
        printf("[Dispatcher] Failed to allocate memory for new subtype subscription.\n");
        return NULL;
    }
    new_subtype->subtype = subtype;
    new_subtype->queue_count = 0;
    new_subtype->queue_capacity = QUEUE_INITIAL_CAPACITY;
    new_subtype->queues = malloc(QUEUE_INITIAL_CAPACITY * sizeof(QueueHandle_t));
    
    if (!new_subtype->queues) {
        printf("[Dispatcher] Failed to allocate memory for queues in new subtype subscription.\n");
        free(new_subtype);
        return NULL;
    }

    new_subtype->next = node->subtypes; // Insert at the beginning of the linked list
    node->subtypes = new_subtype;

    return new_subtype;
}

bool add_event_subscription(event_subtype_subscription_t* subtype_node, QueueHandle_t queue) {
    if(subtype_node->queue_count == subtype_node->queue_capacity) {
        // Resize the queues array
        QueueHandle_t* new_queues = malloc(sizeof(QueueHandle_t) * subtype_node->queue_capacity * 2);
        if (!new_queues) {
            printf("[Dispatcher] Failed to allocate memory for resizing queues.\n");
            return false;
        }
        memcpy(new_queues, subtype_node->queues, sizeof(QueueHandle_t) * subtype_node->queue_capacity);
        free(subtype_node->queues);
        subtype_node->queues = new_queues;
        subtype_node->queue_capacity *= 2; // Double the capacity
    }

    subtype_node->queues[subtype_node->queue_count] = queue;
    subtype_node->queue_count++;
    printf("[Dispatcher] Added queue to subtype %d, now has %d queues.\n", subtype_node->subtype, subtype_node->queue_count);
    return true;    
}

void remove_event_subscription(event_subtype_subscription_t* subtype_node, QueueHandle_t queue) {
    bool found = false;
    for(int i = 0; i < subtype_node->queue_count; i++) {
        if(!found) {
            if(subtype_node->queues[i] == queue) {
                found = true;
                i--;
            }
        } else {
            if(i < subtype_node->queue_capacity - 1)
                subtype_node->queues[i] = subtype_node->queues[i+1];
            else
                subtype_node->queues[i] = NULL;
        }
    }
    if(found)
        subtype_node->queue_count--;
}

bool event_dispatcher_register(QueueHandle_t queue, event_type_t type, event_subtype_t subtype) {
    xSemaphoreTake(subscription_mutex, portMAX_DELAY);
    
    event_type_subscription_t* node = find_type_subscriptions(type);
    event_subtype_subscription_t* subtype_node = find_or_create_subtype_subscription(node, subtype);
    if (!subtype_node) {
        printf("[Dispatcher] Failed to get subtype subscription for type=%d subtype=%d\n", type, subtype);
        xSemaphoreGive(subscription_mutex);
        return false;
    }
    
    bool added = add_event_subscription(subtype_node, queue);
    xSemaphoreGive(subscription_mutex);
    return added;
}

bool event_dispatcher_unregister(QueueHandle_t queue, event_type_t type, uint16_t subtype) {
    xSemaphoreTake(subscription_mutex, portMAX_DELAY);

    event_type_subscription_t* node = find_type_subscriptions(type);
    event_subtype_subscription_t* subtype_node = find_subtype_subscription(node, subtype);

    if (!subtype_node) {
        printf("[Dispatcher] Failed to get subtype subscription for type=%d subtype=%d\n", type, subtype);
        xSemaphoreGive(subscription_mutex);
        return true;
    }

    remove_event_subscription(subtype_node, queue);
    xSemaphoreGive(subscription_mutex);
    return true;
}

bool event_dispatcher_post(event_t* event) {
    printf("[Dispatcher] Posting event type=%d subtype=%d\n", event->type, event->subtype);
    if (!dispatcher_queue) return false;

    xSemaphoreTake(dispatch_mutex, portMAX_DELAY);
    
    if (xQueueSend(dispatcher_queue, &event, portMAX_DELAY) != pdPASS) {
        printf("[Dispatcher] Failed to post event type=%d subtype=%d\n", event->type, event->subtype);
        xSemaphoreGive(dispatch_mutex);
        return false;
    }

    xSemaphoreGive(dispatch_mutex);
    printf("[Dispatcher] Event posted successfully type=%d subtype=%d\n", event->type, event->subtype); 
    return true;
}

void event_dispatcher_task(void* params) {
    event_t* event;
    while (true) {
        if (xQueueReceive(dispatcher_queue, &event, portMAX_DELAY) == pdPASS) {
            printf("[Dispatcher] Dispatching event type=%d subtype=%d\n", event->type, event->subtype);
            
            event_subtype_subscription_t* subtype_node = find_subtype_subscription(find_type_subscriptions(event->type), event->subtype);
            if (!subtype_node) {
                printf("[Dispatcher] No subscriptions found for event type=%d subtype=%d\n", event->type, event->subtype);
                free_event(event);
                return;
            }

            for(int i = 0; i < subtype_node->queue_count; i++) {
                event->reference_counter++;
                if (xQueueSend(subtype_node->queues[i], &event, portMAX_DELAY) != pdPASS) {
                    printf("[Dispatcher] Failed to send event to queue %d for type=%d subtype=%d\n", i, event->type, event->subtype);
                } 
            }
        }
    }
}

bool event_dispatcher_init(void) {
    init_event_mutex();
    dispatcher_queue = xQueueCreate(DISPATCHER_QUEUE_LENGTH, sizeof(event_t*));
    if (!dispatcher_queue) {
        printf("[Dispatcher] Failed to create event dispatcher queue.\n");
    }
    subscription_mutex = xSemaphoreCreateMutex();
    dispatch_mutex = xSemaphoreCreateMutex();

    if (!subscription_mutex) {
        printf("[Dispatcher] Failed to create subscription mutex.\n");
        return false;
    }

    for(int i = 0; i < MAX_EVENT_TYPES; i++) {
        subscriptions[i].type = 0; // Initialize type to 0
        subscriptions[i].subtypes = NULL; // Initialize subtypes to NULL
    }

    return xTaskCreate(event_dispatcher_task, "Dispatcher", 1024, NULL, 2, NULL) == pdPASS; 
}
