#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>

#include "views/script_editor_view.h"
#include "views/full_keyboard_view.h"
#include "helpers/ducky_script.h"
#include "helpers/hid_transport.h"
#include "helpers/keyboard_layout.h"
#include "helpers/payload_storage.h"

#define BAD_USB_STUDIO_MAX_LINE_LENGTH 256
#define BAD_USB_STUDIO_MAX_LINES 512
#define BAD_USB_STUDIO_MAX_FILENAME 64

typedef enum {
    BadUsbStudioViewSubmenu,
    BadUsbStudioViewTextInput,
    BadUsbStudioViewWidget,
    BadUsbStudioViewDialog,
    BadUsbStudioViewPopup,
    BadUsbStudioViewEditor,
    BadUsbStudioViewVariableItemList,
    BadUsbStudioViewFullKeyboard,
} BadUsbStudioView;

typedef enum {
#define ADD_SCENE(prefix, name, id) BadUsbStudioScene##id,
#include "scenes/scene_config.h"
#undef ADD_SCENE
    BadUsbStudioSceneCount,
} BadUsbStudioScene;

typedef enum {
    BadUsbStudioCustomEventNone,
    BadUsbStudioCustomEventMainMenuNewPayload,
    BadUsbStudioCustomEventMainMenuSavedPayloads,
    BadUsbStudioCustomEventMainMenuQuickType,
    BadUsbStudioCustomEventMainMenuSettings,
    BadUsbStudioCustomEventMainMenuPhoneInput,
    BadUsbStudioCustomEventPayloadActionsExecute,
    BadUsbStudioCustomEventPayloadActionsEdit,
    BadUsbStudioCustomEventPayloadActionsDelete,
    BadUsbStudioCustomEventEditorEditLine,
    BadUsbStudioCustomEventEditorAddLine,
    BadUsbStudioCustomEventEditorDeleteLine,
    BadUsbStudioCustomEventEditorSave,
    // AddCmd events are index-based (0..N), no enum needed
    BadUsbStudioCustomEventTextInputDone,
    BadUsbStudioCustomEventDialogLeft,
    BadUsbStudioCustomEventDialogRight,
    BadUsbStudioCustomEventExecDone,
    BadUsbStudioCustomEventExecError,
    BadUsbStudioCustomEventExecProgress,
    BadUsbStudioCustomEventQuickTypeSubmit,
    BadUsbStudioCustomEventQuickTypeResultType,
    BadUsbStudioCustomEventQuickTypeResultSave,
    BadUsbStudioCustomEventPhoneData,
    BadUsbStudioCustomEventPhoneConnected,
    BadUsbStudioCustomEventPhoneDisconnected,
} BadUsbStudioCustomEvent;

typedef enum {
    BadUsbStudioTransportUsb,
    BadUsbStudioTransportBle,
    BadUsbStudioTransportCount,
} BadUsbStudioTransportType;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    DialogEx* dialog;
    Popup* popup;
    VariableItemList* variable_item_list;

    ScriptEditorView* editor_view;
    FullKeyboardView* full_keyboard;

    FuriString** script_lines;
    uint16_t script_line_count;
    uint16_t script_line_capacity;
    int16_t editor_cursor;
    bool script_modified;

    FuriString* file_path;
    FuriString* file_name;

    char text_input_buf[BAD_USB_STUDIO_MAX_LINE_LENGTH];

    BadUsbStudioTransportType transport_type;
    KeyboardLayout keyboard_layout;

    FuriThread* exec_thread;
    bool is_executing;
    volatile bool abort_requested;
    uint16_t current_exec_line;
    uint16_t total_exec_lines;
    FuriString* exec_error;

    FuriString* cmd_prefix;
    bool text_input_is_insert;

    // BLE serial receive buffer for phone input
    FuriString* ble_rx_buf;

    // Payload list state
    FuriString** payload_names;
    uint16_t payload_count;
} BadUsbStudioApp;

BadUsbStudioApp* bad_usb_studio_app_alloc(void);
void bad_usb_studio_app_free(BadUsbStudioApp* app);

void bad_usb_studio_script_clear(BadUsbStudioApp* app);
void bad_usb_studio_script_add_line(BadUsbStudioApp* app, const char* line);
void bad_usb_studio_script_insert_line(BadUsbStudioApp* app, uint16_t index, const char* line);
void bad_usb_studio_script_delete_line(BadUsbStudioApp* app, uint16_t index);
void bad_usb_studio_script_set_line(BadUsbStudioApp* app, uint16_t index, const char* line);

#define ADD_SCENE(prefix, name, id)                                          \
    void prefix##_scene_##name##_on_enter(void* context);                    \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "scenes/scene_config.h"
#undef ADD_SCENE
