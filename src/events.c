#include "events.h"

#include <string.h>

#define _MAX_HANDLERS (16)

// private things

typedef struct {
    uint8_t in_use;
    event_type_t event_type;
    EVENT_HANDLER handler;
    void *context;
} _event_registration_t;


static _event_registration_t _registered_handlers[_MAX_HANDLERS];


// functions

event_handle_t event_register_handler(event_type_t event_type, EVENT_HANDLER handler, void *context) {
    for(uint8_t i = 0; i < _MAX_HANDLERS; i++) {
        _event_registration_t *reg = &(_registered_handlers[i]);
        if (!reg->in_use) {
            reg->in_use = 1;
            reg->event_type = event_type;
            reg->handler = handler;
            reg->context = context;
            return i;
        }
    }
    return 255;
}

void event_unregister_handler(event_handle_t handle) {
    _event_registration_t *reg = &(_registered_handlers[handle]);
    memset(reg, 0, sizeof(*reg));
}

void event_dispatch(event_type_t event_type, void *event_args) {
    for(uint8_t i = 0; i < _MAX_HANDLERS; i++) {
        _event_registration_t *reg = &(_registered_handlers[i]);
        if (reg->in_use && reg->event_type == event_type) {
            reg->handler(reg->context, event_type, event_args);
        }
    }
}
