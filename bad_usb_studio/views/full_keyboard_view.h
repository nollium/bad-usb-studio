#pragma once

#include <gui/view.h>

typedef struct FullKeyboardView FullKeyboardView;

typedef void (*FullKeyboardCallback)(const char* text, void* context);

FullKeyboardView* full_keyboard_view_alloc(void);
void full_keyboard_view_free(FullKeyboardView* kb);

View* full_keyboard_view_get_view(FullKeyboardView* kb);

void full_keyboard_view_set_callback(
    FullKeyboardView* kb,
    FullKeyboardCallback callback,
    void* context);

void full_keyboard_view_set_header(FullKeyboardView* kb, const char* header);
void full_keyboard_view_clear(FullKeyboardView* kb);
