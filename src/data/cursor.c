#include "data/cursor.h"

#include <string.h>

#define MOST 4096

/* A name that stays in the folder it is given in. */
static bool is_plain_file_name(const char *name)
{
    size_t length = strlen(name);
    return length > 0 && length < CURSOR_NAME_SIZE
        && strchr(name, '/') == NULL && strchr(name, '\\') == NULL
        && strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

/* Reads a key that holds two whole numbers. */
static bool read_pair(const TomlTable *table, const char *key,
                      int32_t *first, int32_t *second)
{
    const int32_t *values = NULL;
    size_t count = 0;
    if (!toml_get_int_list(table, key, &values, &count) || count != 2) {
        return false;
    }
    *first = values[0];
    *second = values[1];
    return true;
}

bool cursor_read(const TomlTable *top, Cursor *out)
{
    const char *sheet = NULL;
    const char *alpha = NULL;
    int32_t width = 0;
    int32_t height = 0;
    int32_t tip_x = 0;
    int32_t tip_y = 0;
    if (!toml_get_text(top, "sheet", &sheet) || !is_plain_file_name(sheet)
        || !toml_get_text(top, "alpha", &alpha) || !is_plain_file_name(alpha)
        || !read_pair(top, "cell", &width, &height)
        || width < 1 || width > MOST || height < 1 || height > MOST
        || !read_pair(top, "tip", &tip_x, &tip_y)
        || tip_x < 0 || tip_x >= width || tip_y < 0 || tip_y >= height) {
        return false;
    }
    memset(out, 0, sizeof *out);
    memcpy(out->sheet, sheet, strlen(sheet));
    memcpy(out->alpha, alpha, strlen(alpha));
    out->cell_width = (uint16_t)width;
    out->cell_height = (uint16_t)height;
    out->tip_x = (uint16_t)tip_x;
    out->tip_y = (uint16_t)tip_y;
    return true;
}
