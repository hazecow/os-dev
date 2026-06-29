#include <stdbool.h>

#include "pmm.h"
#include "console.h"
#include "panic.h"
#include "string.h"

#define PAGE_FRAME_SIZE_BYTE 4096

static uint8_t *g_bitmap;
static uint64_t g_hhdm_offset;
static uint64_t g_bitmap_size_byte;

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

    for (uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry *entry = response->entries[i];
        const char *type_name = (entry->type < sizeof(memmap_type_names) / sizeof(memmap_type_names[0]))
            ? memmap_type_names[entry->type] : "UNKNOWN";

        kprint(
            "base=0x%lx, length=0x%lx, type=%s\n", 
            entry->base, entry->length, type_name
        );
    }
    kprint("==============\n");    
}

static bool is_available(struct limine_memmap_entry *entry) {
    uint64_t type = entry->type;
    return (type == LIMINE_MEMMAP_USABLE || type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE);
}

static void bitmap_set(uint64_t frame) {
    g_bitmap[frame / 8] |= (1 << (frame % 8));
}

static void bitmap_clear(uint64_t frame) {
    g_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static bool bitmap_get(uint64_t frame) {
    return (g_bitmap[frame / 8] >> (frame % 8)) & 1;
}

// start_frame を先頭フレームとする nframes フレームを使用中(1)にする
static void bitmap_set_range(uint64_t start_frame, uint64_t nframes) {
    for (uint64_t i = 0; i < nframes; i++) {
        bitmap_set(start_frame + i);
    }
}

// start_frame を先頭フレームとする nframes フレームを未使用(0)にする
static void bitmap_clear_range(uint64_t start_frame, uint64_t nframes) {
    for (uint64_t i = 0; i < nframes; i++) {
        bitmap_clear(start_frame + i);
    }
}

static uint64_t div_ceil(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

void pmm_init(struct limine_memmap_response *response, uint64_t hhdm_offset) {
    g_hhdm_offset = hhdm_offset;

    // メモリマップを走査し highest_paddr を求める
    uint64_t highest_paddr = 0;
    for (uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry *entry = response->entries[i];
        if (is_available(entry) && highest_paddr < entry->base + entry->length) {
            highest_paddr = entry->base + entry->length;
        }
    }

    // ビットマップを表現するのに必要なバイト数 bitmap_size_byte を計算する
    g_bitmap_size_byte = div_ceil(div_ceil(highest_paddr, PAGE_FRAME_SIZE_BYTE), 8);

    // ビットマップが収まる USABLE な領域を探して先頭に配置する
    for (uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry *entry = response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && g_bitmap_size_byte < entry->length) {
            g_bitmap = (uint8_t *)(g_hhdm_offset + entry->base);
            break;
        }
    }
    if (!g_bitmap) {
        panic("pmm_init: bitmap could not be allocated");
    }

    // ビットマップ全体を全ページフレーム使用中として初期化する
    memset((void *)g_bitmap, 0xff, (size_t)g_bitmap_size_byte);

    // USABLE なエントリのページフレームを未使用にマークする
    for (uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry *entry = response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_frame = entry->base / PAGE_FRAME_SIZE_BYTE;
            uint64_t nframes = entry->length / PAGE_FRAME_SIZE_BYTE;
            bitmap_clear_range(start_frame, nframes);
        }
    }

    // ビットマップ自身が占めるページフレームを再度使用中にマークする
    uint64_t bitmap_paddr = (uint64_t)g_bitmap - g_hhdm_offset;
    uint64_t bitmap_start_frame = bitmap_paddr / PAGE_FRAME_SIZE_BYTE;
    uint64_t bitmap_nframes = div_ceil(g_bitmap_size_byte, PAGE_FRAME_SIZE_BYTE);
    bitmap_set_range(bitmap_start_frame, bitmap_nframes);
}

void *pmm_alloc(void) {
    for (uint64_t i = 0; i < g_bitmap_size_byte; i++) {
        for (uint8_t j = 0; j < 8; j++) {
            uint64_t frame = 8 * i + j;
            // 未使用なフレームを見つけたら使用中に変更し、ページフレーム先頭の物理アドレスを返す
            if (!bitmap_get(frame)) {
                bitmap_set(frame);
                return (void *)(frame * PAGE_FRAME_SIZE_BYTE);
            }
        }
    }
    return NULL;
}

void pmm_free(void *paddr) {
    // アライメントチェック
    if ((uint64_t)paddr % PAGE_FRAME_SIZE_BYTE != 0) {
        panic("pmm_free: unaligned address");
    }
    uint64_t frame = (uint64_t)paddr / PAGE_FRAME_SIZE_BYTE;
    bitmap_clear(frame);
}

void pmm_dump_stats(void) {
    kprint("=== PMM STATS ===\n");
    kprint("size of a page frame [KB]: %d\n", PAGE_FRAME_SIZE_BYTE / 1024);
    kprint("total page frames: %ld\n", g_bitmap_size_byte * 8);
    kprint("bitmap page frames: %ld\n", div_ceil(g_bitmap_size_byte, PAGE_FRAME_SIZE_BYTE));

    uint64_t free_frames = 0;
    for (uint64_t i = 0; i < g_bitmap_size_byte * 8; i++) {
        if (!bitmap_get(i)) free_frames++;
    }
    kprint("free page frames: %ld\n", free_frames);
    kprint("=================\n");
}