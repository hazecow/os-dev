#include "memory.h"
#include "console.h"

static const char *memmap_type_names[] = {
    [LIMINE_MEMMAP_USABLE]  = "USABLE",
    [LIMINE_MEMMAP_RESERVED]  = "RESERVED",
    [LIMINE_MEMMAP_ACPI_RECLAIMABLE]  = "ACPI_RECLAIMABLE",
    [LIMINE_MEMMAP_ACPI_NVS]  = "ACPI_NVS",
    [LIMINE_MEMMAP_BAD_MEMORY]  = "BAD_MEMORY",
    [LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE]  = "BOOTLOADER_RECLAIMABLE",
    [LIMINE_MEMMAP_EXECUTABLE_AND_MODULES]  = "EXECUTABLE_AND_MODULES",
    [LIMINE_MEMMAP_FRAMEBUFFER]  = "FRAMEBUFFER",
    [LIMINE_MEMMAP_RESERVED_MAPPED]  = "RESERVED_MAPPED",
};

void display_memmap(struct limine_memmap_response *response) {
    kprint("=== MEMMAP ===\n");    

    for(uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry *entry = response->entries[i];
        const char *type_name = (entry->type < sizeof(memmap_type_names) / sizeof(memmap_type_names[0]))
            ? memmap_type_names[entry->type] : "UNKNOWN";

        kprint(
            "base=0x%lx, length=0x%lx, type=%s\n", 
            entry->base, entry->length, type_name
        );
    }
}