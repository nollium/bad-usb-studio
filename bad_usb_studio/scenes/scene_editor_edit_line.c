#include "../bad_usb_studio.h"

static void bad_usb_studio_scene_editor_edit_line_text_input_callback(void* context) {
    BadUsbStudioApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BadUsbStudioCustomEventTextInputDone);
}

void bad_usb_studio_scene_editor_edit_line_on_enter(void* context) {
    BadUsbStudioApp* app = context;

    text_input_reset(app->text_input);
    text_input_set_header_text(
        app->text_input, app->text_input_is_insert ? "New line" : "Edit line");
    text_input_set_result_callback(
        app->text_input,
        bad_usb_studio_scene_editor_edit_line_text_input_callback,
        app,
        app->text_input_buf,
        BAD_USB_STUDIO_MAX_LINE_LENGTH,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewTextInput);
}

bool bad_usb_studio_scene_editor_edit_line_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == BadUsbStudioCustomEventTextInputDone) {
        if(strlen(app->text_input_buf) > 0) {
            if(app->text_input_is_insert) {
                uint16_t insert_pos = app->editor_cursor;
                if(app->script_line_count > 0) insert_pos++;
                bad_usb_studio_script_insert_line(app, insert_pos, app->text_input_buf);
                app->editor_cursor = insert_pos;
            } else {
                bad_usb_studio_script_set_line(
                    app, app->editor_cursor, app->text_input_buf);
            }
            app->script_modified = true;
        }
        app->text_input_is_insert = false;

        // Go back past AddCmd scene if we came from there
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void bad_usb_studio_scene_editor_edit_line_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    text_input_reset(app->text_input);
}
