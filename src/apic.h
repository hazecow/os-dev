#pragma once

#include <stdint.h>

uint32_t read_lapic_reg(uint64_t offset);
void write_lapic_reg(uint64_t offset, uint32_t value);
void apic_init(uint64_t *pml4);