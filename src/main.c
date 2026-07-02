#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "frame_buffer.h"
#include "font.h"
#include "console.h"
#include "gdt.h"
#include "interrupt.h"
#include "serial.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "apic.h"

extern uint8_t _rodata_start[];
extern uint8_t _rodata_end[];

__attribute__((aligned(16)))
uint8_t g_kernel_stack[16 * 1024];

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request exe_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
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
    interrupt_init();
    kprint("[OK] IDT initialized\n");

    // setup serial port
    serial_init();
    serial_write_string("[OK] Serial port initialized\n");

    // setup memory
    if (memmap_request.response == NULL
        || memmap_request.response->entry_count < 1) {
        hcf();
    }
    if (hhdm_request.response == NULL) {
        hcf();
    }
    pmm_init(memmap_request.response, hhdm_request.response->offset);    
    kprint("[OK] PMM initialized\n");
    uint64_t *pml4 = vmm_init(exe_addr_request.response, memmap_request.response);
    kprint("[OK] VMM initialized\n");
    heap_init(pml4);
    kprint("[OK] Heap initialized\n");

    // APIC の動作確認
    apic_init(pml4);
    // IF フラグの有効化
    __asm__ volatile ("sti");

    hcf();
}