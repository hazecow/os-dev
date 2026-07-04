#pragma once

#include <stdint.h>

uint32_t lapic_read(uint64_t offset);
void lapic_write(uint64_t offset, uint32_t value);
void lapic_init(uint64_t *pml4);

uint32_t ioapic_read(uint64_t ioapic_vaddr, uint32_t reg);
void ioapic_write(uint64_t ioapic_vaddr, uint32_t reg, uint32_t value);

/**
 * IRQ と vector 番号を紐づける
 */
void ioapic_redirect(uint8_t irq, uint8_t vector);

void ioapic_init(void);

void lapic_send_eoi(void);