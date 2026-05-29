#include "../bad_usb_studio.h"
#include <bt/bt_service/bt.h>
#include <profiles/serial_profile.h>
#include <extra_profiles/hid_profile.h>
#include <services/serial_service.h>
#include <storage/storage.h>

static Bt* phone_bt = NULL;
static FuriHalBleProfileBase* phone_profile = NULL;
static volatile bool phone_active = false;

static void phone_bt_status_cb(BtStatus status, void* context) {
    if(!phone_active) return;
    BadUsbStudioApp* app = context;
    if(status == BtStatusConnected) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventPhoneConnected);
    } else if(status == BtStatusAdvertising || status == BtStatusOff) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventPhoneDisconnected);
    }
}

static uint16_t phone_serial_cb(SerialServiceEvent event, void* context) {
    if(!phone_active) return 0;
    BadUsbStudioApp* app = context;

    if(event.event == SerialServiceEventTypeDataReceived) {
        for(uint16_t i = 0; i < event.data.size; i++) {
            char c = (char)event.data.buffer[i];
            if(c == '\0') {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, BadUsbStudioCustomEventPhoneData);
            } else {
                furi_string_push_back(app->ble_rx_buf, c);
            }
        }
    }
    return 0;
}

static void phone_tx_str(const char* str) {
    if(!phone_profile || !phone_active) return;
    uint16_t len = strlen(str) + 1;
    const uint16_t chunk = BLE_PROFILE_SERIAL_PACKET_SIZE_MAX;
    const uint8_t* data = (const uint8_t*)str;
    while(len > 0) {
        uint16_t send = (len > chunk) ? chunk : len;
        ble_profile_serial_tx(phone_profile, (uint8_t*)data, send);
        data += send;
        len -= send;
        if(len > 0) furi_delay_ms(10);
    }
}

static void handle_phone_command(BadUsbStudioApp* app) {
    const char* cmd = furi_string_get_cstr(app->ble_rx_buf);

    if(strcasecmp(cmd, "PING") == 0) {
        phone_tx_str("PONG");

    } else if(strcasecmp(cmd, "LIST") == 0) {
        FuriString** names = NULL;
        uint16_t count = payload_storage_list(&names);
        FuriString* response = furi_string_alloc();
        for(uint16_t i = 0; i < count; i++) {
            if(i > 0) furi_string_push_back(response, '\n');
            furi_string_cat(response, names[i]);
            furi_string_free(names[i]);
        }
        if(names) free(names);
        phone_tx_str(furi_string_get_cstr(response));
        furi_string_free(response);

    } else if(strncasecmp(cmd, "SAVE:", 5) == 0) {
        const char* rest = cmd + 5;
        const char* colon = strchr(rest, ':');
        if(colon) {
            char filename[64];
            size_t name_len = colon - rest;
            if(name_len >= sizeof(filename)) name_len = sizeof(filename) - 1;
            memcpy(filename, rest, name_len);
            filename[name_len] = '\0';
            size_t flen = strlen(filename);
            if(flen < 4 || strcmp(filename + flen - 4, ".txt") != 0) {
                if(flen + 4 < sizeof(filename)) memcpy(filename + flen, ".txt", 5);
            }

            FuriString* full_path = furi_string_alloc();
            payload_storage_make_path(full_path, filename);

            const char* content = colon + 1;
            bad_usb_studio_script_clear(app);
            FuriString* line = furi_string_alloc();
            for(; *content; content++) {
                if(*content == '\n') {
                    bad_usb_studio_script_add_line(app, furi_string_get_cstr(line));
                    furi_string_reset(line);
                } else {
                    furi_string_push_back(line, *content);
                }
            }
            if(furi_string_size(line) > 0)
                bad_usb_studio_script_add_line(app, furi_string_get_cstr(line));
            furi_string_free(line);

            bool ok = payload_storage_save(
                furi_string_get_cstr(full_path), app->script_lines, app->script_line_count);
            phone_tx_str(ok ? "OK" : "ERR");

            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input");
            widget_add_string_element(
                app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary,
                ok ? "Saved!" : "Save failed");
            widget_add_string_element(
                app->widget, 64, 40, AlignCenter, AlignTop, FontSecondary, filename);
            furi_string_free(full_path);
        }

    } else if(strncasecmp(cmd, "EXEC:", 5) == 0) {
        const char* content = cmd + 5;
        bad_usb_studio_script_clear(app);
        FuriString* line = furi_string_alloc();
        for(; *content; content++) {
            if(*content == '\n') {
                bad_usb_studio_script_add_line(app, furi_string_get_cstr(line));
                furi_string_reset(line);
            } else {
                furi_string_push_back(line, *content);
            }
        }
        if(furi_string_size(line) > 0)
            bad_usb_studio_script_add_line(app, furi_string_get_cstr(line));
        furi_string_free(line);

        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input");
        widget_add_string_element(
            app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Executing...");

        phone_tx_str("RUNNING");

        const HidTransport* transport = &hid_transport_usb;
        DuckyState* state = ducky_state_alloc();
        state->transport = transport;
        state->layout = app->keyboard_layout;
        transport->init();
        for(int i = 0; i < 30 && !transport->is_connected(); i++)
            furi_delay_ms(100);
        furi_delay_ms(500);
        ducky_execute_script(state, app->script_lines, app->script_line_count, NULL, NULL);
        transport->kb_release_all();
        transport->deinit();
        ducky_state_free(state);

        phone_tx_str("DONE");

        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input");
        widget_add_string_element(
            app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Done! Connected.");
    }

    furi_string_reset(app->ble_rx_buf);
}

// BLE setup runs in a worker thread to avoid UI deadlock
static int32_t phone_ble_setup_worker(void* context) {
    BadUsbStudioApp* app = context;

    phone_bt = furi_record_open(RECORD_BT);
    bt_disconnect(phone_bt);
    furi_delay_ms(500);

    bt_set_status_changed_callback(phone_bt, phone_bt_status_cb, app);
    bt_keys_storage_set_storage_path(
        phone_bt, EXT_PATH("apps_data/bad_usb_studio/.bt_serial.keys"));

    // Step 1: Switch to HID to cleanly leave the default serial/RPC profile
    BleProfileHidParams hid_params = {.device_name_prefix = "BadUSB", .mac_xor = 0};
    FuriHalBleProfileBase* hid = bt_profile_start(phone_bt, ble_profile_hid, &hid_params);
    if(!hid) {
        if(phone_active) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BadUsbStudioCustomEventExecError);
        }
        return -1;
    }

    // Give the BLE stack time to fully settle on HID
    furi_delay_ms(1000);

    if(!phone_active) return -1;

    // Step 2: Switch from HID to serial — this gives us a clean serial profile handle
    phone_profile = bt_profile_start(phone_bt, ble_profile_serial, NULL);
    if(!phone_profile) {
        // Fallback: restore default
        bt_profile_restore_default(phone_bt);
        if(phone_active) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BadUsbStudioCustomEventExecError);
        }
        return -1;
    }

    furi_delay_ms(500);

    if(!phone_active) return -1;

    // Step 3: Hook our serial callback
    ble_profile_serial_set_event_callback(
        phone_profile, 512, phone_serial_cb, app);

    // Signal success
    if(phone_active) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventPhoneDisconnected);
    }

    return 0;
}

