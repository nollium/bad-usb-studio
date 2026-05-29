#include "../bad_usb_studio.h"

static void bad_usb_studio_scene_quick_type_kb_callback(const char* text, void* context) {
    BadUsbStudioApp* app = context;
    strncpy(app->text_input_buf, text, BAD_USB_STUDIO_MAX_LINE_LENGTH - 1);
    app->text_input_buf[BAD_USB_STUDIO_MAX_LINE_LENGTH - 1] = '\0';
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BadUsbStudioCustomEventQuickTypeSubmit);
}

static int32_t bad_usb_studio_quick_type_worker(void* context) {
    BadUsbStudioApp* app = context;

    const HidTransport* transport = (app->transport_type == BadUsbStudioTransportBle)
                                        ? &hid_transport_ble
                                        : &hid_transport_usb;

    bool ok = transport->init();
    if(!ok) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventExecError);
        return -1;
    }

    // Wait for host to recognize HID device
    for(int i = 0; i < 50 && !transport->is_connected(); i++) {
        if(app->abort_requested) break;
        furi_delay_ms(100);
    }
    furi_delay_ms(500);

    if(!app->abort_requested) {
        DuckyState* state = ducky_state_alloc();
        state->transport = transport;
        state->layout = app->keyboard_layout;

        char line_buf[BAD_USB_STUDIO_MAX_LINE_LENGTH + 8];
        snprintf(line_buf, sizeof(line_buf), "STRING %s", app->text_input_buf);
        ducky_execute_line(state, line_buf);

        transport->kb_release_all();
        ducky_state_free(state);
    }

    transport->deinit();

    view_dispatcher_send_custom_event(
        app->view_dispatcher, BadUsbStudioCustomEventExecDone);
    return 0;
}

void bad_usb_studio_scene_quick_type_on_enter(void* context) {
    BadUsbStudioApp* app = context;

    full_keyboard_view_clear(app->full_keyboard);
    full_keyboard_view_set_header(app->full_keyboard, "Quick Type (OK to send)");
    full_keyboard_view_set_callback(
        app->full_keyboard, bad_usb_studio_scene_quick_type_kb_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewFullKeyboard);
}

bool bad_usb_studio_scene_quick_type_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BadUsbStudioCustomEventQuickTypeSubmit) {
            if(strlen(app->text_input_buf) > 0) {
                // Show "Sending..." widget and launch worker
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Sending...");
                widget_add_string_element(
                    app->widget,
                    64,
                    36,
                    AlignCenter,
                    AlignCenter,
                    FontSecondary,
                    "Press Back to abort");
                view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewWidget);

                app->abort_requested = false;
                app->is_executing = true;
                app->exec_thread = furi_thread_alloc_ex(
                    "QuickType", 2048, bad_usb_studio_quick_type_worker, app);
                furi_thread_start(app->exec_thread);
            }
            return true;
        }
        if(event.event == BadUsbStudioCustomEventExecDone) {
            app->is_executing = false;
            if(app->exec_thread) {
                furi_thread_join(app->exec_thread);
                furi_thread_free(app->exec_thread);
                app->exec_thread = NULL;
            }
            // Go to result scene with save/type again options
            scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneQuickTypeResult);
            return true;
        }
        if(event.event == BadUsbStudioCustomEventExecError) {
            app->is_executing = false;
            if(app->exec_thread) {
                furi_thread_join(app->exec_thread);
                furi_thread_free(app->exec_thread);
                app->exec_thread = NULL;
            }
            scene_manager_previous_scene(app->scene_manager);
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

void bad_usb_studio_scene_quick_type_on_exit(void* context) {
    BadUsbStudioApp* app = context;

    if(app->is_executing) {
        app->abort_requested = true;
        if(app->exec_thread) {
            furi_thread_join(app->exec_thread);
            furi_thread_free(app->exec_thread);
            app->exec_thread = NULL;
        }
        app->is_executing = false;
    }

    widget_reset(app->widget);
}
