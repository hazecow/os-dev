#pragma once

#include "keycode.h"

/**
 * 中間コードから JIS 配列文字への変換を試みる
 * 成功すれば out に ASCII 文字を格納し true を返す
 * 失敗すれば (中間コードに対応する ASCII 文字が用意されていなければ) false を返す
 */
bool keycode_to_char_jis(keycode_t kc, bool shift, char *out);