#include "../bad_usb_studio.h"
#include <bt/bt_service/bt.h>
#include <profiles/serial_profile.h>
#include <services/serial_service.h>
#include <storage/storage.h>

// Protocol: PWA sends newline-terminated commands over BLE serial
// "SAVE:<filename>:<line1>\n<line2>\n..."  — save a payload file
// "EXEC:<line1>\n<line2>\n..."            — execute immediately
// "PING"                                  — connection check, we reply "PONG"
// "LIST"                                  — list saved payloads, we reply with names

static Bt* phone_bt = NULL;

static void bad_usb_studio_phone_bt_status_callback(BtStatus status, void* context) {
    BadUsbStudioApp* app = context;
    if(status == BtStatusConnected) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventPhoneConnected);
    } else if(status == BtStatusAdvertising) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventPhoneDisconnected);
    }
}

static uint16_t bad_usb_studio_phone_serial_callback(SerialServiceEvent event, void* context) {
    BadUsbStudioApp* app = context;

    if(event.event == SerialServiceEventTypeDataReceived) {
        for(uint16_t i = 0; i < event.data.size; i++) {
            char c = (char)event.data.buffer[i];
            if(c == '\0') {
                // End of message
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, BadUsbStudioCustomEventPhoneData);
            } else {
                furi_string_push_back(app->ble_rx_buf, c);
            }
        }
    }

    return 0;
}

static FuriHalBleProfileBase* phone_serial_profile = NULL;

static void phone_tx(const char* data) {
    if(phone_serial_profile) {
        ble_profile_serial_tx(
            phone_serial_profile, (uint8_t*)data, strlen(data));
    }
}

static void handle_phone_command(BadUsbStudioApp* app) {
    const char* cmd = furi_string_get_cstr(app->ble_rx_buf);

    if(strcasecmp(cmd, "PING") == 0) {
        phone_tx("PONG\0");
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
        furi_string_push_back(response, '\0');
        phone_tx(furi_string_get_cstr(response));
        furi_string_free(response);
    } else if(strncasecmp(cmd, "SAVE:", 5) == 0) {
        // Format: SAVE:filename.txt:LINE1\nLINE2\nLINE3
        const char* rest = cmd + 5;
        const char* colon = strchr(rest, ':');
        if(colon) {
            FuriString* filename = furi_string_alloc_set_str(rest);
            furi_string_left(filename, colon - rest);

            // Append .txt if missing
            if(!furi_string_end_with_str(filename, ".txt")) {
                furi_string_cat_str(filename, ".txt");
            }

            FuriString* full_path = furi_string_alloc();
            payload_storage_make_path(full_path, furi_string_get_cstr(filename));

            // Parse lines
            const char* content = colon + 1;
            bad_usb_studio_script_clear(app);
            FuriString* line = furi_string_alloc();
            for(; *content; content++) {
                if(*content == '\n') {
                    bad_usb_studio_script_add_line(
                        app, furi_string_get_cstr(line));
                    furi_string_reset(line);
                } else {
                    furi_string_push_back(line, *content);
                }
            }
            if(furi_string_size(line) > 0) {
                bad_usb_studio_script_add_line(
                    app, furi_string_get_cstr(line));
            }
            furi_string_free(line);

            bool ok = payload_storage_save(
                furi_string_get_cstr(full_path),
                app->script_lines,
                app->script_line_count);

            phone_tx(ok ? "OK\0" : "ERR\0");

            // Update widget
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                "Phone Input (BLE)");
            widget_add_string_element(
                app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary,
                "Connected");
            char saved_msg[64];
            snprintf(saved_msg, sizeof(saved_msg), "Saved: %s",
                     furi_string_get_cstr(filename));
            widget_add_string_element(
                app->widget, 64, 35, AlignCenter, AlignTop, FontSecondary,
                saved_msg);

            furi_string_free(filename);
            furi_string_free(full_path);
        }
    } else if(strncasecmp(cmd, "EXEC:", 5) == 0) {
        const char* content = cmd + 5;
        bad_usb_studio_script_clear(app);
        FuriString* line = furi_string_alloc();
        for(; *content; content++) {
            if(*content == '\n') {
                bad_usb_studio_script_add_line(
                    app, furi_string_get_cstr(line));
                furi_string_reset(line);
            } else {
                furi_string_push_back(line, *content);
            }
        }
        if(furi_string_size(line) > 0) {
            bad_usb_studio_script_add_line(
                app, furi_string_get_cstr(line));
        }
        furi_string_free(line);

        phone_tx("RUNNING\0");

        // Execute via USB HID (BLE is busy with phone)
        const HidTransport* transport = &hid_transport_usb;
        DuckyState* state = ducky_state_alloc();
        state->transport = transport;
        state->layout = app->keyboard_layout;

        transport->init();
        for(int i = 0; i < 30 && !transport->is_connected(); i++) {
            furi_delay_ms(100);
        }
        furi_delay_ms(500);

        ducky_execute_script(state, app->script_lines, app->script_line_count, NULL, NULL);

        transport->kb_release_all();
        transport->deinit();
        ducky_state_free(state);

        phone_tx("DONE\0");
    }

    furi_string_reset(app->ble_rx_buf);
}

