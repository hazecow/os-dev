#include <stdbool.h>
#include "event_queue.h"

#define KEY_QUEUE_SIZE 32

// キーイベントを管理するキュー型リングバッファ
static key_event_t g_key_queue[KEY_QUEUE_SIZE];
static volatile size_t g_head = 0;
static volatile size_t g_tail = 0;

// ハンドラ側（生産者）
void key_queue_push(keycode_t kc, bool released) {
    size_t next = (g_head + 1) % KEY_QUEUE_SIZE;
    if (next == g_tail) {
        return; // 満杯、古いイベントを守るため破棄（設計次第）
    }
    g_key_queue[g_head].code = kc;
    g_key_queue[g_head].released = released;
    g_head = next;
}

// メインループ側（消費者）
bool key_queue_pop(key_event_t *out) {
    if (g_head == g_tail) {
        return false; // 空
    }
    *out = g_key_queue[g_tail];
    g_tail = (g_tail + 1) % KEY_QUEUE_SIZE;
    return true;
}