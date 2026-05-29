#include "../bad_usb_studio.h"

static void bad_usb_studio_scene_payload_actions_submenu_callback(void* context, uint32_t index) {
    BadUsbStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void bad_usb_studio_scene_payload_actions_dialog_callback(
    DialogExResult result,
    void* context) {
    BadUsbStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void bad_usb_studio_scene_payload_actions_on_enter(void* context) {
    BadUsbStudioApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, furi_string_get_cstr(app->file_name));
    submenu_add_item(
        submenu,
        "Execute",
        BadUsbStudioCustomEventPayloadActionsExecute,
        bad_usb_studio_scene_payload_actions_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Edit",
        BadUsbStudioCustomEventPayloadActionsEdit,
        bad_usb_studio_scene_payload_actions_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Delete",
        BadUsbStudioCustomEventPayloadActionsDelete,
        bad_usb_studio_scene_payload_actions_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewSubmenu);
}

bool bad_usb_studio_scene_payload_actions_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case BadUsbStudioCustomEventPayloadActionsExecute: {
            // Load script
            bad_usb_studio_script_clear(app);
            FuriString** loaded_lines = NULL;
            uint16_t count =
                payload_storage_load(furi_string_get_cstr(app->file_path), &loaded_lines);
            for(uint16_t i = 0; i < count; i++) {
                bad_usb_studio_script_add_line(app, furi_string_get_cstr(loaded_lines[i]));
                furi_string_free(loaded_lines[i]);
            }
            if(loaded_lines) free(loaded_lines);
            scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneExecute);
            return true;
        }
        case BadUsbStudioCustomEventPayloadActionsEdit: {
            // Load script into editor
            bad_usb_studio_script_clear(app);
            FuriString** loaded_lines = NULL;
            uint16_t count =
                payload_storage_load(furi_string_get_cstr(app->file_path), &loaded_lines);
            for(uint16_t i = 0; i < count; i++) {
                bad_usb_studio_script_add_line(app, furi_string_get_cstr(loaded_lines[i]));
                furi_string_free(loaded_lines[i]);
            }
            if(loaded_lines) free(loaded_lines);
            app->script_modified = false;
            app->editor_cursor = 0;
            scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneEditor);
            return true;
        }
        case BadUsbStudioCustomEventPayloadActionsDelete:
            scene_manager_set_scene_state(
                app->scene_manager, BadUsbStudioScenePayloadActions, 1);
            // Show confirmation dialog
            dialog_ex_set_header(app->dialog, "Delete payload?", 64, 0, AlignCenter, AlignTop);
            dialog_ex_set_text(
                app->dialog,
                furi_string_get_cstr(app->file_name),
                64,
                26,
                AlignCenter,
                AlignCenter);
            dialog_ex_set_left_button_text(app->dialog, "Cancel");
            dialog_ex_set_right_button_text(app->dialog, "Delete");
            dialog_ex_set_result_callback(
                app->dialog,
                bad_usb_studio_scene_payload_actions_dialog_callback);
            dialog_ex_set_context(app->dialog, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewDialog);
            return true;
        case DialogExResultLeft:
            scene_manager_previous_scene(app->scene_manager);
            return true;
        case DialogExResultRight:
            payload_storage_delete(furi_string_get_cstr(app->file_path));
            // Go back to payload list
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BadUsbStudioScenePayloadList);
            return true;
        }
    }

    return false;
}

void bad_usb_studio_scene_payload_actions_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    submenu_reset(app->submenu);
    dialog_ex_reset(app->dialog);
}
