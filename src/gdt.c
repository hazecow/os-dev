#include <stdint.h>

#include "gdt.h"
#include "console.h"

// GDT エントリ (8バイト)
typedef struct {
    uint16_t limit_low;     // Limit[15:0]
    uint16_t base_low;     // Base[15:0]
    uint8_t base_mid;       // Base[23:16]
    uint8_t access;         // Access byte
    uint8_t flags_limit;    // (Flags)[7:4] | (Limit[19:16])[3:0]
    uint8_t base_high;      // Base[31:24]
} __attribute__((packed)) gdt_entry_t;

// GDTR に渡す構造体
typedef struct {
    uint16_t limit;     // GDT のサイズ - 1
    uint64_t base;      // GDT のリニアアドレス
} __attribute__((packed)) gdtr_t;

// 3エントリ: Null / Kernel Code / Kernel Data
static gdt_entry_t gdt[3];
static gdtr_t gdtr;

// lgdt + セレクタリロード（アセンブリ側で定義）
extern void gdt_flush(uint64_t gdtr_addr);

static void gdt_set_entry(
    int idx, 
    uint32_t base, 
    uint32_t limit, 
    uint8_t access, 
    uint8_t flags
) 
{
    gdt[idx].limit_low = (uint16_t)(limit & 0x0000ffff);
    gdt[idx].base_low = (uint16_t)(base & 0x0000ffff);
    gdt[idx].base_mid = (uint8_t)((base >> 16) & 0x00ff);
    gdt[idx].access = access;
    gdt[idx].flags_limit = (uint8_t)((flags << 4) | ((limit >> 16) & 0x000f));
    gdt[idx].base_high = (uint8_t)(base >> 24);
}

void gdt_init(void) {
    // 0: Null descriptor
    gdt_set_entry(0, 0, 0, 0x00, 0x0);

    // 1: Kernel Mode Code Segment (base = 0, limit = 0xFFFFF, access = 0x9A, flags = 0xA)
    // access = 0x9A: P=1, DPL=00, S=1, E=1, DC=0, RW=1, A=0
    // flags  = 0xA:  G=1, DB=0, L=1, prsv=0
    gdt_set_entry(1, 0, 0xfffff, 0x9a, 0xa);

    // 2: Kernel Mode Data Segment (base = 0, limit = 0xFFFFF, access = 0x92, flags = 0xC)
    // access = 0x92: P=1, DPL=00, S=1, E=0, DC=0, RW=1, A=0
    // flags  = 0xC:  G=1, DB=1, L=0, prsv=0
    gdt_set_entry(2, 0, 0xfffff, 0x92, 0xc);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;
    
    gdt_flush((uint64_t)&gdtr);
}

void gdt_dump(void) {
    uint8_t *p = (uint8_t *)gdt;
    for (int i = 0; i < 3; i++) {
        kprint("GDT[%d]: ", i);
        for (int j = 0; j < 8; j++) {
            kprint("%x ", p[i * 8 + j]);
        }
        kprint("\n");
    }
}

void gdtr_verify(void) {
    gdtr_t loaded;
    __asm__ volatile("sgdt %0" : "=m"(loaded));
    kprint("GDTR limit=0x%x base=%p\n", loaded.limit, (void *)loaded.base);
    kprint("Expected: limit=0x%x base=%p\n",
           (uint16_t)(sizeof(gdt) - 1), (void *)&gdt);
}