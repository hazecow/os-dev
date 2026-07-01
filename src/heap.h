#pragma once

#include <stdint.h>

void heap_init(uint64_t *pml4_vaddr);
void *kmalloc(uint64_t size);
void kfree(void *ptr);