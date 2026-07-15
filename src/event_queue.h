#pragma once

#include "keycode.h"
#include <stdint.h>

typedef struct {
  keycode_t code;
  bool released;
} key_event_t;

typedef struct {
  enum event_type {
    TIMER_TIMEOUT,
    KEY_PRESSED,
  } type;

  union {
    struct {
      uint64_t timeout;
      int32_t value;
    } timer;

    struct {
      keycode_t kc;
      bool released;
    } kbd;
  } arg;
} event_message_t;

void event_queue_push(event_message_t msg);
bool event_queue_pop(event_message_t *msg);
