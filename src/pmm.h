#pragma once

#include <limine.h>

#define PAGE_FRAME_SIZE_BYTE 4096

extern uint64_t g_hhdm_offset;
static inline void *paddr_to_vaddr(uint64_t paddr) {
    return (void *)(g_hhdm_offset + paddr);
}

void display_memmap(struct limine_memmap_response *response);

void pmm_init(struct limine_memmap_response *response, uint64_t hhdm_offset);
void *pmm_alloc(void);
void pmm_free(void *addr);

void pmm_dump_stats(void);