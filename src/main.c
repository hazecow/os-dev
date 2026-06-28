#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "frame_buffer.h"
#include "font.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "memory.h"
#include "serial.h"

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);


__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};


__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // setup framebuffer
    if (framebuffer_request.response == NULL
        || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    fb_init(framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch);

    // setup font and console
    font_init();
    console_init();
    // kprint はこれ以降使用可能
    kprint("[OK] Console initialized\n");

    // setup GDT
    gdt_init();
    kprint("[OK] GDT initialized\n");

    // setup IDT
    idt_init();
    kprint("[OK] IDT initialized\n");

    // setup serial port
    serial_init();
    serial_write_string("[OK] Serial port initialized\n");

    // setup memory
    if (memmap_request.response == NULL
        || memmap_request.response->entry_count < 1) {
        hcf();
    }
    display_memmap(memmap_request.response);
    if (hhdm_request.response == NULL) {
        hcf();
    }
    pmm_init(memmap_request.response, hhdm_request.response->offset);    
    pmm_dump_stats();

    // メモリ割り当てと解放のテスト
    void *p1 = pmm_alloc();
    void *p2 = pmm_alloc();
    kprint("p1 = 0x%lx\n", (uint64_t)p1);
    kprint("p2 = 0x%lx\n", (uint64_t)p2);
    pmm_free(p1);
    void *p3 = pmm_alloc();
    kprint("p3 = 0x%lx\n", (uint64_t)p3);
    // 今の割り当て方なら p3 == p1 になるはず

    hcf();
}