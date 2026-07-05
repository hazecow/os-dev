#pragma once

#include <stdint.h>
#include <limine.h>
#include <stddef.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_paddr;
    uint32_t length;
    uint64_t xsdt_paddr;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) sdt_header_t;

typedef struct {
    sdt_header_t header;
    uint64_t entries[];
} __attribute__((packed)) xsdt_t;

typedef struct {
    sdt_header_t header;
    uint32_t local_interrupt_controller_paddr;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed)) madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_header_t;

typedef struct {
    madt_entry_header_t header;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_paddr;
    uint32_t gsi_base;
} __attribute__((packed)) madt_entry_ioapic_t;

typedef struct {
    uint16_t polarity : 2;
    uint16_t trigger_mode : 2;
    uint16_t reserved : 12;
} __attribute__((packed)) mps_inti_flags_t;

typedef struct {
    madt_entry_header_t header;
    uint8_t bus;
    uint8_t irq;
    uint32_t gsi;
    mps_inti_flags_t flags;
} __attribute__((packed)) madt_entry_iso_t;

void acpi_init(struct limine_rsdp_response *response, uint64_t *pml4);
size_t acpi_ioapic_entries_num(void);
size_t acpi_iso_entries_num(void);

/**
 * IRQ を渡して対応する ISO エントリのポインタを返す
 * 見つからなければ NULL を返す
 */
const madt_entry_iso_t *acpi_find_iso(uint8_t irq);

/**
 * GSI を渡して対応する I/O APIC エントリへのポインタを返す
 * 見つからなければ NULL を返す
 */
const madt_entry_ioapic_t *acpi_find_ioapic(uint32_t gsi);