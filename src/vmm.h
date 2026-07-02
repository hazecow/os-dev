#pragma once

#include <stdint.h>

#define PAGE_PRESENT  (1ull << 0)
#define PAGE_RW       (1ull << 1)
#define PAGE_USER     (1ull << 2)
#define PAGE_PWT      (1ull << 3)
#define PAGE_PCD      (1ull << 4)
#define PAGE_HUGE     (1ull << 7)
#define PAGE_NX       (1ull << 63)

void vmm_map(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
uint64_t *vmm_init(
    struct limine_executable_address_response *exe_addr_response,
    struct limine_memmap_response *memmap_response
);

void vmm_dump_entry(uint64_t *pml4, uint64_t vaddr);