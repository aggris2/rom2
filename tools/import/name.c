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

/* Adds `name` to the text of `length` characters in `out`, which has
 * room for `size` bytes: in lower case, with one '_' for every run of
 * characters that are not letters or digits. With `gap` a '_' also goes
 * in front. Stops when `out` is full. Returns the new length. */
static size_t append_label(char *out, size_t length, size_t size,
    const char *name, bool gap)
{
    for (const char *p = name; *p != '\0'; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
            /* A '_' is owed before the next letter or digit, unless
             * nothing has been written yet. */
            gap = gap || length > 0;
            continue;
        }
        if (length + (gap ? 2u : 1u) >= size) {
            break;
        }
        if (gap) {
            out[length++] = '_';
            gap = false;
        }
        out[length++] = c;
    }
    out[length] = '\0';
    return length;
}

bool name_folder(int32_t id, const char *name, char out[NAME_FOLDER_SIZE])
{
    if (id < 0 || id > 999) {
        return false;
    }
    char folder[NAME_FOLDER_SIZE];
    size_t length = (size_t)snprintf(folder, sizeof folder, "%03d", (int)id);
    length = append_label(folder, length, sizeof folder, name, true);
    memcpy(out, folder, length + 1);
    return true;
}

void name_label(const char *name, char *out, size_t size)
{
    if (size == 0) {
        return;
    }
    append_label(out, 0, size, name, false);
}
