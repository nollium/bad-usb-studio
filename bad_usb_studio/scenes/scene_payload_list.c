#include "../bad_usb_studio.h"

static void bad_usb_studio_scene_payload_list_callback(void* context, uint32_t index) {
    BadUsbStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bad_usb_studio_scene_payload_list_on_enter(void* context) {
    BadUsbStudioApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Saved Payloads");

    // Free previous list
    if(app->payload_names) {
        for(uint16_t i = 0; i < app->payload_count; i++) {
            furi_string_free(app->payload_names[i]);
        }
        free(app->payload_names);
        app->payload_names = NULL;
        app->payload_count = 0;
    }

    app->payload_count = payload_storage_list(&app->payload_names);

    if(app->payload_count == 0) {
        submenu_add_item(submenu, "(no payloads saved)", 0, NULL, NULL);
    } else {
        for(uint16_t i = 0; i < app->payload_count; i++) {
            submenu_add_item(
                submenu,
                furi_string_get_cstr(app->payload_names[i]),
                i,
                bad_usb_studio_scene_payload_list_callback,
                app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewSubmenu);
}

bool bad_usb_studio_scene_payload_list_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->payload_count) {
            // Build full path
            payload_storage_make_path(
                app->file_path, furi_string_get_cstr(app->payload_names[event.event]));
            furi_string_set(app->file_name, app->payload_names[event.event]);
            scene_manager_next_scene(app->scene_manager, BadUsbStudioScenePayloadActions);
            return true;
        }
    }

    return false;
}

void bad_usb_studio_scene_payload_list_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    submenu_reset(app->submenu);
}
