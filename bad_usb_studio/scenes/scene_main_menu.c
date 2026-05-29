#include "../bad_usb_studio.h"

static void bad_usb_studio_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    BadUsbStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bad_usb_studio_scene_main_menu_on_enter(void* context) {
    BadUsbStudioApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Bad USB Studio");
    submenu_add_item(
        submenu,
        "New Payload",
        BadUsbStudioCustomEventMainMenuNewPayload,
        bad_usb_studio_scene_main_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Saved Payloads",
        BadUsbStudioCustomEventMainMenuSavedPayloads,
        bad_usb_studio_scene_main_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Quick Type",
        BadUsbStudioCustomEventMainMenuQuickType,
        bad_usb_studio_scene_main_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Phone Input (BLE)",
        BadUsbStudioCustomEventMainMenuPhoneInput,
        bad_usb_studio_scene_main_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Settings",
        BadUsbStudioCustomEventMainMenuSettings,
        bad_usb_studio_scene_main_menu_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, BadUsbStudioSceneMainMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewSubmenu);
}

bool bad_usb_studio_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, BadUsbStudioSceneMainMenu, event.event);

        switch(event.event) {
        case BadUsbStudioCustomEventMainMenuNewPayload:
            bad_usb_studio_script_clear(app);
            furi_string_reset(app->file_path);
            furi_string_reset(app->file_name);
            app->script_modified = false;
            app->editor_cursor = 0;
            scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneEditor);
            return true;
        case BadUsbStudioCustomEventMainMenuSavedPayloads:
            scene_manager_next_scene(app->scene_manager, BadUsbStudioScenePayloadList);
            return true;
        case BadUsbStudioCustomEventMainMenuQuickType:
            scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneQuickType);
            return true;
        case BadUsbStudioCustomEventMainMenuPhoneInput:
            scene_manager_next_scene(app->scene_manager, BadUsbStudioScenePhoneInput);
            return true;
        case BadUsbStudioCustomEventMainMenuSettings:
            scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneSettings);
            return true;
        }
    }

    return false;
}

void bad_usb_studio_scene_main_menu_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    submenu_reset(app->submenu);
}
