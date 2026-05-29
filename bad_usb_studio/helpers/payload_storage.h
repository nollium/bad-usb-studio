#pragma once

#include <furi.h>
#include <storage/storage.h>

#define BAD_USB_STUDIO_APP_PATH_FOLDER EXT_PATH("apps_data/bad_usb_studio")

// Ensure the app data directory exists
bool payload_storage_init(void);

// Get list of .txt payload files. Returns count. Caller must free names with furi_string_free.
uint16_t payload_storage_list(FuriString*** names_out);

// Load a payload file into an array of lines. Returns line count. Caller frees lines.
uint16_t payload_storage_load(const char* path, FuriString*** lines_out);

// Save lines to a file
bool payload_storage_save(const char* path, FuriString** lines, uint16_t line_count);

// Delete a payload file
bool payload_storage_delete(const char* path);

// Build full path from filename
void payload_storage_make_path(FuriString* out, const char* filename);
