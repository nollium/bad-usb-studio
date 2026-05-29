#include "script_editor_view.h"
#include <gui/elements.h>

#define EDITOR_LINES_ON_SCREEN 4
#define EDITOR_LINE_HEIGHT 12
#define EDITOR_HEADER_HEIGHT 12
#define EDITOR_FOOTER_HEIGHT 12
#define EDITOR_MAX_CHARS_PER_LINE 20

typedef struct {
    FuriString** lines;
    uint16_t line_count;
    int16_t cursor;
    int16_t scroll_offset;
    bool modified;
} ScriptEditorViewModel;

struct ScriptEditorView {
    View* view;
    ScriptEditorCallback callback;
    void* callback_context;
};

static void script_editor_view_draw_callback(Canvas* canvas, void* model) {
    ScriptEditorViewModel* m = model;

    canvas_clear(canvas);

    // Header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Payload Editor");
    if(m->modified) {
        canvas_draw_str(canvas, 100, 10, "[*]");
    }
    canvas_draw_line(canvas, 0, EDITOR_HEADER_HEIGHT, 128, EDITOR_HEADER_HEIGHT);

    // Content area
    canvas_set_font(canvas, FontSecondary);
    if(m->line_count == 0) {
        canvas_draw_str_aligned(
            canvas, 64, 35, AlignCenter, AlignCenter, "Empty - press > to add");
    } else {
        for(int i = 0; i < EDITOR_LINES_ON_SCREEN; i++) {
            int16_t line_idx = m->scroll_offset + i;
            if(line_idx >= m->line_count) break;

            uint8_t y = EDITOR_HEADER_HEIGHT + 2 + (i * EDITOR_LINE_HEIGHT);

            if(line_idx == m->cursor) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, 0, y - 1, 128, EDITOR_LINE_HEIGHT);
                canvas_set_color(canvas, ColorWhite);
            }

            // Line number
            char num_buf[12];
            snprintf(num_buf, sizeof(num_buf), "%d:", line_idx + 1);
            canvas_draw_str(canvas, 1, y + 9, num_buf);

            // Line content (truncated)
            const char* str = furi_string_get_cstr(m->lines[line_idx]);
            char display_buf[EDITOR_MAX_CHARS_PER_LINE + 1];
            size_t len = strlen(str);
            if(len > EDITOR_MAX_CHARS_PER_LINE) {
                strncpy(display_buf, str, EDITOR_MAX_CHARS_PER_LINE - 1);
                display_buf[EDITOR_MAX_CHARS_PER_LINE - 1] = '~';
                display_buf[EDITOR_MAX_CHARS_PER_LINE] = '\0';
            } else {
                strncpy(display_buf, str, sizeof(display_buf) - 1);
                display_buf[sizeof(display_buf) - 1] = '\0';
            }
            canvas_draw_str(canvas, 18, y + 9, display_buf);

            if(line_idx == m->cursor) {
                canvas_set_color(canvas, ColorBlack);
            }
        }

        // Scrollbar
        if(m->line_count > EDITOR_LINES_ON_SCREEN) {
            uint8_t scroll_y = EDITOR_HEADER_HEIGHT + 1;
            uint8_t scroll_h = 64 - EDITOR_HEADER_HEIGHT - EDITOR_FOOTER_HEIGHT - 2;
            uint8_t bar_h = (EDITOR_LINES_ON_SCREEN * scroll_h) / m->line_count;
            if(bar_h < 3) bar_h = 3;
            uint8_t bar_y = scroll_y + (m->scroll_offset * scroll_h) / m->line_count;
            canvas_draw_box(canvas, 126, bar_y, 2, bar_h);
        }
    }

    // Footer
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 52, 128, 52);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 62, "OK:Edit >:Add [OK]:Save");
    if(m->line_count > 0) {
        char pos_buf[16];
        snprintf(pos_buf, sizeof(pos_buf), "%d/%d", m->cursor + 1, m->line_count);
        canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, pos_buf);
    }
}

