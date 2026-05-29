#include "../bad_usb_studio.h"

static const char* transport_names[] = {"USB", "BLE"};
static const char* layout_names_short[] = {"US", "FR", "DE", "ES"};

static void bad_usb_studio_scene_settings_transport_changed(VariableItem* item) {
    BadUsbStudioApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->transport_type = index;
    variable_item_set_current_value_text(item, transport_names[index]);
}

static void bad_usb_studio_scene_settings_layout_changed(VariableItem* item) {
    BadUsbStudioApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->keyboard_layout = index;
    variable_item_set_current_value_text(item, layout_names_short[index]);
}

void bad_usb_studio_scene_settings_on_enter(void* context) {
    BadUsbStudioApp* app = context;
    VariableItemList* list = app->variable_item_list;

    variable_item_list_reset(list);

    VariableItem* transport_item = variable_item_list_add(
        list, "Transport", BadUsbStudioTransportCount,
        bad_usb_studio_scene_settings_transport_changed, app);
    variable_item_set_current_value_index(transport_item, app->transport_type);
    variable_item_set_current_value_text(transport_item, transport_names[app->transport_type]);

    VariableItem* layout_item = variable_item_list_add(
        list, "KB Layout", KeyboardLayoutCount,
        bad_usb_studio_scene_settings_layout_changed, app);
    variable_item_set_current_value_index(layout_item, app->keyboard_layout);
    variable_item_set_current_value_text(
        layout_item, layout_names_short[app->keyboard_layout]);

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewVariableItemList);
}

bool bad_usb_studio_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bad_usb_studio_scene_settings_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
