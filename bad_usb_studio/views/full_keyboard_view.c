#include "full_keyboard_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

#define MAX_TEXT_LEN 256
#define KEYS_PER_ROW 10
#define KEY_W 12
#define KEY_H 11
#define KB_X_OFFSET 4
#define KB_Y_OFFSET 22
#define TEXT_Y 10

// 4 pages of characters, switchable with Left/Right
static const char* const key_pages[] = {
    "abcdefghij"
    "klmnopqrst"
    "uvwxyz .\b\n",

    "ABCDEFGHIJ"
    "KLMNOPQRST"
    "UVWXYZ .\b\n",

    "0123456789"
    "!@#$%^&*()"
    "-=_+[]{}.\n",

    "\\|;:'\",.<>"
    "/?`~ !@#$%"
    "^&*()-_+=\n",
};

#define PAGE_COUNT (sizeof(key_pages) / sizeof(key_pages[0]))
#define ROWS_PER_PAGE 3

typedef struct {
    char text[MAX_TEXT_LEN];
    uint16_t text_len;
    uint8_t page;
    uint8_t cursor_x;
    uint8_t cursor_y;
    const char* header;
} FullKeyboardModel;

struct FullKeyboardView {
    View* view;
    FullKeyboardCallback callback;
    void* callback_context;
};

static char get_key_at(const FullKeyboardModel* m, uint8_t x, uint8_t y) {
    uint8_t idx = y * KEYS_PER_ROW + x;
    const char* page = key_pages[m->page];
    uint8_t page_len = strlen(page);
    if(idx >= page_len) return 0;
    return page[idx];
}

static const char* get_key_label(char c, char* buf) {
    if(c == '\b') return "<-";
    if(c == '\n') return "OK";
    if(c == ' ') return "SP";
    buf[0] = c;
    buf[1] = '\0';
    return buf;
}

static void full_keyboard_view_draw(Canvas* canvas, void* model) {
    FullKeyboardModel* m = model;

    canvas_clear(canvas);

    // Header
    canvas_set_font(canvas, FontSecondary);
    if(m->header) {
        canvas_draw_str(canvas, 2, 8, m->header);
    }

    // Text preview (right-aligned if too long)
    canvas_set_font(canvas, FontPrimary);
    const char* display_text = m->text;
    uint8_t max_display = 18;
    if(m->text_len > max_display) {
        display_text = m->text + m->text_len - max_display;
        canvas_draw_str(canvas, 2, TEXT_Y + 8, "..");
        canvas_draw_str(canvas, 14, TEXT_Y + 8, display_text);
    } else if(m->text_len > 0) {
        canvas_draw_str(canvas, 2, TEXT_Y + 8, display_text);
    }
    // Cursor blink
    uint8_t cursor_x_pos = 2;
    if(m->text_len > max_display) {
        cursor_x_pos = 14 + canvas_string_width(canvas, display_text);
    } else {
        cursor_x_pos = 2 + canvas_string_width(canvas, display_text);
    }
    canvas_draw_str(canvas, cursor_x_pos, TEXT_Y + 8, "_");

    // Page indicator
    canvas_set_font(canvas, FontSecondary);
    char page_buf[8];
    snprintf(page_buf, sizeof(page_buf), "<%d/%d>", m->page + 1, (int)PAGE_COUNT);
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, page_buf);

    // Keyboard grid
    canvas_set_font(canvas, FontSecondary);
    char label_buf[2];
    for(uint8_t row = 0; row < ROWS_PER_PAGE; row++) {
        for(uint8_t col = 0; col < KEYS_PER_ROW; col++) {
            char c = get_key_at(m, col, row);
            if(!c) continue;

            uint8_t x = KB_X_OFFSET + col * KEY_W;
            uint8_t y = KB_Y_OFFSET + row * KEY_H;

            if(col == m->cursor_x && row == m->cursor_y) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rbox(canvas, x, y, KEY_W, KEY_H, 1);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, x, y, KEY_W, KEY_H, 1);
            }

            const char* label = get_key_label(c, label_buf);
            canvas_draw_str_aligned(
                canvas, x + KEY_W / 2, y + KEY_H / 2 + 1, AlignCenter, AlignCenter, label);

            if(col == m->cursor_x && row == m->cursor_y) {
                canvas_set_color(canvas, ColorBlack);
            }
        }
    }

    // Footer
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, "Back:cancel");
}

