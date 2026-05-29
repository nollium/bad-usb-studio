#include "payload_storage.h"
#include <storage/storage.h>

bool payload_storage_init(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, BAD_USB_STUDIO_APP_PATH_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return true;
}

uint16_t payload_storage_list(FuriString*** names_out) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    uint16_t count = 0;
    uint16_t capacity = 16;
    FuriString** names = malloc(sizeof(FuriString*) * capacity);

    if(storage_dir_open(dir, BAD_USB_STUDIO_APP_PATH_FOLDER)) {
        FileInfo file_info;
        char name_buf[256];

        while(storage_dir_read(dir, &file_info, name_buf, sizeof(name_buf))) {
            if(file_info.flags & FSF_DIRECTORY) continue;

            size_t len = strlen(name_buf);
            if(len < 4) continue;
            if(strcmp(name_buf + len - 4, ".txt") != 0) continue;

            if(count >= capacity) {
                capacity *= 2;
                names = realloc(names, sizeof(FuriString*) * capacity);
            }
            names[count] = furi_string_alloc_set(name_buf);
            count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    *names_out = names;
    return count;
}

uint16_t payload_storage_load(const char* path, FuriString*** lines_out) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    uint16_t count = 0;
    uint16_t capacity = 32;
    FuriString** lines = malloc(sizeof(FuriString*) * capacity);

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        char buf[1];
        bool has_content = false;

        furi_string_reset(line);
        while(storage_file_read(file, buf, 1) == 1) {
            if(buf[0] == '\n') {
                if(count >= capacity) {
                    capacity *= 2;
                    lines = realloc(lines, sizeof(FuriString*) * capacity);
                }
                lines[count] = furi_string_alloc_set(line);
                count++;
                furi_string_reset(line);
                has_content = false;
            } else if(buf[0] != '\r') {
                furi_string_push_back(line, buf[0]);
                has_content = true;
            }
        }

        if(has_content || furi_string_size(line) > 0) {
            if(count >= capacity) {
                capacity *= 2;
                lines = realloc(lines, sizeof(FuriString*) * capacity);
            }
            lines[count] = furi_string_alloc_set(line);
            count++;
        }

        furi_string_free(line);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    *lines_out = lines;
    return count;
}

bool payload_storage_save(const char* path, FuriString** lines, uint16_t line_count) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    payload_storage_init();

    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        success = true;
        for(uint16_t i = 0; i < line_count && success; i++) {
            const char* str = furi_string_get_cstr(lines[i]);
            size_t len = furi_string_size(lines[i]);

            if(len > 0) {
                if(storage_file_write(file, str, len) != len) {
                    success = false;
                }
            }
            if(success && i < line_count - 1) {
                if(storage_file_write(file, "\n", 1) != 1) {
                    success = false;
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

bool payload_storage_delete(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error err = storage_common_remove(storage, path);
    furi_record_close(RECORD_STORAGE);
    return err == FSE_OK;
}

void payload_storage_make_path(FuriString* out, const char* filename) {
    furi_string_printf(out, "%s/%s", BAD_USB_STUDIO_APP_PATH_FOLDER, filename);
}
