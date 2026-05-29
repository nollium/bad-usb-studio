#include "../bad_usb_studio.h"

static void bad_usb_studio_scene_editor_callback(ScriptEditorEvent event, void* context) {
    BadUsbStudioApp* app = context;
    switch(event) {
    case ScriptEditorEventEditLine:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventEditorEditLine);
        break;
    case ScriptEditorEventAddLine:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventEditorAddLine);
        break;
    case ScriptEditorEventDeleteLine:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventEditorDeleteLine);
        break;
    case ScriptEditorEventSave:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BadUsbStudioCustomEventEditorSave);
        break;
    }
}

void bad_usb_studio_scene_editor_on_enter(void* context) {
    BadUsbStudioApp* app = context;

    script_editor_view_set_callback(
        app->editor_view, bad_usb_studio_scene_editor_callback, app);
    script_editor_view_set_data(
        app->editor_view, app->script_lines, app->script_line_count, app->script_modified);
    script_editor_view_set_cursor(app->editor_view, app->editor_cursor);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewEditor);
}

bool bad_usb_studio_scene_editor_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        app->editor_cursor = script_editor_view_get_cursor(app->editor_view);

        switch(event.event) {
        case BadUsbStudioCustomEventEditorEditLine:
            if(app->script_line_count > 0 && app->editor_cursor < app->script_line_count) {
                strncpy(
                    app->text_input_buf,
                    furi_string_get_cstr(app->script_lines[app->editor_cursor]),
                    BAD_USB_STUDIO_MAX_LINE_LENGTH - 1);
                app->text_input_buf[BAD_USB_STUDIO_MAX_LINE_LENGTH - 1] = '\0';
                app->text_input_is_insert = false;
                scene_manager_next_scene(
                    app->scene_manager, BadUsbStudioSceneEditorEditLine);
            }
            return true;

        case BadUsbStudioCustomEventEditorAddLine:
            scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneEditorAddCmd);
            return true;

        case BadUsbStudioCustomEventEditorDeleteLine:
            if(app->script_line_count > 0 && app->editor_cursor < app->script_line_count) {
                bad_usb_studio_script_delete_line(app, app->editor_cursor);
                app->script_modified = true;
                if(app->editor_cursor >= app->script_line_count && app->script_line_count > 0) {
                    app->editor_cursor = app->script_line_count - 1;
                }
                script_editor_view_set_data(
                    app->editor_view,
                    app->script_lines,
                    app->script_line_count,
                    app->script_modified);
                script_editor_view_set_cursor(app->editor_view, app->editor_cursor);
            }
            return true;

        case BadUsbStudioCustomEventEditorSave:
            if(furi_string_size(app->file_path) > 0) {
                // Save to existing path
                payload_storage_save(
                    furi_string_get_cstr(app->file_path),
                    app->script_lines,
                    app->script_line_count);
                app->script_modified = false;
                script_editor_view_set_data(
                    app->editor_view,
                    app->script_lines,
                    app->script_line_count,
                    app->script_modified);
            } else {
                // Need filename - go to Save scene
                scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneSave);
            }
            return true;
        }
    }

    return false;
}

void bad_usb_studio_scene_editor_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    app->editor_cursor = script_editor_view_get_cursor(app->editor_view);
}