static bool full_keyboard_view_input(InputEvent* event, void* context) {
    FullKeyboardView* kb = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool fire_callback = false;

    with_view_model(
        kb->view,
        FullKeyboardModel * m,
        {
            switch(event->key) {
            case InputKeyUp:
                if(m->cursor_y > 0)
                    m->cursor_y--;
                else
                    m->cursor_y = ROWS_PER_PAGE - 1;
                consumed = true;
                break;
            case InputKeyDown:
                if(m->cursor_y < ROWS_PER_PAGE - 1)
                    m->cursor_y++;
                else
                    m->cursor_y = 0;
                consumed = true;
                break;
            case InputKeyLeft:
                if(m->cursor_x > 0) {
                    m->cursor_x--;
                } else {
                    // Switch page left
                    m->page = (m->page + PAGE_COUNT - 1) % PAGE_COUNT;
                    m->cursor_x = KEYS_PER_ROW - 1;
                }
                consumed = true;
                break;
            case InputKeyRight:
                if(m->cursor_x < KEYS_PER_ROW - 1) {
                    m->cursor_x++;
                } else {
                    // Switch page right
                    m->page = (m->page + 1) % PAGE_COUNT;
                    m->cursor_x = 0;
                }
                consumed = true;
                break;
            case InputKeyOk: {
                char c = get_key_at(m, m->cursor_x, m->cursor_y);
                if(c == '\b') {
                    // Backspace
                    if(m->text_len > 0) {
                        m->text_len--;
                        m->text[m->text_len] = '\0';
                    }
                } else if(c == '\n') {
                    // Submit
                    fire_callback = true;
                } else if(c && m->text_len < MAX_TEXT_LEN - 1) {
                    m->text[m->text_len] = c;
                    m->text_len++;
                    m->text[m->text_len] = '\0';
                }
                consumed = true;
                break;
            }
            default:
                break;
            }
        },
        true);

    if(fire_callback && kb->callback) {
        char text_copy[MAX_TEXT_LEN];
        with_view_model(
            kb->view,
            FullKeyboardModel * m,
            {
                strncpy(text_copy, m->text, MAX_TEXT_LEN);
                text_copy[MAX_TEXT_LEN - 1] = '\0';
            },
            false);
        kb->callback(text_copy, kb->callback_context);
    }

    return consumed;
}

FullKeyboardView* full_keyboard_view_alloc(void) {
    FullKeyboardView* kb = malloc(sizeof(FullKeyboardView));
    kb->view = view_alloc();
    kb->callback = NULL;
    kb->callback_context = NULL;

    view_allocate_model(kb->view, ViewModelTypeLocking, sizeof(FullKeyboardModel));
    view_set_draw_callback(kb->view, full_keyboard_view_draw);
    view_set_input_callback(kb->view, full_keyboard_view_input);
    view_set_context(kb->view, kb);

    with_view_model(
        kb->view,
        FullKeyboardModel * m,
        {
            m->text[0] = '\0';
            m->text_len = 0;
            m->page = 0;
            m->cursor_x = 0;
            m->cursor_y = 0;
            m->header = NULL;
        },
        true);

    return kb;
}

void full_keyboard_view_free(FullKeyboardView* kb) {
    view_free(kb->view);
    free(kb);
}

View* full_keyboard_view_get_view(FullKeyboardView* kb) {
    return kb->view;
}

void full_keyboard_view_set_callback(
    FullKeyboardView* kb,
    FullKeyboardCallback callback,
    void* context) {
    kb->callback = callback;
    kb->callback_context = context;
}

void full_keyboard_view_set_header(FullKeyboardView* kb, const char* header) {
    with_view_model(
        kb->view, FullKeyboardModel * m, { m->header = header; }, true);
}

void full_keyboard_view_clear(FullKeyboardView* kb) {
    with_view_model(
        kb->view,
        FullKeyboardModel * m,
        {
            m->text[0] = '\0';
            m->text_len = 0;
        },
        true);
}
