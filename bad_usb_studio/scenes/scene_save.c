#include "../bad_usb_studio.h"

static void bad_usb_studio_scene_save_text_input_callback(void* context) {
    BadUsbStudioApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BadUsbStudioCustomEventTextInputDone);
}

void bad_usb_studio_scene_save_on_enter(void* context) {
    BadUsbStudioApp* app = context;

    // Pre-fill with existing name or empty
    if(furi_string_size(app->file_name) > 0) {
        const char* name = furi_string_get_cstr(app->file_name);
        // Strip .txt extension for editing
        size_t len = strlen(name);
        if(len > 4 && strcmp(name + len - 4, ".txt") == 0) {
            strncpy(app->text_input_buf, name, len - 4);
            app->text_input_buf[len - 4] = '\0';
        } else {
            strncpy(app->text_input_buf, name, BAD_USB_STUDIO_MAX_LINE_LENGTH - 1);
            app->text_input_buf[BAD_USB_STUDIO_MAX_LINE_LENGTH - 1] = '\0';
        }
    } else {
        strncpy(app->text_input_buf, "payload", BAD_USB_STUDIO_MAX_LINE_LENGTH - 1);
    }

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Filename (without .txt)");
    text_input_set_result_callback(
        app->text_input,
        bad_usb_studio_scene_save_text_input_callback,
        app,
        app->text_input_buf,
        BAD_USB_STUDIO_MAX_FILENAME,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewTextInput);
}

bool bad_usb_studio_scene_save_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == BadUsbStudioCustomEventTextInputDone) {
        if(strlen(app->text_input_buf) > 0) {
            // Build filename with .txt
            FuriString* filename = furi_string_alloc_printf("%s.txt", app->text_input_buf);
            furi_string_set(app->file_name, filename);

            // Build full path
            payload_storage_make_path(app->file_path, furi_string_get_cstr(filename));

            // Save
            payload_storage_save(
                furi_string_get_cstr(app->file_path),
                app->script_lines,
                app->script_line_count);

            app->script_modified = false;

            furi_string_free(filename);

            // Show confirmation
            popup_set_header(app->popup, "Saved!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                furi_string_get_cstr(app->file_name),
                64,
                35,
                AlignCenter,
                AlignCenter);
            popup_set_timeout(app->popup, 1500);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, NULL);
            popup_set_context(app->popup, NULL);
            view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewPopup);
        }
        return true;
    }

    return false;
}

void bad_usb_studio_scene_save_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    text_input_reset(app->text_input);
    popup_reset(app->popup);
}