void bad_usb_studio_scene_phone_input_on_enter(void* context) {
    BadUsbStudioApp* app = context;

    furi_string_reset(app->ble_rx_buf);
    phone_active = true;
    phone_profile = NULL;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input (BLE)");
    widget_add_string_element(
        app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Starting BLE...");
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignTop, FontSecondary, "Please wait ~3s");
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewWidget);

    // Run BLE setup in background thread
    app->exec_thread = furi_thread_alloc_ex(
        "PhoneBLE", 2048, phone_ble_setup_worker, app);
    furi_thread_start(app->exec_thread);
}

bool bad_usb_studio_scene_phone_input_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case BadUsbStudioCustomEventPhoneConnected:
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input");
            widget_add_string_element(
                app->widget, 64, 25, AlignCenter, AlignCenter, FontSecondary,
                "Phone connected!");
            widget_add_string_element(
                app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary,
                "Waiting for commands...");
            return true;

        case BadUsbStudioCustomEventPhoneDisconnected:
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input");
            widget_add_string_element(
                app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary, "Advertising...");
            widget_add_string_element(
                app->widget, 64, 34, AlignCenter, AlignTop, FontSecondary,
                "Open PWA on phone");
            widget_add_string_element(
                app->widget, 64, 46, AlignCenter, AlignTop, FontSecondary,
                "and tap Connect");
            return true;

        case BadUsbStudioCustomEventExecError:
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input");
            widget_add_string_element(
                app->widget, 64, 25, AlignCenter, AlignCenter, FontSecondary,
                "BLE init failed!");
            widget_add_string_element(
                app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary,
                "Press Back to return");
            return true;

        case BadUsbStudioCustomEventPhoneData:
            handle_phone_command(app);
            return true;
        }
    }

    return false;
}

void bad_usb_studio_scene_phone_input_on_exit(void* context) {
    BadUsbStudioApp* app = context;

    // Signal worker to stop
    phone_active = false;

    // Wait for worker thread to finish
    if(app->exec_thread) {
        furi_thread_join(app->exec_thread);
        furi_thread_free(app->exec_thread);
        app->exec_thread = NULL;
    }

    // Restore BLE
    if(phone_bt) {
        bt_set_status_changed_callback(phone_bt, NULL, NULL);
        if(phone_profile) {
            ble_profile_serial_set_event_callback(phone_profile, 0, NULL, NULL);
            phone_profile = NULL;
        }
        bt_profile_restore_default(phone_bt);
        bt_keys_storage_set_default_path(phone_bt);
        furi_record_close(RECORD_BT);
        phone_bt = NULL;
    }

    furi_string_reset(app->ble_rx_buf);
    widget_reset(app->widget);
}
