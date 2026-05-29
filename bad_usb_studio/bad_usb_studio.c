#include "bad_usb_studio.h"

// Scene handler arrays generated from scene_config.h
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const on_enter_handlers[])(void*) = {
#include "scenes/scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const on_event_handlers[])(void*, SceneManagerEvent) = {
#include "scenes/scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const on_exit_handlers[])(void*) = {
#include "scenes/scene_config.h"
};
#undef ADD_SCENE

static const SceneManagerHandlers scene_handlers = {
    .on_enter_handlers = on_enter_handlers,
    .on_event_handlers = on_event_handlers,
    .on_exit_handlers = on_exit_handlers,
    .scene_num = BadUsbStudioSceneCount,
};

static bool bad_usb_studio_custom_event_callback(void* context, uint32_t event) {
    BadUsbStudioApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bad_usb_studio_back_event_callback(void* context) {
    BadUsbStudioApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void bad_usb_studio_script_clear(BadUsbStudioApp* app) {
    if(app->script_lines) {
        for(uint16_t i = 0; i < app->script_line_count; i++) {
            furi_string_free(app->script_lines[i]);
        }
        free(app->script_lines);
    }
    app->script_lines = NULL;
    app->script_line_count = 0;
    app->script_line_capacity = 0;
}

static void ensure_capacity(BadUsbStudioApp* app, uint16_t needed) {
    if(needed <= app->script_line_capacity) return;
    uint16_t new_cap = app->script_line_capacity ? app->script_line_capacity * 2 : 16;
    while(new_cap < needed) new_cap *= 2;
    if(new_cap > BAD_USB_STUDIO_MAX_LINES) new_cap = BAD_USB_STUDIO_MAX_LINES;
    app->script_lines = realloc(app->script_lines, sizeof(FuriString*) * new_cap);
    app->script_line_capacity = new_cap;
}

void bad_usb_studio_script_add_line(BadUsbStudioApp* app, const char* line) {
    if(app->script_line_count >= BAD_USB_STUDIO_MAX_LINES) return;
    ensure_capacity(app, app->script_line_count + 1);
    app->script_lines[app->script_line_count] = furi_string_alloc_set(line);
    app->script_line_count++;
}

void bad_usb_studio_script_insert_line(BadUsbStudioApp* app, uint16_t index, const char* line) {
    if(app->script_line_count >= BAD_USB_STUDIO_MAX_LINES) return;
    if(index > app->script_line_count) index = app->script_line_count;
    ensure_capacity(app, app->script_line_count + 1);

    for(uint16_t i = app->script_line_count; i > index; i--) {
        app->script_lines[i] = app->script_lines[i - 1];
    }
    app->script_lines[index] = furi_string_alloc_set(line);
    app->script_line_count++;
}

void bad_usb_studio_script_delete_line(BadUsbStudioApp* app, uint16_t index) {
    if(index >= app->script_line_count) return;
    furi_string_free(app->script_lines[index]);
    for(uint16_t i = index; i < app->script_line_count - 1; i++) {
        app->script_lines[i] = app->script_lines[i + 1];
    }
    app->script_line_count--;
}

void bad_usb_studio_script_set_line(BadUsbStudioApp* app, uint16_t index, const char* line) {
    if(index >= app->script_line_count) return;
    furi_string_set_str(app->script_lines[index], line);
}

BadUsbStudioApp* bad_usb_studio_app_alloc(void) {
    BadUsbStudioApp* app = malloc(sizeof(BadUsbStudioApp));
    memset(app, 0, sizeof(BadUsbStudioApp));

    // Init storage
    payload_storage_init();

    // Open records
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Scene manager
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bad_usb_studio_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bad_usb_studio_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Allocate views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbStudioViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbStudioViewTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbStudioViewWidget, widget_get_view(app->widget));

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbStudioViewDialog, dialog_ex_get_view(app->dialog));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbStudioViewPopup, popup_get_view(app->popup));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BadUsbStudioViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->editor_view = script_editor_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BadUsbStudioViewEditor,
        script_editor_view_get_view(app->editor_view));

    app->full_keyboard = full_keyboard_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BadUsbStudioViewFullKeyboard,
        full_keyboard_view_get_view(app->full_keyboard));

    // Init app state
    app->file_path = furi_string_alloc();
    app->file_name = furi_string_alloc();
    app->cmd_prefix = furi_string_alloc();
    app->exec_error = furi_string_alloc();
    app->ble_rx_buf = furi_string_alloc();
    app->script_lines = NULL;
    app->script_line_count = 0;
    app->script_line_capacity = 0;
    app->editor_cursor = 0;
    app->script_modified = false;
    app->transport_type = BadUsbStudioTransportUsb;
    app->keyboard_layout = KeyboardLayoutUS;
    app->payload_names = NULL;
    app->payload_count = 0;
    app->exec_thread = NULL;
    app->is_executing = false;
    app->abort_requested = false;

    return app;
}

void bad_usb_studio_app_free(BadUsbStudioApp* app) {
    // Remove views
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbStudioViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbStudioViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbStudioViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbStudioViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbStudioViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbStudioViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbStudioViewEditor);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbStudioViewFullKeyboard);

    // Free views
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    widget_free(app->widget);
    dialog_ex_free(app->dialog);
    popup_free(app->popup);
    variable_item_list_free(app->variable_item_list);
    script_editor_view_free(app->editor_view);
    full_keyboard_view_free(app->full_keyboard);

    // Free scene manager & view dispatcher
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Free app state
    bad_usb_studio_script_clear(app);
    furi_string_free(app->file_path);
    furi_string_free(app->file_name);
    furi_string_free(app->cmd_prefix);
    furi_string_free(app->exec_error);
    furi_string_free(app->ble_rx_buf);

    // Free payload list
    if(app->payload_names) {
        for(uint16_t i = 0; i < app->payload_count; i++) {
            furi_string_free(app->payload_names[i]);
        }
        free(app->payload_names);
    }

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t bad_usb_studio_app(void* p) {
    UNUSED(p);

    BadUsbStudioApp* app = bad_usb_studio_app_alloc();
    scene_manager_next_scene(app->scene_manager, BadUsbStudioSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);
    bad_usb_studio_app_free(app);

    return 0;
}
