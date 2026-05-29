#include "ducky_script.h"
#include "ducky_script_keycodes.h"
#include "keyboard_layout.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    const char* name;
    uint16_t keycode;
} KeywordMapping;

static const KeywordMapping keyword_keys[] = {
    {"ENTER", HID_KEYBOARD_RETURN},
    {"RETURN", HID_KEYBOARD_RETURN},
    {"TAB", HID_KEYBOARD_TAB},
    {"ESCAPE", HID_KEYBOARD_ESCAPE},
    {"ESC", HID_KEYBOARD_ESCAPE},
    {"SPACE", HID_KEYBOARD_SPACEBAR},
    {"BACKSPACE", HID_KEYBOARD_DELETE},
    {"DELETE", HID_KEYBOARD_DELETE_FORWARD},
    {"DEL", HID_KEYBOARD_DELETE_FORWARD},
    {"INSERT", HID_KEYBOARD_INSERT},
    {"HOME", HID_KEYBOARD_HOME},
    {"END", HID_KEYBOARD_END},
    {"PAGEUP", HID_KEYBOARD_PAGE_UP},
    {"PAGEDOWN", HID_KEYBOARD_PAGE_DOWN},
    {"UPARROW", HID_KEYBOARD_UP_ARROW},
    {"UP", HID_KEYBOARD_UP_ARROW},
    {"DOWNARROW", HID_KEYBOARD_DOWN_ARROW},
    {"DOWN", HID_KEYBOARD_DOWN_ARROW},
    {"LEFTARROW", HID_KEYBOARD_LEFT_ARROW},
    {"LEFT", HID_KEYBOARD_LEFT_ARROW},
    {"RIGHTARROW", HID_KEYBOARD_RIGHT_ARROW},
    {"RIGHT", HID_KEYBOARD_RIGHT_ARROW},
    {"CAPSLOCK", HID_KEYBOARD_CAPS_LOCK},
    {"NUMLOCK", 0x53},
    {"SCROLLLOCK", HID_KEYBOARD_SCROLL_LOCK},
    {"PRINTSCREEN", HID_KEYBOARD_PRINT_SCREEN},
    {"PAUSE", HID_KEYBOARD_PAUSE},
    {"BREAK", HID_KEYBOARD_PAUSE},
    {"F1", HID_KEYBOARD_F1},
    {"F2", HID_KEYBOARD_F2},
    {"F3", HID_KEYBOARD_F3},
    {"F4", HID_KEYBOARD_F4},
    {"F5", HID_KEYBOARD_F5},
    {"F6", HID_KEYBOARD_F6},
    {"F7", HID_KEYBOARD_F7},
    {"F8", HID_KEYBOARD_F8},
    {"F9", HID_KEYBOARD_F9},
    {"F10", HID_KEYBOARD_F10},
    {"F11", HID_KEYBOARD_F11},
    {"F12", HID_KEYBOARD_F12},
    {NULL, 0},
};

static uint16_t ducky_find_key(const char* name) {
    for(const KeywordMapping* k = keyword_keys; k->name; k++) {
        if(strcasecmp(k->name, name) == 0) return k->keycode;
    }
    if(strlen(name) == 1 && isalpha((unsigned char)name[0])) {
        return HID_KEYBOARD_A + (toupper((unsigned char)name[0]) - 'A');
    }
    return HID_KEYBOARD_NONE;
}

static void ducky_type_string(DuckyState* state, const char* str) {
    for(; *str; str++) {
        uint16_t key = keyboard_layout_map_char(state->layout, *str);
        if(key == HID_KEYBOARD_NONE) continue;
        state->transport->kb_press(key);
        furi_delay_ms(10);
        state->transport->kb_release_all();
        furi_delay_ms(10);
    }
}

static void ducky_press_key_combo(DuckyState* state, const char* line) {
    uint16_t modifiers[4] = {0};
    int mod_count = 0;
    uint16_t key = HID_KEYBOARD_NONE;

    char buf[256];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Replace dashes/plus with spaces for tokenization
    for(char* p = buf; *p; p++) {
        if(*p == '-' || *p == '+') *p = ' ';
    }

    // Manual tokenization (strtok_r not in Flipper API)
    char* p = buf;
    while(*p) {
        while(*p == ' ') p++;
        if(!*p) break;

        char* token_start = p;
        while(*p && *p != ' ') p++;
        char saved = *p;
        *p = '\0';

        if(strcasecmp(token_start, "CTRL") == 0 || strcasecmp(token_start, "CONTROL") == 0) {
            if(mod_count < 4) modifiers[mod_count++] = HID_KEYBOARD_L_CTRL;
        } else if(strcasecmp(token_start, "SHIFT") == 0) {
            if(mod_count < 4) modifiers[mod_count++] = HID_KEYBOARD_L_SHIFT;
        } else if(strcasecmp(token_start, "ALT") == 0) {
            if(mod_count < 4) modifiers[mod_count++] = HID_KEYBOARD_L_ALT;
        } else if(strcasecmp(token_start, "GUI") == 0 || strcasecmp(token_start, "WINDOWS") == 0 ||
                   strcasecmp(token_start, "COMMAND") == 0 || strcasecmp(token_start, "META") == 0) {
            if(mod_count < 4) modifiers[mod_count++] = HID_KEYBOARD_L_GUI;
        } else {
            key = ducky_find_key(token_start);
        }

        if(saved) p++;
    }

    for(int i = 0; i < mod_count; i++) {
        state->transport->kb_press(modifiers[i]);
    }
    if(key != HID_KEYBOARD_NONE) {
        state->transport->kb_press(key);
    }
    furi_delay_ms(50);
    state->transport->kb_release_all();
    furi_delay_ms(10);
}

