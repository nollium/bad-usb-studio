#pragma once

#include <stdbool.h>
#include <stdint.h>

// Abstract HID keyboard transport (USB or BLE).
// Key values use the SDK's combined format: keycode | KEY_MOD_* flags.
typedef struct {
    bool (*init)(void);
    void (*deinit)(void);
    bool (*is_connected)(void);
    void (*kb_press)(uint16_t key);
    void (*kb_release)(uint16_t key);
    void (*kb_release_all)(void);
} HidTransport;

extern const HidTransport hid_transport_usb;
extern const HidTransport hid_transport_ble;
