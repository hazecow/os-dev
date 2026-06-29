#pragma once

#include <limine.h>

void display_memmap(struct limine_memmap_response *response);

void pmm_init(struct limine_memmap_response *response, uint64_t hhdm_offset);
void *pmm_alloc(void);
void pmm_free(void *addr);

void pmm_dump_stats(void);