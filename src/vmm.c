#include <limine.h>
#include <stdbool.h>

#include "vmm.h"
#include "panic.h"
#include "pmm.h"
#include "string.h"
#include "console.h"

#define PML4_INDEX(vaddr) (((vaddr) >> 39) & 0x1ff)
#define PDPT_INDEX(vaddr) (((vaddr) >> 30) & 0x1ff)
#define PD_INDEX(vaddr)   (((vaddr) >> 21) & 0x1ff)
#define PT_INDEX(vaddr)   (((vaddr) >> 12) & 0x1ff)
#define PAGE_PADDR_MASK 0x000ffffffffff000ull

#define PML4_LEVEL 0
#define PDPT_LEVEL 1
#define PD_LEVEL   2
#define PT_LEVEL   3

extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];
extern uint8_t _limine_requests_start[];
extern uint8_t _limine_requests_end[];
extern uint8_t _text_start[];
extern uint8_t _text_end[];
extern uint8_t _rodata_start[];
extern uint8_t _rodata_end[];
extern uint8_t _data_start[];
extern uint8_t _data_end[];

/** 
 * 各インデックスを計算
 * PML4 → PDPT → PD → PT の順に辿る
 * 各レベルでエントリが未設定なら pmm_alloc() で次レベルテーブル作成かつゼロクリア
 * PD または PT レベルで paddr | flags を書き込む
 * 実質的なアクセス制御は終端エントリ (huge page PD, PT) で行うので、
 * 中間エントリ (PML4, PDPT) は VMM 側で固定的に PAGE_PRESENT | PAGE_RW を付与しておく
 */
static void set_table_entry(uint8_t level, uint64_t *table_vaddr, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    switch (level) {
        case PML4_LEVEL: {
            uint64_t pml4_index = PML4_INDEX(vaddr); 
            if (!(table_vaddr[pml4_index] & PAGE_PRESENT)) {
                uint64_t next_table_paddr = (uint64_t)pmm_alloc();
                void *next_table_vaddr = paddr_to_vaddr(next_table_paddr);
                memset(next_table_vaddr, 0, PAGE_FRAME_SIZE_BYTE);
                table_vaddr[pml4_index] = next_table_paddr | PAGE_PRESENT | PAGE_RW;
            }
            uint64_t *next_entry_vaddr = (uint64_t *)paddr_to_vaddr(table_vaddr[pml4_index] & PAGE_PADDR_MASK);
            set_table_entry(PDPT_LEVEL, next_entry_vaddr, vaddr, paddr, flags);
            break;
        }
        case PDPT_LEVEL: {
            uint64_t pdpt_index = PDPT_INDEX(vaddr); 
            if (!(table_vaddr[pdpt_index] & PAGE_PRESENT)) {
                uint64_t next_table_paddr = (uint64_t)pmm_alloc();
                void *next_table_vaddr = paddr_to_vaddr(next_table_paddr);
                memset(next_table_vaddr, 0, PAGE_FRAME_SIZE_BYTE);
                table_vaddr[pdpt_index] = next_table_paddr | PAGE_PRESENT | PAGE_RW;
            }
            uint64_t *next_entry_vaddr = (uint64_t *)paddr_to_vaddr(table_vaddr[pdpt_index] & PAGE_PADDR_MASK);
            set_table_entry(PD_LEVEL, next_entry_vaddr, vaddr, paddr, flags);
            break;
        }
        case PD_LEVEL: {
            uint64_t pd_index = PD_INDEX(vaddr); 
            if (flags & PAGE_HUGE) {
                table_vaddr[pd_index] = paddr | flags;
            } else {
                if (!(table_vaddr[pd_index] & PAGE_PRESENT)) {
                    uint64_t next_table_paddr = (uint64_t)pmm_alloc();
                    void *next_table_vaddr = paddr_to_vaddr(next_table_paddr);
                    memset(next_table_vaddr, 0, PAGE_FRAME_SIZE_BYTE);
                    table_vaddr[pd_index] = next_table_paddr | PAGE_PRESENT | PAGE_RW;
                }
                uint64_t *next_entry_vaddr = (uint64_t *)paddr_to_vaddr(table_vaddr[pd_index] & PAGE_PADDR_MASK);
                set_table_entry(PT_LEVEL, next_entry_vaddr, vaddr, paddr, flags);
            }
            break;
        }
        case PT_LEVEL: {
            uint64_t pt_index = PT_INDEX(vaddr); 
            table_vaddr[pt_index] = paddr | flags;
            break;
        }
        default: {
            panic("vmm.c: unreachable here");
            break;
        }
    }
}

void vmm_map(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    set_table_entry(PML4_LEVEL, pml4, vaddr, paddr, flags);
}

static bool hhdm_mappable(struct limine_memmap_entry *entry) {
    uint64_t type = entry->type;
    if (
        type == LIMINE_MEMMAP_USABLE ||
        type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
        type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES ||
        type == LIMINE_MEMMAP_FRAMEBUFFER ||
        type == LIMINE_MEMMAP_RESERVED_MAPPED ||
        type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
        type == LIMINE_MEMMAP_ACPI_NVS
    ) {
        return true;
    }
    return false;
}

