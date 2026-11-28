#include "data/font.h"

#include <string.h>

/* A name that stays in the folder it is given in. */
static bool is_plain_file_name(const char *name)
{
    size_t length = strlen(name);
    return length > 0 && length < FONT_NAME_SIZE
        && strchr(name, '/') == NULL && strchr(name, '\\') == NULL
        && strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

bool font_read(const TomlTable *top, Font *out)
{
    const char *glyphs = NULL;
    const int32_t *cell = NULL;
    const int32_t *widths = NULL;
    size_t cell_count = 0;
    size_t width_count = 0;
    bool coloured = false;
    int32_t columns = 0;
    int32_t first = 0;
    int32_t count = 0;
    if (!toml_get_text(top, "glyphs", &glyphs) || !is_plain_file_name(glyphs)
        || !toml_get_bool(top, "coloured", &coloured)
        || !toml_get_int_list(top, "cell", &cell, &cell_count)
        || cell_count != 2 || cell[0] < 1 || cell[0] > UINT8_MAX
        || cell[1] < 1 || cell[1] > UINT8_MAX
        || !toml_get_int(top, "columns", &columns) || columns < 1
        || columns > FONT_MAX_GLYPHS
        || !toml_get_int(top, "first_character", &first) || first < 0
        || !toml_get_int(top, "count", &count) || count < 1
        || count > FONT_MAX_GLYPHS || first + count > FONT_MAX_GLYPHS
        || !toml_get_int_list(top, "widths", &widths, &width_count)
        || width_count != (size_t)count) {
        return false;
    }
    for (int32_t i = 0; i < count; i++) {
        if (widths[i] < 0 || widths[i] > cell[0]) {
            return false;
        }
    }
    memset(out, 0, sizeof *out);
    memcpy(out->glyphs, glyphs, strlen(glyphs));
    out->coloured = coloured;
    out->cell_width = (uint16_t)cell[0];
    out->cell_height = (uint16_t)cell[1];
    out->columns = (uint16_t)columns;
    out->first_character = (uint16_t)first;
    out->count = (uint16_t)count;
    for (int32_t i = 0; i < count; i++) {
        out->widths[i] = (uint8_t)widths[i];
    }
    return true;
}
