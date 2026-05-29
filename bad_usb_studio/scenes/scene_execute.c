#include "../bad_usb_studio.h"

static int32_t bad_usb_studio_exec_worker(void* context) {
    BadUsbStudioApp* app = context;

    const HidTransport* transport = (app->transport_type == BadUsbStudioTransportBle)
                                        ? &hid_transport_ble
                                        : &hid_transport_usb;

    DuckyState* state = ducky_state_alloc();
    state->transport = transport;
    state->layout = app->keyboard_layout;
    state->abort_flag = &app->abort_requested;

    bool transport_ok = transport->init();
    if(!transport_ok) {
        furi_string_set_str(app->exec_error, "Failed to init transport");
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventExecError);
        ducky_state_free(state);
        return -1;
    }

    // Wait for host to recognize the HID device
    for(int i = 0; i < 50 && !transport->is_connected(); i++) {
        if(app->abort_requested) break;
        furi_delay_ms(100);
    }
    // Extra settle time after connection
    furi_delay_ms(500);

    DuckyStatus result = ducky_execute_script(
        state,
        app->script_lines,
        app->script_line_count,
        NULL,
        NULL);

    transport->kb_release_all();
    transport->deinit();
    ducky_state_free(state);

    if(result == DuckyStatusAborted) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventExecError);
    } else {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventExecDone);
    }

    return 0;
}

static void bad_usb_studio_scene_execute_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    BadUsbStudioApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeLeft) {
        app->abort_requested = true;
    }
}

void bad_usb_studio_scene_execute_on_enter(void* context) {
    BadUsbStudioApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    const char* transport_name =
        (app->transport_type == BadUsbStudioTransportBle) ? "BLE" : "USB";

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Executing...");

    char info_buf[64];
    snprintf(
        info_buf,
        sizeof(info_buf),
        "%s | %d lines | %s",
        furi_string_size(app->file_name) > 0 ? furi_string_get_cstr(app->file_name) : "unsaved",
        app->script_line_count,
        transport_name);
    widget_add_string_element(
        widget, 64, 20, AlignCenter, AlignTop, FontSecondary, info_buf);

    if(app->transport_type == BadUsbStudioTransportBle) {
        widget_add_string_element(
            widget, 64, 33, AlignCenter, AlignTop, FontSecondary, "Waiting for BLE connection...");
    }

    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Abort",
        bad_usb_studio_scene_execute_widget_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewWidget);

    // Start execution thread
    app->abort_requested = false;
    app->is_executing = true;
    app->exec_thread = furi_thread_alloc_ex("BadUsbExec", 2048, bad_usb_studio_exec_worker, app);
    furi_thread_start(app->exec_thread);
}

bool bad_usb_studio_scene_execute_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BadUsbStudioCustomEventExecDone) {
            app->is_executing = false;

            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Done!");
            widget_add_string_element(
                app->widget,
                64,
                36,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                "Press Back to return");
            return true;
        }
        if(event.event == BadUsbStudioCustomEventExecError) {
            app->is_executing = false;

            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 15, AlignCenter, AlignCenter, FontPrimary, "Stopped");

            const char* err = furi_string_size(app->exec_error) > 0
                                  ? furi_string_get_cstr(app->exec_error)
                                  : "Aborted by user";
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, err);
            widget_add_string_element(
                app->widget,
                64,
                46,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                "Press Back to return");
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        if(app->is_executing) {
            app->abort_requested = true;
            return true;
        }
    }

    return false;
}

void bad_usb_studio_scene_execute_on_exit(void* context) {
    BadUsbStudioApp* app = context;

    if(app->is_executing) {
        app->abort_requested = true;
    }

    if(app->exec_thread) {
        furi_thread_join(app->exec_thread);
        furi_thread_free(app->exec_thread);
        app->exec_thread = NULL;
    }

    app->is_executing = false;
    widget_reset(app->widget);
    furi_string_reset(app->exec_error);
}