/**
 * カーネルのページテーブルを初期化する
 * 仮定：4 KiB ページ only
 */
uint64_t *vmm_init(
    struct limine_executable_address_response *exe_addr_response,
    struct limine_memmap_response *memmap_response
) {
    // pmm_alloc() で PML4 を1ページフレーム確保してゼロクリア
    uint64_t pml4_paddr = (uint64_t)pmm_alloc();
    uint64_t *pml4_vaddr = (uint64_t *)paddr_to_vaddr(pml4_paddr);
    memset((void *)pml4_vaddr, 0, PAGE_FRAME_SIZE_BYTE);

    // HHDM 全体をマップ
    for (uint64_t i = 0; i < memmap_response->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap_response->entries[i];
        if (hhdm_mappable(entry)) {
            uint64_t region_size = entry->length;
            for (uint64_t offset = 0; offset < region_size; offset += PAGE_FRAME_SIZE_BYTE) {
                vmm_map(
                    pml4_vaddr,
                    (uint64_t)paddr_to_vaddr(entry->base + offset),
                    entry->base + offset,
                    PAGE_PRESENT | PAGE_RW
                );
            }
        }
    }

    // カーネル自身をマップ
    uint64_t kernel_size_byte = (uint64_t)_kernel_end - (uint64_t)_kernel_start;
    for (uint64_t offset = 0; offset < kernel_size_byte; offset += PAGE_FRAME_SIZE_BYTE) {
        // limine_requests: RW
        if (
            (uint64_t)_limine_requests_start <= exe_addr_response->virtual_base + offset &&
            exe_addr_response->virtual_base + offset < (uint64_t)_limine_requests_end
        ) {
            vmm_map(
                pml4_vaddr,
                exe_addr_response->virtual_base + offset,
                exe_addr_response->physical_base + offset,
                PAGE_PRESENT | PAGE_RW | PAGE_NX
            );
        } 
        // text: RX
        else if (
            (uint64_t)_text_start <= exe_addr_response->virtual_base + offset &&
            exe_addr_response->virtual_base + offset < (uint64_t)_text_end
        ) {
            vmm_map(
                pml4_vaddr,
                exe_addr_response->virtual_base + offset,
                exe_addr_response->physical_base + offset,
                PAGE_PRESENT
            );
        }
        // rodata: RO
        else if (
            (uint64_t)_rodata_start <= exe_addr_response->virtual_base + offset &&
            exe_addr_response->virtual_base + offset < (uint64_t)_rodata_end
        ) {
            vmm_map(
                pml4_vaddr,
                exe_addr_response->virtual_base + offset,
                exe_addr_response->physical_base + offset,
                PAGE_PRESENT | PAGE_NX
            );
        }
        // data: RW
        else if (
            (uint64_t)_data_start <= exe_addr_response->virtual_base + offset &&
            exe_addr_response->virtual_base + offset < (uint64_t)_data_end
        ) {
            vmm_map(
                pml4_vaddr,
                exe_addr_response->virtual_base + offset,
                exe_addr_response->physical_base + offset,
                PAGE_PRESENT | PAGE_RW | PAGE_NX
            );
        }
        // その他: RWX
        else {
            vmm_map(
                pml4_vaddr,
                exe_addr_response->virtual_base + offset,
                exe_addr_response->physical_base + offset,
                PAGE_PRESENT | PAGE_RW
            );
        }
    }

    // CR3 に PML4 の物理アドレスをセットして切り替え
    __asm__ volatile ("mov cr3, %[pml4]" : : [pml4] "r"(pml4_paddr));

    return (uint64_t *)pml4_vaddr;
}

void vmm_dump_entry(uint64_t *pml4, uint64_t vaddr) {
    uint64_t pml4_index = PML4_INDEX(vaddr);
    if (!(pml4[pml4_index] & PAGE_PRESENT)) {
        kprint("PML4E not present\n");
        return;
    }

    uint64_t *pdpt = (uint64_t *)paddr_to_vaddr(pml4[pml4_index] & PAGE_PADDR_MASK);
    uint64_t pdpt_index = PDPT_INDEX(vaddr);
    if (!(pdpt[pdpt_index] & PAGE_PRESENT)) {
        kprint("PDPTE not present\n");
        return;
    }

    uint64_t *pd = (uint64_t *)paddr_to_vaddr(pdpt[pdpt_index] & PAGE_PADDR_MASK);
    uint64_t pd_index = PD_INDEX(vaddr);
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        kprint("PDE not present\n");
        return;
    }
    if (pd[pd_index] & PAGE_HUGE) {
        kprint("PDE (huge) = 0x%lx\n", pd[pd_index]);
        return;
    }

    uint64_t *pt = (uint64_t *)paddr_to_vaddr(pd[pd_index] & PAGE_PADDR_MASK);
    uint64_t pt_index = PT_INDEX(vaddr);
    kprint("PTE = 0x%lx\n", pt[pt_index]);
}