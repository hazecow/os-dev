#include "acpi.h"
#include "pmm.h"
#include "string.h"
#include "panic.h"
#include "apic.h"
#include "vmm.h"

#define IOAPIC_ENTRY_MAX 8
#define ISO_ENTRY_MAX    16

static madt_entry_ioapic_t g_ioapic_entries[IOAPIC_ENTRY_MAX];
static madt_entry_iso_t    g_iso_entries[ISO_ENTRY_MAX];
static size_t              g_ioapic_count = 0;
static size_t              g_iso_count = 0;

static void *find_madt(xsdt_t *xsdt) {
    uint32_t xsdt_entry_count = (xsdt->header.length - sizeof(xsdt->header)) / sizeof(uint64_t);
    for (uint32_t i = 0; i < xsdt_entry_count; i++) {
        sdt_header_t *header = (sdt_header_t *)(xsdt->entries[i] + g_hhdm_offset);
        if (!memcmp((const void *)header->signature, "APIC", 4)) {
            return (void *)header;
        }
    }

    return NULL;
}

void acpi_init(struct limine_rsdp_response *response, uint64_t *pml4) {
    rsdp_t *rsdp = (rsdp_t *)response->address;
    xsdt_t *xsdt = (xsdt_t *)(rsdp->xsdt_paddr + g_hhdm_offset);
    void *result = find_madt(xsdt);
    if (!result) {
        panic("acpi_init: MADT not found");
    }

    madt_t *madt = (madt_t *)result;
    uint8_t *ptr = madt->entries;
    uint8_t *end = (uint8_t *)madt + madt->header.length;
    while (ptr < end) {
        madt_entry_header_t *entry = (madt_entry_header_t *)ptr;
        switch (entry->type) {
            // I/O APIC
            case 1: {
                if (g_ioapic_count < IOAPIC_ENTRY_MAX) {
                    g_ioapic_entries[g_ioapic_count++] = *(madt_entry_ioapic_t *)entry;
                    // I/O APIC はここでマップしておく
                    vmm_map(
                        pml4,
                        paddr_to_vaddr(g_ioapic_entries[g_ioapic_count - 1].io_apic_paddr),
                        g_ioapic_entries[g_ioapic_count - 1].io_apic_paddr,
                        PAGE_PRESENT | PAGE_RW | PAGE_NX | PAGE_PWT | PAGE_PCD
                    );
                }
                break;
            }
            // Interrupt Source Override
            case 2: {
                if (g_iso_count < ISO_ENTRY_MAX) {
                    g_iso_entries[g_iso_count++] = *(madt_entry_iso_t *)entry;
                    madt_entry_iso_t *iso = (madt_entry_iso_t *)entry;
                }
                break;
            }
        }
        ptr += entry->length;
    }
}

size_t acpi_ioapic_entries_num(void) {
    return g_ioapic_count;
}

size_t acpi_iso_entries_num(void) {
    return g_iso_count;
}

const madt_entry_iso_t *acpi_find_iso(uint8_t irq) {
    for (size_t i = 0; i < g_iso_count; i++) {
        if (g_iso_entries[i].irq == irq) {
            return &g_iso_entries[i];
        }
    }
    return NULL;
}

const madt_entry_ioapic_t *acpi_find_ioapic(uint32_t gsi) {
    for (size_t i = 0; i < g_ioapic_count; i++) {
        // I/O APIC のピン数を取得する
        uint32_t ver = ioapic_read(
            paddr_to_vaddr(g_ioapic_entries[i].io_apic_paddr),
            0x01
        );
        uint32_t pin_count = ((ver >> 16) & 0xff) + 1;
        if (
            g_ioapic_entries[i].gsi_base <= gsi &&
            gsi < g_ioapic_entries[i].gsi_base + pin_count
        ) {
            return &g_ioapic_entries[i];
        }
    }
    return NULL;
}