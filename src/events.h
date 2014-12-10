#ifndef EVENTS_H_
#define EVENTS_H_

#include <stdint.h>

typedef enum {
    EVENT_TYPE_TICK = 0, // event_args is NULL

    EVENT_TYPE_COUNT
} event_type_t;

typedef uint8_t event_handle_t;
typedef void (*EVENT_HANDLER)(void *context, event_type_t event_type, void *event_args);

event_handle_t event_register_handler(event_type_t event_type, EVENT_HANDLER handler, void *context);
void event_unregister_handler(event_handle_t handle);

void event_dispatch(event_type_t event_type, void *event_args);

#endif
