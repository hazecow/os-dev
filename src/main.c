#include <limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "acpi.h"
#include "apic.h"
#include "console.h"
#include "event_queue.h"
#include "font.h"
#include "frame_buffer.h"
#include "gdt.h"
#include "heap.h"
#include "interrupt.h"
#include "keyboard.h"
#include "layout_jis.h"
#include "pmm.h"
#include "ps2.h"
#include "serial.h"
#include "vmm.h"

extern uint8_t _rodata_start[];
extern uint8_t _rodata_end[];

__attribute__((aligned(16))) uint8_t g_kernel_stack[16 * 1024];

__attribute__((used, section(".limine_requests"))) static volatile uint64_t
    limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((
    used,
    section(
        ".limine_requests"))) static volatile struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0};

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_memmap_request
    memmap_request = {.id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {.id = LIMINE_HHDM_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct
    limine_executable_address_request exe_addr_request = {
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID, .revision = 0};

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_rsdp_request
    rsdp_request = {.id = LIMINE_RSDP_REQUEST_ID, .revision = 0};

__attribute__((used,
               section(".limine_requests_start"))) static volatile uint64_t
    limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t
    limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
  for (;;) {
    asm("hlt");
  }
}

void kmain(void) {
  if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
    hcf();
  }

  // setup framebuffer
  if (framebuffer_request.response == NULL ||
      framebuffer_request.response->framebuffer_count < 1) {
    hcf();
  }
  struct limine_framebuffer *framebuffer =
      framebuffer_request.response->framebuffers[0];
  fb_init(framebuffer->address, framebuffer->width, framebuffer->height,
          framebuffer->pitch);

  // setup font and console
  font_init();
  console_init();
  // kprint はこれ以降使用可能

  // setup GDT
  gdt_init();

  // setup IDT
  interrupt_init();

  // setup serial port
  serial_init();

  // setup memory
  if (memmap_request.response == NULL ||
      memmap_request.response->entry_count < 1) {
    hcf();
  }
  if (hhdm_request.response == NULL) {
    hcf();
  }
  pmm_init(memmap_request.response, hhdm_request.response->offset);
  uint64_t *pml4 = vmm_init(exe_addr_request.response, memmap_request.response);
  heap_init(pml4);

  // タイマ割込みの設定
  lapic_init(pml4);

  // キーボード割込みの設定
  acpi_init(rsdp_request.response, pml4);
  ioapic_init();
  ps2_init();

  // IF フラグの有効化
  __asm__ volatile("sti");

  // 画面描画
  rect_t rect1 = {.x = 300, .y = 200, .width = 100, .height = 100};
  rect_t rect2 = {.x = 300, .y = 300, .width = 100, .height = 100};
  rect_t rect3 = {.x = 400, .y = 200, .width = 300, .height = 400};
  fb_draw_rect(rect1, 0x0070ff);
  fb_draw_rect(rect2, 0xffff00);
  fb_draw_rect(rect3, 0xff88c8);

  event_message_t msg;
  for (;;) {
    if (event_queue_pop(&msg)) {
      switch (msg.type) {
      case KEY_PRESSED: {
        char c;
        if (keycode_to_char_jis(msg.arg.kbd.kc, keyboard_is_shift_pressed(),
                                &c)) {
          if (msg.arg.kbd.released) {
            kprint("%c", c);
          }
        }
        break;
      }
      case TIMER_TIMEOUT: {
        blink_cursor();
        break;
      }
      }
    }
  }

  hcf();
}
