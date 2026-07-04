#include <stddef.h>

#include "pmm.h"
#include "vmm.h"
#include "console.h"
#include "interrupt.h"
#include "apic.h"
#include "io.h"
#include "acpi.h"
#include "panic.h"
#include "serial.h"
#include "ps2.h"

#define IA32_APIC_BASE_MSR   0x1b

#define LAPIC_ID_OFFSET      0x0020
#define TPR_OFFSET           0x0080
#define EOI_OFFSET           0x00b0
#define SVR_OFFSET           0x00f0
#define LVT_TIMER_OFFSET     0x0320
#define INIT_COUNT_OFFSET    0x0380
#define CURRENT_COUNT_OFFSET 0x0390
#define DIV_CONFIG_OFFSET    0x03e0

// PIT の初期値
#define PIT_INIT_COUNT 0xffff // 65535

// PIT の周波数
#define PIT_FREQ 1193182

// 待機カウント数 (測定時間)
#define PIT_WAIT_COUNT_10MS 11932

// APIC タイマーの分周比
// Intel SDM の Divide Configuration Register を参照
#define APIC_DIVISION_RATIO_2    0b0000
#define APIC_DIVISION_RATIO_4    0b0001
#define APIC_DIVISION_RATIO_8    0b0010
#define APIC_DIVISION_RATIO_16   0b0011
#define APIC_DIVISION_RATIO_32   0b1000
#define APIC_DIVISION_RATIO_64   0b1001
#define APIC_DIVISION_RATIO_128  0b1010
#define APIC_DIVISION_RATIO_1    0b1011

extern void isr_stub_255(void);
extern void isr_stub_32(void);
extern void isr_stub_33(void);

static uint64_t g_lapic_base_paddr;
static uint64_t g_lapic_base_vaddr;
static uint32_t g_lapic_freq;

// IA32_APIC_BASE MSR から Local APIC base addr を取得する
static uint64_t get_lapic_base(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=d"(hi), "=a"(lo) : "c"(IA32_APIC_BASE_MSR));

    // 4KiBアラインなので (MAXPHYADDR-1):12 bits は残して 11:0 bits は落とす
    return ((((uint64_t)hi << 32) | lo) & 0x000ffffffffff000ull);
}

uint32_t lapic_read(uint64_t offset) {
    volatile uint32_t *reg_vaddr = (volatile uint32_t *)(g_lapic_base_vaddr + offset);
    return *reg_vaddr;
}

void lapic_write(uint64_t offset, uint32_t value) {
    volatile uint32_t *reg_vaddr = (volatile uint32_t *)(g_lapic_base_vaddr + offset);
    *reg_vaddr = value;
}

// spurious 割込みハンドラは何もしない
static void spurious_handler(interrupt_frame_t *frame) {
    (void)frame;
}

static void measure_lapic_timer_freq(void) {
    // PIT をワンショットモードに設定 (0x43 に 0x30 を書く)
    outb(0x43, 0x30);

    // PIT のカウント初期値をセット (0x40 に 0xff, 0xff を書く = 65535)
    outb(0x40, 0xff);
    outb(0x40, 0xff);

    // APIC タイマーの分周比をセット
    lapic_write(DIV_CONFIG_OFFSET, (uint32_t)APIC_DIVISION_RATIO_16);

    // APIC タイマーの Initial Count に大きな値をセット (カウント開始)
    uint32_t apic_init_count = 0xffffffff;
    lapic_write(INIT_COUNT_OFFSET, apic_init_count);

    // PIT が一定カウント減るまで待つ (ポーリング)
    uint16_t count;
    do {
        outb(0x43, 0x00);
        uint8_t lo = inb(0x40);
        uint8_t hi = inb(0x40);
        count = ((uint16_t)hi << 8) | lo;
    } while (count > (PIT_INIT_COUNT - PIT_WAIT_COUNT_10MS));

    // APIC タイマーの Current Count を読む
    uint32_t apic_cur_count = lapic_read(CURRENT_COUNT_OFFSET);

    // タイマーの周波数を計算する
    uint64_t pit_lapsed = PIT_INIT_COUNT - count;
    uint64_t apic_lapsed = (uint64_t)apic_init_count - (uint64_t)apic_cur_count;
    uint64_t freq = (uint64_t)PIT_FREQ * apic_lapsed / (uint64_t)pit_lapsed;
    g_lapic_freq = (uint32_t)freq;
}

static void lvt_timer_handler(interrupt_frame_t *frame) {
    (void)frame;
    kprint("APIC timer: time out!\n");
    lapic_send_eoi();
}

static void set_lvt_timer(void) {
    // タイマー割り込み (32) を IDT に登録する
    interrupt_register(32, (uintptr_t)isr_stub_32, lvt_timer_handler);
    
    // Initial Counter Register を動作周波数に合わせる 
    // 1秒ごとにタイマ割り込み処理が発生することを期待
    lapic_write(DIV_CONFIG_OFFSET, (uint32_t)APIC_DIVISION_RATIO_16);
    lapic_write(INIT_COUNT_OFFSET, g_lapic_freq);

    // LVT Timer レジスタをセットする
    uint32_t lvt_timer_reg_value =
        ((uint32_t)0b01 << 17) | // timer: periodic
        ((uint32_t)0 << 16)    | // mask: not masked
        (uint32_t)32;            // vector
    lapic_write(LVT_TIMER_OFFSET, lvt_timer_reg_value);
}