static bool script_editor_view_input_callback(InputEvent* event, void* context) {
    ScriptEditorView* editor = context;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        bool consumed = false;
        bool redraw = false;

        with_view_model(
            editor->view,
            ScriptEditorViewModel * m,
            {
                switch(event->key) {
                case InputKeyUp:
                    if(m->cursor > 0) {
                        m->cursor--;
                        if(m->cursor < m->scroll_offset) {
                            m->scroll_offset = m->cursor;
                        }
                        redraw = true;
                    }
                    consumed = true;
                    break;
                case InputKeyDown:
                    if(m->cursor < m->line_count - 1) {
                        m->cursor++;
                        if(m->cursor >= m->scroll_offset + EDITOR_LINES_ON_SCREEN) {
                            m->scroll_offset = m->cursor - EDITOR_LINES_ON_SCREEN + 1;
                        }
                        redraw = true;
                    }
                    consumed = true;
                    break;
                case InputKeyOk:
                    if(m->line_count > 0) {
                        consumed = true;
                    }
                    break;
                case InputKeyLeft:
                    if(m->line_count > 0) {
                        consumed = true;
                    }
                    break;
                case InputKeyRight:
                    consumed = true;
                    break;
                default:
                    break;
                }
            },
            redraw);

        if(consumed && editor->callback) {
            switch(event->key) {
            case InputKeyOk:
                editor->callback(ScriptEditorEventEditLine, editor->callback_context);
                break;
            case InputKeyLeft:
                editor->callback(ScriptEditorEventDeleteLine, editor->callback_context);
                break;
            case InputKeyRight:
                editor->callback(ScriptEditorEventAddLine, editor->callback_context);
                break;
            default:
                break;
            }
        }

        return consumed;
    }

    // Long press OK = save
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(editor->callback) {
            editor->callback(ScriptEditorEventSave, editor->callback_context);
        }
        return true;
    }

    return false;
}

ScriptEditorView* script_editor_view_alloc(void) {
    ScriptEditorView* editor = malloc(sizeof(ScriptEditorView));
    editor->view = view_alloc();
    editor->callback = NULL;
    editor->callback_context = NULL;

    view_allocate_model(editor->view, ViewModelTypeLocking, sizeof(ScriptEditorViewModel));
    view_set_draw_callback(editor->view, script_editor_view_draw_callback);
    view_set_input_callback(editor->view, script_editor_view_input_callback);
    view_set_context(editor->view, editor);

    with_view_model(
        editor->view,
        ScriptEditorViewModel * m,
        {
            m->lines = NULL;
            m->line_count = 0;
            m->cursor = 0;
            m->scroll_offset = 0;
            m->modified = false;
        },
        true);

    return editor;
}

void script_editor_view_free(ScriptEditorView* editor) {
    view_free(editor->view);
    free(editor);
}

View* script_editor_view_get_view(ScriptEditorView* editor) {
    return editor->view;
}

void script_editor_view_set_callback(
    ScriptEditorView* editor,
    ScriptEditorCallback callback,
    void* context) {
    editor->callback = callback;
    editor->callback_context = context;
}

void script_editor_view_set_data(
    ScriptEditorView* editor,
    FuriString** lines,
    uint16_t line_count,
    bool modified) {
    with_view_model(
        editor->view,
        ScriptEditorViewModel * m,
        {
            m->lines = lines;
            m->line_count = line_count;
            m->modified = modified;
            if(m->cursor >= line_count && line_count > 0) {
                m->cursor = line_count - 1;
            }
            if(line_count == 0) {
                m->cursor = 0;
                m->scroll_offset = 0;
            }
        },
        true);
}

int16_t script_editor_view_get_cursor(ScriptEditorView* editor) {
    int16_t cursor = 0;
    with_view_model(
        editor->view, ScriptEditorViewModel * m, { cursor = m->cursor; }, false);
    return cursor;
}

void script_editor_view_set_cursor(ScriptEditorView* editor, int16_t cursor) {
    with_view_model(
        editor->view,
        ScriptEditorViewModel * m,
        {
            if(cursor >= 0 && cursor < m->line_count) {
                m->cursor = cursor;
                if(m->cursor < m->scroll_offset) {
                    m->scroll_offset = m->cursor;
                }
                if(m->cursor >= m->scroll_offset + EDITOR_LINES_ON_SCREEN) {
                    m->scroll_offset = m->cursor - EDITOR_LINES_ON_SCREEN + 1;
                }
            }
        },
        true);
}
