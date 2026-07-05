#pragma once

#include <stdbool.h>
#include "interrupt.h"

void keyboard_set_shift(bool pressed);
bool keyboard_is_shift_pressed(void);

void keyboard_handler(interrupt_frame_t *frame);