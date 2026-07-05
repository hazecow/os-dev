#include "layout_jis.h"

bool keycode_to_char_jis(keycode_t kc, bool shift, char *out) {
    // アルファベットや数字は列挙子の並びを利用したオフセット計算でシンプルに実装可能
    if (kc >= KEY_A && kc <= KEY_Z) {
        char base = shift ? 'A' : 'a';
        *out = base + (kc - KEY_A);
        return true;
    }
    else if (kc >= KEY_0 && kc <= KEY_9) {
        if (shift) {
            switch (kc) {
                case KEY_0: {
                    return false;
                }
                case KEY_1: {
                    *out = '!';
                    return true;
                }
                case KEY_2: {
                    *out = '"';
                    return true;
                }
                case KEY_3: {
                    *out = '#';
                    return true;
                }
                case KEY_4: {
                    *out = '$';
                    return true;
                }
                case KEY_5: {
                    *out = '%';
                    return true;
                }
                case KEY_6: {
                    *out = '&';
                    return true;
                }
                case KEY_7: {
                    *out = '\'';
                    return true;
                }
                case KEY_8: {
                    *out = '(';
                    return true;
                }
                case KEY_9: {
                    *out = ')';
                    return true;
                }
            }
        }
        else {
            *out = '0' + (kc - KEY_0);
            return true;
        }
    }
    else {
        switch (kc) {
            case KEY_MINUS: {
                *out = shift ? '=' : '-';
                return true;
            }
            case KEY_EQUAL: {
                *out = shift ? '~' : '^';
                return true;
            }
            case KEY_LBRACKET: {
                *out = shift ? '`' : '@';
                return true;
            }
            case KEY_RBRACKET: {
                *out = shift ? '{' : '[';
                return true;
            }
            case KEY_SQUOTE: {
                *out = shift ? '*' : ':';
                return true;
            }
            case KEY_FSLASH: {
                *out = shift ? '?' : '/';
                return true;
            }
            case KEY_BSLASH: {
                *out = shift ? '}' : ']';
                return true;
            }
            case KEY_SEMICOLON: {
                *out = shift ? '+' : ';';
                return true;
            }
            case KEY_COMMA: {
                *out = shift ? '<' : ',';
                return true;
            }
            case KEY_DOT: {
                *out = shift ? '>' : '.';
                return true;
            }
            case KEY_SPACE: {
                *out = ' ';
                return true;
            }
            case KEY_ENTER: {
                *out = '\n';
                return true;
            }
            case KEY_TAB: {
                *out = '\t';
                return true;
            }
            default: {
                return false;
            }
        }
    }
}