#include "event_queue.h"
#include <stdbool.h>

#define EVENT_QUEUE_SIZE 32

static event_message_t g_event_queue[EVENT_QUEUE_SIZE];
static volatile size_t g_head = 0;
static volatile size_t g_tail = 0;

void event_queue_push(event_message_t msg) {
  size_t next = (g_head + 1) % EVENT_QUEUE_SIZE;
  if (next == g_tail) {
    return; // 満杯のときは push しない
  }
  g_event_queue[g_head] = msg;
  g_head = next;
}

bool event_queue_pop(event_message_t *msg) {
  if (g_head == g_tail) {
    return false; // 空
  }
  *msg = g_event_queue[g_tail];
  g_tail = (g_tail + 1) % EVENT_QUEUE_SIZE;
  return true;
}
