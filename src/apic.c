#include "pmm.h"
#include "vmm.h"
#include "console.h"
#include "interrupt.h"
#include "apic.h"
#include "io.h"

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

static uint64_t g_apic_base_paddr;
static uint64_t g_apic_base_vaddr;
static uint32_t g_apic_freq;

// IA32_APIC_BASE MSR から APIC base addr を取得する
static uint64_t get_apic_base(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=d"(hi), "=a"(lo) : "c"(IA32_APIC_BASE_MSR));

    // 4KiBアラインなので (MAXPHYADDR-1):12 bits は残して 11:0 bits は落とす
    return ((((uint64_t)hi << 32) | lo) & 0x000ffffffffff000ull);
}

uint32_t read_lapic_reg(uint64_t offset) {
    volatile uint32_t *reg_vaddr = (volatile uint32_t *)(g_apic_base_vaddr + offset);
    return *reg_vaddr;
}

void write_lapic_reg(uint64_t offset, uint32_t value) {
    volatile uint32_t *reg_vaddr = (volatile uint32_t *)(g_apic_base_vaddr + offset);
    *reg_vaddr = value;
}

// spurious 割込みハンドラは何もしない
static void spurious_handler(interrupt_frame_t *frame) {
    (void)frame;
}

static void measure_apic_timer_freq(void) {
    // PIT をワンショットモードに設定 (0x43 に 0x30 を書く)
    outb(0x43, 0x30);

    // PIT のカウント初期値をセット (0x40 に 0xff, 0xff を書く = 65535)
    outb(0x40, 0xff);
    outb(0x40, 0xff);

    // APIC タイマーの分周比をセット
    write_lapic_reg(DIV_CONFIG_OFFSET, (uint32_t)APIC_DIVISION_RATIO_16);

    // APIC タイマーの Initial Count に大きな値をセット (カウント開始)
    uint32_t apic_init_count = 0xffffffff;
    write_lapic_reg(INIT_COUNT_OFFSET, apic_init_count);

    // PIT が一定カウント減るまで待つ (ポーリング)
    uint16_t count;
    do {
        outb(0x43, 0x00);
        uint8_t lo = inb(0x40);
        uint8_t hi = inb(0x40);
        count = ((uint16_t)hi << 8) | lo;
    } while (count > (PIT_INIT_COUNT - PIT_WAIT_COUNT_10MS));

    // APIC タイマーの Current Count を読む
    uint32_t apic_cur_count = read_lapic_reg(CURRENT_COUNT_OFFSET);

    // タイマーの周波数を計算する
    uint64_t pit_lapsed = PIT_INIT_COUNT - count;
    uint64_t apic_lapsed = (uint64_t)apic_init_count - (uint64_t)apic_cur_count;
    uint64_t freq = (uint64_t)PIT_FREQ * apic_lapsed / (uint64_t)pit_lapsed;
    g_apic_freq = (uint32_t)freq;
}

static void lvt_timer_handler(interrupt_frame_t *frame) {
    kprint("APIC timer: time out!\n");
    
    // EOI を送る
    write_lapic_reg(EOI_OFFSET, 0);
}

static void set_lvt_timer(void) {
    // タイマー割り込み (32) を IDT に登録する
    interrupt_register(32, (uintptr_t)isr_stub_32, lvt_timer_handler);
    
    // Initial Counter Register を動作周波数に合わせる 
    // 1秒ごとにタイマ割り込み処理が発生することを期待
    write_lapic_reg(DIV_CONFIG_OFFSET, (uint32_t)APIC_DIVISION_RATIO_16);
    write_lapic_reg(INIT_COUNT_OFFSET, g_apic_freq);

    // LVT Timer レジスタをセットする
    uint32_t lvt_timer_reg_value =
        ((uint32_t)0b01 << 17) | // timer: periodic
        ((uint32_t)0 << 16)    | // mask: not masked
        (uint32_t)32;            // vector
    write_lapic_reg(LVT_TIMER_OFFSET, lvt_timer_reg_value);
}

void apic_init(uint64_t *pml4) {
    // APIC base address をマッピングする
    // メモリ属性が UC (PAT3) になるように設定する (PAT=0, PCD=1, PWT=1)
    g_apic_base_paddr = get_apic_base();
    g_apic_base_vaddr = (uint64_t)paddr_to_vaddr(g_apic_base_paddr);
    vmm_map(
        pml4,
        (uint64_t)paddr_to_vaddr(g_apic_base_paddr),
        g_apic_base_paddr,
        PAGE_PRESENT | PAGE_RW | PAGE_NX | PAGE_PCD | PAGE_PWT   
    );

    // SVR の値を表示する
    kprint("SVR: 0x%x\n", read_lapic_reg(SVR_OFFSET));

    // Spurious interrupt (255) を IDT に登録する
    interrupt_register(255, (uintptr_t)isr_stub_255, spurious_handler);

    // APIC タイマーの周波数を測定する
    measure_apic_timer_freq();
    kprint("APIC timer freq: %d\n", g_apic_freq);

    // LVT Timer を Periodic モードで設定してアンマスクする
    set_lvt_timer();
}