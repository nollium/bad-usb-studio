#include "../bad_usb_studio.h"

// BLE serial profile switching crashes/freezes the Flipper on stock firmware.
// For now, show instructions to use the mobile app's file manager instead.
// TODO: Find a safe way to take over BLE serial without profile switching.

void bad_usb_studio_scene_phone_input_on_enter(void* context) {
    BadUsbStudioApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 3, AlignCenter, AlignTop, FontPrimary, "Phone Input");
    widget_add_string_element(
        app->widget, 64, 17, AlignCenter, AlignTop, FontSecondary,
        "Use Flipper mobile app:");
    widget_add_string_element(
        app->widget, 64, 29, AlignCenter, AlignTop, FontSecondary,
        "File Manager > SD Card >");
    widget_add_string_element(
        app->widget, 64, 41, AlignCenter, AlignTop, FontSecondary,
        "apps_data/bad_usb_studio/");
    widget_add_string_element(
        app->widget, 64, 53, AlignCenter, AlignTop, FontSecondary,
        "Upload .txt DuckyScript files");

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewWidget);
}

bool bad_usb_studio_scene_phone_input_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bad_usb_studio_scene_phone_input_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    widget_reset(app->widget);
}
