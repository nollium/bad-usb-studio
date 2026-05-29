#include "hid_transport.h"
#include <furi.h>
#include <furi_hal.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include <storage/storage.h>

static Bt* bt = NULL;
static FuriHalBleProfileBase* ble_profile = NULL;

static bool ble_init(void) {
    bt = furi_record_open(RECORD_BT);
    bt_disconnect(bt);
    furi_delay_ms(200);
    bt_keys_storage_set_storage_path(
        bt, EXT_PATH("apps_data/bad_usb_studio/.bt_hid.keys"));

    BleProfileHidParams hid_params = {
        .device_name_prefix = "BadUSB",
        .mac_xor = 0x0001,
    };
    ble_profile = bt_profile_start(bt, ble_profile_hid, &hid_params);
    return ble_profile != NULL;
}

static void ble_deinit(void) {
    if(bt) {
        bt_profile_restore_default(bt);
        bt_keys_storage_set_default_path(bt);
        furi_record_close(RECORD_BT);
        bt = NULL;
        ble_profile = NULL;
    }
}

static bool ble_is_connected(void) {
    return ble_profile != NULL && furi_hal_bt_is_active();
}

static void ble_kb_press(uint16_t key) {
    if(ble_profile) {
        ble_profile_hid_kb_press(ble_profile, key);
    }
}

static void ble_kb_release(uint16_t key) {
    if(ble_profile) {
        ble_profile_hid_kb_release(ble_profile, key);
    }
}

static void ble_kb_release_all(void) {
    if(ble_profile) {
        ble_profile_hid_kb_release_all(ble_profile);
    }
}

const HidTransport hid_transport_ble = {
    .init = ble_init,
    .deinit = ble_deinit,
    .is_connected = ble_is_connected,
    .kb_press = ble_kb_press,
    .kb_release = ble_kb_release,
    .kb_release_all = ble_kb_release_all,
};