void lapic_init(uint64_t *pml4) {
    // PIC を 0x20/0x28 にリマップして全マスク
    outb(0x20, 0x11); outb(0xa0, 0x11); // ICW1: init
    outb(0x21, 0x20); outb(0xa1, 0x28); // ICW2: vector offset
    outb(0x21, 0x04); outb(0xa1, 0x02); // ICW3: cascade
    outb(0x21, 0x01); outb(0xa1, 0x01); // ICW4: 8086 mode
    outb(0x21, 0xff); outb(0xa1, 0xff); // マスク全て
    // PIC の pending をクリア
    outb(0x20, 0x20); // master PIC EOI
    outb(0xa0, 0x20); // slave PIC EOI

    // Local APIC base address をマッピングする
    // メモリ属性が UC (PAT3) になるように設定する (PAT=0, PCD=1, PWT=1)
    g_lapic_base_paddr = get_lapic_base();
    g_lapic_base_vaddr = (uint64_t)paddr_to_vaddr(g_lapic_base_paddr);
    vmm_map(
        pml4,
        (uint64_t)paddr_to_vaddr(g_lapic_base_paddr),
        g_lapic_base_paddr,
        PAGE_PRESENT | PAGE_RW | PAGE_NX | PAGE_PCD | PAGE_PWT   
    );

    // Spurious interrupt (255) を IDT に登録する
    interrupt_register(255, (uintptr_t)isr_stub_255, spurious_handler);

    // APIC タイマーの周波数を測定する
    measure_lapic_timer_freq();
    kprint("APIC timer freq: %d\n", g_lapic_freq);

    // LVT Timer を Periodic モードで設定してアンマスクする
    set_lvt_timer();
}

uint32_t ioapic_read(uint64_t ioapic_vaddr, uint32_t reg) {
    // IOREGSEL (0x00) に読み込むレジスタ reg を指定
    *(volatile uint32_t *)(ioapic_vaddr + 0x00) = reg;
    // IOWIN (0x10) からレジスタ reg の内容を読み込む
    return *(volatile uint32_t *)(ioapic_vaddr + 0x10);
}

void ioapic_write(uint64_t ioapic_vaddr, uint32_t reg, uint32_t value) {
    // IOREGSEL (0x00) に書き込むレジスタ reg を指定
    *(volatile uint32_t *)(ioapic_vaddr + 0x00) = reg;
    // IOWIN (0x10) からレジスタ reg に value を読み込む
    *(volatile uint32_t *)(ioapic_vaddr + 0x10) = value;
}

void ioapic_redirect(uint8_t irq, uint8_t vector) {
    // ISO エントリがない場合の処理に注意
    const madt_entry_iso_t *iso_entry = acpi_find_iso(irq);
    uint32_t gsi         = iso_entry ? iso_entry->gsi                : irq;
    uint8_t polarity     = iso_entry ? (iso_entry->flags.polarity     & 0b10) >> 1 : 0;
    uint8_t trigger_mode = iso_entry ? (iso_entry->flags.trigger_mode & 0b10) >> 1 : 0;

    const madt_entry_ioapic_t *ioapic_entry = acpi_find_ioapic(gsi);
    if (!ioapic_entry) {
        panic("ioapic_redirect: no I/O APIC found for GSI %d", gsi);
    }

    // BSP APIC ID は Local APIC の ID レジスタ (offset 0x20) から取得する
    // Local APIC ID は 31:24 bits にある
    uint8_t destination = (uint8_t)(lapic_read(0x20) >> 24);

    // IOREDTBL に RTE を書く
    uint32_t pin = gsi - ioapic_entry->gsi_base;
    // 下位 32 bit
    ioapic_write(
        (uint64_t)paddr_to_vaddr(ioapic_entry->io_apic_paddr),
        0x10 + 2 * pin,
        (0 << 16) |
        (trigger_mode << 15) |
        (polarity << 13) |
        vector
    );
    // 上位 32 bit
    ioapic_write(
        (uint64_t)paddr_to_vaddr(ioapic_entry->io_apic_paddr),
        0x11 + 2 * pin,
        (uint32_t)destination << 24
    );
}

static void keyboard_handler(interrupt_frame_t *frame) {
    (void)frame;
    uint8_t scancode = inb(0x60);
    (void)scancode;
    kprint("key!\n");
    lapic_send_eoi();
}

/**
 * この関数が呼ばれるより先に acpi_init() が呼ばれているので、
 * 既に I/O APIC のマッピングは完了していることが保証される
 */
void ioapic_init(void) {
    // IRQ1 を vector 33 に紐づけ
    ioapic_redirect(1, 33);
    // 仮ハンドラ登録
    interrupt_register(33, (uintptr_t)isr_stub_33, keyboard_handler);
}

void lapic_send_eoi(void) {
    lapic_write(EOI_OFFSET, 0);
}