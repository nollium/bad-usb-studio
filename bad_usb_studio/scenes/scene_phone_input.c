#include "../bad_usb_studio.h"
#include <bt/bt_service/bt.h>
#include <profiles/serial_profile.h>
#include <extra_profiles/hid_profile.h>
#include <services/serial_service.h>
#include <storage/storage.h>

#define FURI_LOG_TAG "PhoneBLE"

static Bt* phone_bt = NULL;
static FuriHalBleProfileBase* phone_profile = NULL;
static volatile bool phone_active = false;
static volatile uint32_t rx_byte_count = 0;
static volatile uint32_t rx_msg_count = 0;
static volatile uint32_t cb_call_count = 0;

static void update_status_widget(BadUsbStudioApp* app, const char* line1, const char* line2, const char* line3) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 3, AlignCenter, AlignTop, FontPrimary, "Phone Input");
    if(line1) widget_add_string_element(
        app->widget, 64, 17, AlignCenter, AlignTop, FontSecondary, line1);
    if(line2) widget_add_string_element(
        app->widget, 64, 29, AlignCenter, AlignTop, FontSecondary, line2);
    if(line3) widget_add_string_element(
        app->widget, 64, 41, AlignCenter, AlignTop, FontSecondary, line3);

    char stats[48];
    snprintf(stats, sizeof(stats), "cb:%lu bytes:%lu msgs:%lu",
             cb_call_count, rx_byte_count, rx_msg_count);
    widget_add_string_element(
        app->widget, 64, 55, AlignCenter, AlignTop, FontSecondary, stats);
}

static void phone_bt_status_cb(BtStatus status, void* context) {
    if(!phone_active) return;
    BadUsbStudioApp* app = context;
    FURI_LOG_I(FURI_LOG_TAG, "BT status: %d", status);
    if(status == BtStatusConnected) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventPhoneConnected);
    } else if(status == BtStatusAdvertising || status == BtStatusOff) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventPhoneDisconnected);
    }
}

