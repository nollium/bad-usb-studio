#pragma once

#include <furi.h>
#include "hid_transport.h"
#include "keyboard_layout.h"

typedef enum {
    DuckyStatusOk,
    DuckyStatusDone,
    DuckyStatusError,
    DuckyStatusAborted,
} DuckyStatus;

typedef struct {
    const HidTransport* transport;
    KeyboardLayout layout;
    uint16_t default_delay;
    volatile bool* abort_flag;
    FuriString* last_command;
} DuckyState;

DuckyState* ducky_state_alloc(void);
void ducky_state_free(DuckyState* state);

DuckyStatus ducky_execute_line(DuckyState* state, const char* line);

typedef void (*DuckyProgressCallback)(uint16_t current_line, uint16_t total_lines, void* context);

DuckyStatus ducky_execute_script(
    DuckyState* state,
    FuriString** lines,
    uint16_t line_count,
    DuckyProgressCallback progress_cb,
    void* cb_context);