DuckyState* ducky_state_alloc(void) {
    DuckyState* state = malloc(sizeof(DuckyState));
    state->transport = NULL;
    state->layout = KeyboardLayoutUS;
    state->default_delay = 0;
    state->abort_flag = NULL;
    state->last_command = furi_string_alloc();
    return state;
}

void ducky_state_free(DuckyState* state) {
    furi_string_free(state->last_command);
    free(state);
}

DuckyStatus ducky_execute_line(DuckyState* state, const char* line) {
    while(*line == ' ' || *line == '\t') line++;
    if(*line == '\0') return DuckyStatusOk;
    if(strncasecmp(line, "REM", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
        return DuckyStatusOk;
    }

    if(state->abort_flag && *state->abort_flag) {
        return DuckyStatusAborted;
    }

    if(strncasecmp(line, "DELAY ", 6) == 0) {
        int ms = atoi(line + 6);
        if(ms > 0) {
            while(ms > 0) {
                if(state->abort_flag && *state->abort_flag) return DuckyStatusAborted;
                int chunk = (ms > 100) ? 100 : ms;
                furi_delay_ms(chunk);
                ms -= chunk;
            }
        }
        furi_string_set_str(state->last_command, line);
        return DuckyStatusOk;
    }

    if(strncasecmp(line, "DEFAULT_DELAY ", 14) == 0 ||
       strncasecmp(line, "DEFAULTDELAY ", 13) == 0) {
        const char* val = strchr(line, ' ') + 1;
        state->default_delay = atoi(val);
        return DuckyStatusOk;
    }

    if(strncasecmp(line, "STRING ", 7) == 0) {
        ducky_type_string(state, line + 7);
        furi_string_set_str(state->last_command, line);
        return DuckyStatusOk;
    }

    if(strncasecmp(line, "STRINGLN ", 9) == 0) {
        ducky_type_string(state, line + 9);
        state->transport->kb_press(HID_KEYBOARD_RETURN);
        furi_delay_ms(10);
        state->transport->kb_release_all();
        furi_string_set_str(state->last_command, line);
        return DuckyStatusOk;
    }

    if(strncasecmp(line, "REPEAT ", 7) == 0) {
        int count = atoi(line + 7);
        if(count > 0 && furi_string_size(state->last_command) > 0) {
            const char* last = furi_string_get_cstr(state->last_command);
            for(int i = 0; i < count; i++) {
                if(state->abort_flag && *state->abort_flag) return DuckyStatusAborted;
                DuckyStatus s = ducky_execute_line(state, last);
                if(s != DuckyStatusOk) return s;
                if(state->default_delay > 0) furi_delay_ms(state->default_delay);
            }
        }
        return DuckyStatusOk;
    }

    // Standalone special key
    uint16_t standalone_key = ducky_find_key(line);
    if(standalone_key != HID_KEYBOARD_NONE) {
        state->transport->kb_press(standalone_key);
        furi_delay_ms(50);
        state->transport->kb_release_all();
        furi_delay_ms(10);
        furi_string_set_str(state->last_command, line);
        return DuckyStatusOk;
    }

    // Modifier combos: GUI, CTRL, ALT, SHIFT (with optional key argument)
    if(strncasecmp(line, "GUI", 3) == 0 || strncasecmp(line, "WINDOWS", 7) == 0 ||
       strncasecmp(line, "COMMAND", 7) == 0 || strncasecmp(line, "META", 4) == 0 ||
       strncasecmp(line, "CTRL", 4) == 0 || strncasecmp(line, "CONTROL", 7) == 0 ||
       strncasecmp(line, "ALT", 3) == 0 || strncasecmp(line, "SHIFT", 5) == 0) {
        ducky_press_key_combo(state, line);
        furi_string_set_str(state->last_command, line);
        return DuckyStatusOk;
    }

    // Unknown — try as key combo anyway
    ducky_press_key_combo(state, line);
    furi_string_set_str(state->last_command, line);
    return DuckyStatusOk;
}

DuckyStatus ducky_execute_script(
    DuckyState* state,
    FuriString** lines,
    uint16_t line_count,
    DuckyProgressCallback progress_cb,
    void* cb_context) {
    for(uint16_t i = 0; i < line_count; i++) {
        if(state->abort_flag && *state->abort_flag) {
            return DuckyStatusAborted;
        }

        const char* line = furi_string_get_cstr(lines[i]);
        DuckyStatus status = ducky_execute_line(state, line);

        if(status != DuckyStatusOk) {
            return status;
        }

        if(state->default_delay > 0) {
            furi_delay_ms(state->default_delay);
        }

        if(progress_cb) {
            progress_cb(i + 1, line_count, cb_context);
        }
    }

    return DuckyStatusDone;
}
