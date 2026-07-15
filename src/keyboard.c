#include "keyboard.h"
#include "apic.h"
#include "event_queue.h"
#include "io.h"
#include "keycode.h"

static bool g_extended = false;
static bool g_shift_pressed = false;

void keyboard_set_shift(bool pressed) { g_shift_pressed = pressed; }

bool keyboard_is_shift_pressed(void) { return g_shift_pressed; }

void keyboard_handler(interrupt_frame_t *frame) {
  uint8_t scancode = inb(0x60);

  // 拡張バイト (0xE0) の場合
  if (scancode == 0xe0) {
    g_extended = true;
    lapic_send_eoi();
    return;
  }

  bool released = scancode & 0x80;
  keycode_t kc = scancode_to_keycode(scancode, g_extended);
  g_extended = false;

  // 未対応キーは無視する
  if (kc == KEY_UNKNOWN) {
    lapic_send_eoi();
    return;
  }

  // shift 状態の更新
  if (kc == KEY_LSHIFT || kc == KEY_RSHIFT) {
    keyboard_set_shift(!released);
  }

  // イベント通知
  event_message_t msg = {.type = KEY_PRESSED,
                         .arg = {.kbd = {.kc = kc, .released = released}}};
  event_queue_push(msg);

  lapic_send_eoi();
}
