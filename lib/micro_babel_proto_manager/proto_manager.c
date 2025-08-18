#include "proto_manager.h"

#include <stdlib.h>

#define TAG "ProtoManager"

typedef struct protocol_subscription
{
    uint8_t protocol_id;
    QueueHandle_t queue;
    struct protocol_subscription *next;
} protocol_subscription_t;

static protocol_subscription_t *protocols = NULL;

static protocol_subscription_t *find_protocol_subscription(uint16_t proto_id)
{
    protocol_subscription_t *current = protocols;
    while (current != NULL)
    {
        if (current->protocol_id == proto_id)
            return current;
        current = current->next;
    }
    return current;
}

bool proto_manager_register_protocol(QueueHandle_t queue, uint16_t protocol_id)
{
    protocol_subscription_t *ps = find_protocol_subscription(protocol_id);
    if (ps != NULL)
    {
        ps->queue = queue;
        return true;
    }

    ps = malloc(sizeof(protocol_subscription_t));
    if (ps == NULL)
    {
        LOG_ERROR(TAG, "Could not allocate memory for registering a protocol queue");
        return false;
    }
    ps->protocol_id = protocol_id;
    ps->queue = queue;
    ps->next = protocols;

    protocols = ps;
    return true;
}

QueueHandle_t find_protocol(uint16_t proto_id)
{
    protocol_subscription_t *ps = find_protocol_subscription(proto_id);
    if (ps != NULL)
        return ps->queue;
    return NULL;
}