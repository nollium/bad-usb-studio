#pragma once

#include <gui/view.h>
#include <furi.h>

typedef struct ScriptEditorView ScriptEditorView;

typedef enum {
    ScriptEditorEventEditLine,
    ScriptEditorEventAddLine,
    ScriptEditorEventDeleteLine,
    ScriptEditorEventSave,
} ScriptEditorEvent;

typedef void (*ScriptEditorCallback)(ScriptEditorEvent event, void* context);

ScriptEditorView* script_editor_view_alloc(void);
void script_editor_view_free(ScriptEditorView* editor);

View* script_editor_view_get_view(ScriptEditorView* editor);

void script_editor_view_set_callback(
    ScriptEditorView* editor,
    ScriptEditorCallback callback,
    void* context);

// Update the displayed script data
void script_editor_view_set_data(
    ScriptEditorView* editor,
    FuriString** lines,
    uint16_t line_count,
    bool modified);

// Get/set cursor position
int16_t script_editor_view_get_cursor(ScriptEditorView* editor);
void script_editor_view_set_cursor(ScriptEditorView* editor, int16_t cursor);
