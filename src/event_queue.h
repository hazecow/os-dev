#pragma once

#include "keycode.h"

typedef struct {
    keycode_t code;
    bool released;
} key_event_t;

void key_queue_push(keycode_t kc, bool released);
bool key_queue_pop(key_event_t *out);