#include "data/sprite_sheet.h"

#include <string.h>

#define MOST 4096

/* Reads a key that holds two whole numbers, each from `low` to MOST. */
static bool read_pair(const TomlTable *table, const char *key, int32_t low,
                      int32_t *first, int32_t *second)
{
    const int32_t *values = NULL;
    size_t count = 0;
    if (!toml_get_int_list(table, key, &values, &count) || count != 2
        || values[0] < low || values[0] > MOST
        || values[1] < low || values[1] > MOST) {
        return false;
    }
    *first = values[0];
    *second = values[1];
    return true;
}

/* A name that stays in the folder it is given in: no way up or down
 * from it. */
static bool is_plain_file_name(const char *name)
{
    size_t length = strlen(name);
    return length > 0 && length < SPRITE_SHEET_NAME_SIZE
        && strchr(name, '/') == NULL && strchr(name, '\\') == NULL
        && strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

bool sprite_sheet_read(const TomlTable *top, SpriteSheet *out)
{
    const TomlTable *sprite = toml_get_table(top, "sprite");
    const char *sheet = NULL;
    const char *edges = NULL;
    int32_t cell_width = 0;
    int32_t cell_height = 0;
    int32_t anchor_x = 0;
    int32_t anchor_y = 0;
    if (sprite == NULL || !toml_get_text(sprite, "sheet", &sheet)
        || !is_plain_file_name(sheet)
        || (toml_has(sprite, "edges")
            && (!toml_get_text(sprite, "edges", &edges)
                || !is_plain_file_name(edges)))
        || !read_pair(sprite, "cell", 1, &cell_width, &cell_height)
        || (toml_has(sprite, "anchor")
            && !read_pair(sprite, "anchor", -MOST, &anchor_x, &anchor_y))) {
        return false;
    }
    memset(out, 0, sizeof *out);
    memcpy(out->file, sheet, strlen(sheet));
    if (edges != NULL) {
        memcpy(out->edges, edges, strlen(edges));
    }
    out->cell_width = (uint16_t)cell_width;
    out->cell_height = (uint16_t)cell_height;
    out->anchor_x = (int16_t)anchor_x;
    out->anchor_y = (int16_t)anchor_y;
    return true;
}

bool sprite_sheet_join(const Png *figure, const Png *edges,
                       uint8_t *colours, uint8_t *solid)
{
    if (figure->kind != PNG_INDEXED
        || (edges != NULL && (edges->kind != PNG_INDEXED
            || edges->width != figure->width
            || edges->height != figure->height))) {
        return false;
    }
    size_t count = (size_t)figure->width * figure->height;
    for (size_t i = 0; i < count; i++) {
        uint8_t colour = figure->pixels[i];
        if (figure->alpha[colour] != 0) {
            colours[i] = colour;
            solid[i] = SPRITE_SHEET_SOLID;
        } else if (edges != NULL && edges->alpha[edges->pixels[i]] != 0) {
            colours[i] = edges->pixels[i];
            solid[i] = SPRITE_SHEET_EDGE;
        } else {
            colours[i] = 0;
            solid[i] = 0;
        }
    }
    return true;
}
