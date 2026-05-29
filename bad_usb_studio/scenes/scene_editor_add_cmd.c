#include "../bad_usb_studio.h"

typedef struct {
    const char* label;
    const char* prefix;
    bool needs_arg;
} CmdEntry;

static const CmdEntry cmd_entries[] = {
    {"STRING ...", "STRING ", true},
    {"DELAY ...", "DELAY ", true},
    {"ENTER", "ENTER", false},
    {"TAB", "TAB", false},
    {"ESCAPE", "ESCAPE", false},
    {"SPACE", "SPACE", false},
    {"BACKSPACE", "BACKSPACE", false},
    {"GUI ...", "GUI ", true},
    {"GUI r (Run)", "GUI r", false},
    {"CTRL ...", "CTRL ", true},
    {"ALT ...", "ALT ", true},
    {"SHIFT ...", "SHIFT ", true},
    {"REM ...", "REM ", true},
    {"REPEAT ...", "REPEAT ", true},
    {"DEFAULT_DELAY ...", "DEFAULT_DELAY ", true},
    {"STRINGLN ...", "STRINGLN ", true},
    {"Raw line ...", "", true},
};

#define CMD_ENTRY_COUNT (sizeof(cmd_entries) / sizeof(cmd_entries[0]))

static void bad_usb_studio_scene_editor_add_cmd_callback(void* context, uint32_t index) {
    BadUsbStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bad_usb_studio_scene_editor_add_cmd_on_enter(void* context) {
    BadUsbStudioApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Add Command");

    for(uint32_t i = 0; i < CMD_ENTRY_COUNT; i++) {
        submenu_add_item(
            submenu,
            cmd_entries[i].label,
            i,
            bad_usb_studio_scene_editor_add_cmd_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbStudioViewSubmenu);
}

bool bad_usb_studio_scene_editor_add_cmd_on_event(void* context, SceneManagerEvent event) {
    BadUsbStudioApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t index = event.event;
        if(index >= CMD_ENTRY_COUNT) return false;

        const CmdEntry* entry = &cmd_entries[index];

        if(entry->needs_arg) {
            strncpy(
                app->text_input_buf, entry->prefix, BAD_USB_STUDIO_MAX_LINE_LENGTH - 1);
            app->text_input_buf[BAD_USB_STUDIO_MAX_LINE_LENGTH - 1] = '\0';
            app->text_input_is_insert = true;
            scene_manager_next_scene(
                app->scene_manager, BadUsbStudioSceneEditorEditLine);
        } else {
            uint16_t insert_pos = app->editor_cursor;
            if(app->script_line_count > 0) insert_pos++;
            bad_usb_studio_script_insert_line(app, insert_pos, entry->prefix);
            app->script_modified = true;
            app->editor_cursor = insert_pos;
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    }

    return false;
}

void bad_usb_studio_scene_editor_add_cmd_on_exit(void* context) {
    BadUsbStudioApp* app = context;
    submenu_reset(app->submenu);
}