static uint16_t phone_serial_cb(SerialServiceEvent event, void* context) {
    cb_call_count++;
    FURI_LOG_I(FURI_LOG_TAG, "serial_cb event=%d size=%d", event.event, event.data.size);

    if(!phone_active) return 0;
    BadUsbStudioApp* app = context;

    if(event.event == SerialServiceEventTypeDataReceived) {
        rx_byte_count += event.data.size;
        for(uint16_t i = 0; i < event.data.size; i++) {
            char c = (char)event.data.buffer[i];
            if(c == '\0') {
                rx_msg_count++;
                FURI_LOG_I(FURI_LOG_TAG, "msg: \"%s\"",
                           furi_string_get_cstr(app->ble_rx_buf));
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
    FURI_LOG_I(FURI_LOG_TAG, "TX: \"%s\"", str);
    if(!phone_profile || !phone_active) {
        FURI_LOG_E(FURI_LOG_TAG, "TX failed: profile=%p active=%d",
                   phone_profile, phone_active);
        return;
    }
    uint16_t len = strlen(str) + 1;
    const uint16_t chunk = BLE_PROFILE_SERIAL_PACKET_SIZE_MAX;
    const uint8_t* data = (const uint8_t*)str;
    while(len > 0) {
        uint16_t send = (len > chunk) ? chunk : len;
        bool ok = ble_profile_serial_tx(phone_profile, (uint8_t*)data, send);
        FURI_LOG_I(FURI_LOG_TAG, "TX chunk %d bytes, ok=%d", send, ok);
        data += send;
        len -= send;
        if(len > 0) furi_delay_ms(10);
    }
}

static void handle_phone_command(BadUsbStudioApp* app) {
    const char* cmd = furi_string_get_cstr(app->ble_rx_buf);
    FURI_LOG_I(FURI_LOG_TAG, "CMD: \"%s\"", cmd);

    char display_buf[32];
    snprintf(display_buf, sizeof(display_buf), "CMD: %.24s", cmd);

    if(strcasecmp(cmd, "PING") == 0) {
        update_status_widget(app, "Got PING!", "Sending PONG...", NULL);
        phone_tx_str("PONG");

    } else if(strcasecmp(cmd, "LIST") == 0) {
        update_status_widget(app, "Got LIST", "Listing payloads...", NULL);
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

            update_status_widget(app,
                ok ? "Saved!" : "Save FAILED",
                filename, NULL);
            furi_string_free(full_path);
        }

    } else if(strncasecmp(cmd, "EXEC:", 5) == 0) {
        update_status_widget(app, "Executing payload...", NULL, NULL);
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
        update_status_widget(app, "Done!", "Waiting for commands...", NULL);
    } else {
        update_status_widget(app, "Unknown command:", display_buf, NULL);
    }

    furi_string_reset(app->ble_rx_buf);
}

static int32_t phone_ble_setup_worker(void* context) {
    BadUsbStudioApp* app = context;

    FURI_LOG_I(FURI_LOG_TAG, "Setup: opening BT");
    phone_bt = furi_record_open(RECORD_BT);
    bt_disconnect(phone_bt);
    furi_delay_ms(500);

    bt_set_status_changed_callback(phone_bt, phone_bt_status_cb, app);
    bt_keys_storage_set_storage_path(
        phone_bt, EXT_PATH("apps_data/bad_usb_studio/.bt_serial.keys"));

    FURI_LOG_I(FURI_LOG_TAG, "Setup: switching to HID");
    BleProfileHidParams hid_params = {.device_name_prefix = "BadUSB", .mac_xor = 0};
    FuriHalBleProfileBase* hid = bt_profile_start(phone_bt, ble_profile_hid, &hid_params);
    FURI_LOG_I(FURI_LOG_TAG, "Setup: HID profile = %p", hid);
    if(!hid) {
        if(phone_active)
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BadUsbStudioCustomEventExecError);
        return -1;
    }

    furi_delay_ms(1000);
    if(!phone_active) return -1;

    FURI_LOG_I(FURI_LOG_TAG, "Setup: switching to serial");
    phone_profile = bt_profile_start(phone_bt, ble_profile_serial, NULL);
    FURI_LOG_I(FURI_LOG_TAG, "Setup: serial profile = %p", phone_profile);
    if(!phone_profile) {
        bt_profile_restore_default(phone_bt);
        if(phone_active)
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BadUsbStudioCustomEventExecError);
        return -1;
    }

    furi_delay_ms(500);
    if(!phone_active) return -1;

    FURI_LOG_I(FURI_LOG_TAG, "Setup: setting callback + disabling RPC");
    ble_profile_serial_set_event_callback(
        phone_profile, 512, phone_serial_cb, app);
    ble_profile_serial_set_rpc_active(phone_profile, false);
    FURI_LOG_I(FURI_LOG_TAG, "Setup: done, advertising");

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
    rx_byte_count = 0;
    rx_msg_count = 0;
    cb_call_count = 0;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input (BLE)");
    widget_add_string_element(
        app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Starting BLE...");
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignTop, FontSecondary, "Please wait ~3s");
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewWidget);

    app->exec_thread = furi_thread_alloc_ex(
        "PhoneBLE", 2048, phone_ble_setup_worker, app);
    furi_thread_start(app->exec_thread);
}

bool bad_usb_studio_scene_phone_input_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case BadUsbStudioCustomEventPhoneConnected:
            FURI_LOG_I(FURI_LOG_TAG, "Phone connected!");
            update_status_widget(app, "Phone connected!", "Waiting for commands...", NULL);
            return true;

        case BadUsbStudioCustomEventPhoneDisconnected:
            FURI_LOG_I(FURI_LOG_TAG, "Advertising / disconnected");
            update_status_widget(app, "Advertising...", "Open PWA and connect", NULL);
            return true;

        case BadUsbStudioCustomEventExecError:
            FURI_LOG_E(FURI_LOG_TAG, "BLE init failed");
            update_status_widget(app, "BLE init FAILED!", "Press Back to return", NULL);
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

    phone_active = false;

    if(app->exec_thread) {
        furi_thread_join(app->exec_thread);
        furi_thread_free(app->exec_thread);
        app->exec_thread = NULL;
    }

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
