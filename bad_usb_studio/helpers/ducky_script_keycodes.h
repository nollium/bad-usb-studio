#pragma once

// Pull in the full HAL which includes furi_hal_usb_hid.h with proper dependencies.
// This gives us: HID_KEYBOARD_*, KEY_MOD_*, hid_asciimap[], HID_ASCII_TO_KEY()
#include <furi_hal.h>