void bad_usb_studio_scene_phone_input_on_enter(void* context) {
    BadUsbStudioApp* app = context;

    furi_string_reset(app->ble_rx_buf);

    // Setup widget
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Phone Input (BLE)");
    widget_add_string_element(
        app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary, "Waiting for connection...");
    widget_add_string_element(
        app->widget, 64, 38, AlignCenter, AlignTop, FontSecondary, "Open PWA on your phone");
    widget_add_string_element(
        app->widget, 64, 50, AlignCenter, AlignTop, FontSecondary, "and connect to Flipper");
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewWidget);

    // Setup BLE serial
    phone_bt = furi_record_open(RECORD_BT);
    bt_disconnect(phone_bt);
    furi_delay_ms(200);
    bt_set_status_changed_callback(phone_bt, bad_usb_studio_phone_bt_status_callback, app);

    // The default profile is already serial. Just hook our callback.
    // Get the current profile and set our serial callback on it
    phone_serial_profile = NULL;

    // Disable RPC so we get raw serial data
    // We need to start our own serial profile
    bt_keys_storage_set_storage_path(
        phone_bt, EXT_PATH("apps_data/bad_usb_studio/.bt_serial.keys"));
    phone_serial_profile = bt_profile_start(phone_bt, ble_profile_serial, NULL);

    if(phone_serial_profile) {
        ble_profile_serial_set_event_callback(
            phone_serial_profile, 512, bad_usb_studio_phone_serial_callback, app);
    }
}

bool bad_usb_studio_scene_phone_input_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case BadUsbStudioCustomEventPhoneConnected:
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                "Phone Input (BLE)");
            widget_add_string_element(
                app->widget, 64, 25, AlignCenter, AlignCenter, FontSecondary,
                "Connected!");
            widget_add_string_element(
                app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary,
                "Waiting for commands...");
            return true;

        case BadUsbStudioCustomEventPhoneDisconnected:
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                "Phone Input (BLE)");
            widget_add_string_element(
                app->widget, 64, 25, AlignCenter, AlignCenter, FontSecondary,
                "Disconnected");
            widget_add_string_element(
                app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary,
                "Waiting for reconnection...");
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

    if(phone_bt) {
        bt_set_status_changed_callback(phone_bt, NULL, NULL);
        bt_profile_restore_default(phone_bt);
        bt_keys_storage_set_default_path(phone_bt);
        furi_record_close(RECORD_BT);
        phone_bt = NULL;
        phone_serial_profile = NULL;
    }

    furi_string_reset(app->ble_rx_buf);
    widget_reset(app->widget);
}
