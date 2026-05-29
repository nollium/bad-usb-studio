#include "../bad_usb_studio.h"

static void bad_usb_studio_scene_quick_type_result_callback(void* context, uint32_t index) {
    BadUsbStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bad_usb_studio_scene_quick_type_result_on_enter(void* context) {
    BadUsbStudioApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Text sent!");
    submenu_add_item(
        submenu,
        "Type Again",
        BadUsbStudioCustomEventQuickTypeResultType,
        bad_usb_studio_scene_quick_type_result_callback,
        app);
    submenu_add_item(
        submenu,
        "Save as Payload",
        BadUsbStudioCustomEventQuickTypeResultSave,
        bad_usb_studio_scene_quick_type_result_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewSubmenu);
}

bool bad_usb_studio_scene_quick_type_result_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case BadUsbStudioCustomEventQuickTypeResultType:
            // Go back to quick type keyboard (pop result scene)
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BadUsbStudioSceneQuickType);
            return true;

        case BadUsbStudioCustomEventQuickTypeResultSave:
            // Build a script from the typed text: STRING <text> + ENTER
            bad_usb_studio_script_clear(app);
            {
                char line_buf[BAD_USB_STUDIO_MAX_LINE_LENGTH + 8];
                snprintf(
                    line_buf, sizeof(line_buf), "STRING %s", app->text_input_buf);
                bad_usb_studio_script_add_line(app, line_buf);
                bad_usb_studio_script_add_line(app, "ENTER");
            }
            furi_string_reset(app->file_path);
            furi_string_reset(app->file_name);
            app->script_modified = true;
            // Go to Save scene to pick a filename
            scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneSave);
            return true;
        }
    }

    return false;
}

void bad_usb_studio_scene_quick_type_result_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    submenu_reset(app->submenu);
}
