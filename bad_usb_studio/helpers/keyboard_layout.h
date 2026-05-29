#pragma once

#include <stdint.h>

typedef enum {
    KeyboardLayoutUS,
    KeyboardLayoutFR,
    KeyboardLayoutDE,
    KeyboardLayoutES,
    KeyboardLayoutCount,
} KeyboardLayout;

const char* keyboard_layout_get_name(KeyboardLayout layout);

// Map an ASCII character to a combined HID uint16_t (keycode | KEY_MOD_* flags)
// for the given layout. Returns HID_KEYBOARD_NONE (0) for unmappable chars.
uint16_t keyboard_layout_map_char(KeyboardLayout layout, char c);
