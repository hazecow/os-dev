#include <stddef.h>

#include "heap.h"
#include "panic.h"
#include "pmm.h"
#include "vmm.h"

#define HEAP_VIRT_BASE 0xffff900000000000ull
#define HEAP_SIZE_BYTE (4 * 1024 * 1024)
#define HEAP_ALIGN 16

static uint64_t g_heap_next;
static uint64_t g_heap_end;

/**
 * addr を align の倍数になるように切り上げる
 * align は2の累乗でなければならない
 */
static uint64_t align_up(uint64_t addr, uint64_t align) {
    if ((align & (align - 1)) != 0) {
        panic("align_up: align must be powers of 2");
    }
    return (addr + align - 1) & ~(align - 1);
}

void heap_init(uint64_t *pml4_vaddr) {
    for (uint64_t offset = 0; offset < HEAP_SIZE_BYTE; offset += PAGE_FRAME_SIZE_BYTE) {
        uint64_t paddr = (uint64_t)pmm_alloc();
        vmm_map(pml4_vaddr, HEAP_VIRT_BASE + offset, paddr, PAGE_PRESENT | PAGE_RW | PAGE_NX);
    }
    g_heap_next = HEAP_VIRT_BASE;
    g_heap_end = HEAP_VIRT_BASE + HEAP_SIZE_BYTE;
}

void *kmalloc(uint64_t size) {
    uint64_t addr = align_up(g_heap_next, HEAP_ALIGN);
    if (addr + size > g_heap_end) {
        return NULL;
    }
    g_heap_next = addr + size;
    return (void *)addr;
}

void kfree(void *ptr) {
    (void)ptr;
    panic("kfree: not implemented");
}