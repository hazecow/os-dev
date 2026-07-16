#include <stdbool.h>
#include <stdint.h>

#include "io.h"
#include "panic.h"
#include "ps2.h"

// 出力バッファが満たされるまで (ステータスバッファ 0x64 の bit 0 が立つまで)
// 待つ
static void ps2_wait_output(void) {
  while (!(inb(0x64) & 0x01))
    ;
}

void ps2_init(void) {
  // 初期化のために各 port を無効化
  outb(0x64, 0xad); // first port
  outb(0x64, 0xa7); // second port

  // バッファフラッシュ
  inb(0x60);

  // controller config byte を設定する
  // interrupt (bit 0) をクリアして無効化し
  // clock signal (bit 4) をクリアして有効化する
  outb(0x64, 0x20);
  uint8_t config = inb(0x60);
  config &= ~(0x01 | 0x10);
  outb(0x64, 0x60);
  outb(0x60, config);

  // セルフテストを行う
  outb(0x64, 0xaa);
  ps2_wait_output();
  uint8_t result = inb(0x60);
  if (result != 0x55) {
    panic("ps2_init: self test failed: 0x%x", result);
  }

  // 2チャンネルかどうかの判定
  // 0xA8 を送って second port を有効化し、config byte の bit 5 を確認する
  // bit 5 がクリアなら dual channel である
  // その場合、0xA7 で再度無効化ののち bit 1, 5 をクリアし、second port の
  // interrupt を無効化、clock を有効化する
  bool is_dual_channel = false;
  outb(0x64, 0xa8);
  outb(0x64, 0x20);
  config = inb(0x60);
  if (!(config & 0x20)) {
    is_dual_channel = true;
    outb(0x64, 0xa7);

    outb(0x64, 0x20);
    config = inb(0x60);
    config &= ~(0x02 | 0x20);
    outb(0x64, 0x60);
    outb(0x60, config);
  }

  // インターフェーステスト
  // first port
  outb(0x64, 0xab);
  ps2_wait_output();
  result = inb(0x60);
  if (result != 0x00) {
    switch (result) {
    case 0x01: {
      panic("ps2_init: first port clock line stuck low");
      break;
    }
    case 0x02: {
      panic("ps2_init: first port clock line stuck high");
      break;
    }
    case 0x03: {
      panic("ps2_init: first port data line stuck low");
      break;
    }
    case 0x04: {
      panic("ps2_init: first port data line stuck high");
      break;
    }
    }
  }
  // second port (if supported)
  if (is_dual_channel) {
    outb(0x64, 0xa9);
    ps2_wait_output();
    result = inb(0x60);
    if (result != 0x00) {
      switch (result) {
      case 0x01: {
        panic("ps2_init: second port clock line stuck low");
        break;
      }
      case 0x02: {
        panic("ps2_init: second port clock line stuck high");
        break;
      }
      case 0x03: {
        panic("ps2_init: second port data line stuck low");
        break;
      }
      case 0x04: {
        panic("ps2_init: second port data line stuck high");
        break;
      }
      }
    }
  }

  // 各 port を有効化する (interupt 有効化も行う)
  // first port
  outb(0x64, 0xae);
  outb(0x64, 0x20);
  config = inb(0x60);
  config |= 0x01;
  outb(0x64, 0x60);
  outb(0x60, config);
  // second port (if supported)
  if (is_dual_channel) {
    outb(0x64, 0xa8);
    outb(0x64, 0x20);
    config = inb(0x60);
    config |= 0x02;
    outb(0x64, 0x60);
    outb(0x60, config);
  }
}
