#include "hid_transport.h"
#include <furi.h>
#include <furi_hal.h>

static FuriHalUsbInterface* prev_usb_mode = NULL;

static bool usb_init(void) {
    prev_usb_mode = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid, NULL));
    // Wait for the host to enumerate the HID device
    for(int i = 0; i < 20; i++) {
        if(furi_hal_hid_is_connected()) break;
        furi_delay_ms(100);
    }
    return true;
}

static void usb_deinit(void) {
    if(prev_usb_mode) {
        furi_hal_usb_set_config(prev_usb_mode, NULL);
        prev_usb_mode = NULL;
    }
}

static bool usb_is_connected(void) {
    return furi_hal_hid_is_connected();
}

static void usb_kb_press(uint16_t key) {
    furi_hal_hid_kb_press(key);
}

static void usb_kb_release(uint16_t key) {
    furi_hal_hid_kb_release(key);
}

static void usb_kb_release_all(void) {
    furi_hal_hid_kb_release_all();
}

const HidTransport hid_transport_usb = {
    .init = usb_init,
    .deinit = usb_deinit,
    .is_connected = usb_is_connected,
    .kb_press = usb_kb_press,
    .kb_release = usb_kb_release,
    .kb_release_all = usb_kb_release_all,
};
