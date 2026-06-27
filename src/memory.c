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
        struct limine_memmap_entry *entry 
            = (struct limine_memmap_entry *)response->entries[i];
        kprint(
            "base=0x%x, length=0x%x, type=%s\n", 
            entry->base, entry->length, memmap_type_names[entry->type]
        );
    }
}