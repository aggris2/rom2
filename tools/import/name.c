#include "import/name.h"

#include <stdio.h>
#include <string.h>

void name_copy(char *dst, size_t dst_size, const uint8_t *field,
    size_t field_size)
{
    if (dst_size == 0) {
        return;
    }
    size_t i = 0;
    while (i < dst_size - 1 && i < field_size && field[i] != 0) {
        dst[i] = (char)field[i];
        i++;
    }
    dst[i] = '\0';
}

bool name_folder(int32_t id, const char *name, char out[NAME_FOLDER_SIZE])
{
    if (id < 0 || id > 999) {
        return false;
    }
    char folder[NAME_FOLDER_SIZE];
    size_t length = (size_t)snprintf(folder, sizeof folder, "%03d", (int)id);
    /* A '_' is owed before the next letter or digit: after the ID, and
     * after any run of other characters. */
    bool gap = true;
    for (const char *p = name; *p != '\0'; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
            gap = true;
            continue;
        }
        if (length + (gap ? 2u : 1u) >= sizeof folder) {
            break;
        }
        if (gap) {
            folder[length++] = '_';
            gap = false;
        }
        folder[length++] = c;
    }
    folder[length] = '\0';
    memcpy(out, folder, length + 1);
    return true;
}
