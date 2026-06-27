#include "serial.h"
#include "io.h"

#define PORT 0x3f8  // COM1

void serial_init(void) {
    outb(PORT + 1, 0x00);   // 割り込みをすべて無効化 
    outb(PORT + 3, 0x80);   // DLAB=1 にセット（ボーレート設定モードへ）
    outb(PORT + 0, 0x03);   // 除数 低バイト = 3 -> 38,400 baud
    outb(PORT + 1, 0x00);   // 除数 高バイト = 0 
    outb(PORT + 3, 0x03);   // DLAB をクリア & 8N1 設定
    outb(PORT + 2, 0xc7);   // FIFO 有効化・クリア、14バイト閾値
    outb(PORT + 4, 0x0b);   // IRQ 有効化、RTS/DSR セット
    outb(PORT + 4, 0x0f);   // 通常動作モードへ
}

static int is_transmit_empty(void) {
    return inb(PORT + 5) & 0x20;
}

void serial_write_char(char c) {
    while (!is_transmit_empty());    // 既に行われている送信が完了するまで待つ
    outb(PORT, c);
}

void serial_write_string(const char *s) {
    while (*s) {
        serial_write_char(*s++);
    }
}