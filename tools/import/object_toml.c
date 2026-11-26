#include "import/object_toml.h"

#include "import/objects.h"
#include "import/toml.h"

bool object_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE])
{
    Things t = objects_things(reg);
    char section[THINGS_SECTION_SIZE];
    const char *text = "";
    if (!things_section(&t, id, section)) {
        return false;
    }
    things_text(&t, id, "DescText", &text);
    return name_folder(id, text, out);
}

/* Adds a whole number of the object under another name, if the object
 * or a parent states it. */
static bool copy_int(ByteBuffer *b, const Things *t, int32_t id,
    const char *key, const char *name)
{
    int32_t value = 0;
    return !things_int(t, id, key, &value) || toml_int(b, name, value);
}

/* Reads a list of the object. A list that nobody states has `*count` 0.
 * Returns false for a list that is stated but cannot be read. */
static bool read_list(const Things *t, int32_t id, const char *key,
    int32_t out[OBJECT_TOML_MAX_STEPS], size_t *count)
{
    *count = 0;
    return things_int_list(t, id, key, out, OBJECT_TOML_MAX_STEPS, count)
        || !things_has(t, id, key);
}

bool object_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const ObjectSheet *sheet)
{
    Things t = objects_things(reg);
    char section[THINGS_SECTION_SIZE];
    int32_t frames[OBJECT_TOML_MAX_STEPS];
    int32_t ticks[OBJECT_TOML_MAX_STEPS];
    size_t frame_count = 0;
    size_t tick_count = 0;
    const char *name = "";
    if (!things_section(&t, id, section)
        || !read_list(&t, id, "AnimationFrame", frames, &frame_count)
        || !read_list(&t, id, "AnimationTime", ticks, &tick_count)
        || frame_count != tick_count) {
        return false;
    }
    for (size_t i = 0; i < frame_count; i++) {
        if (frames[i] < 0 || (uint32_t)frames[i] >= sheet->sheet.columns) {
            return false;
        }
    }
    things_text(&t, id, "DescText", &name);

    /* An object that is never destroyed says -1, which is left out. */
    int32_t dead = -1;
    things_int(&t, id, "DeadObject", &dead);

    const int32_t cell[2] = {
        (int32_t)sheet->sheet.cell_width, (int32_t)sheet->sheet.cell_height
    };
    const int32_t anchor[2] = {sheet->anchor_x, sheet->anchor_y};

    return toml_comment(b, name)
        && toml_int(b, "id", id)
        && toml_text(b, "name", name)
        && copy_int(b, &t, id, "InMapEditor", "in_map_editor")
        && copy_int(b, &t, id, "IconID", "icon_id")
        && (dead < 0 || toml_int(b, "dead_object", dead))
        && copy_int(b, &t, id, "FireObject", "fire_object")
        && toml_table(b, "sprite")
        && toml_text(b, "sheet", "sprites.png")
        && (sheet->edges.pixels == NULL
            || toml_text(b, "edges", "edges.png"))
        && toml_int_list(b, "cell", cell, 2)
        && toml_int_list(b, "anchor", anchor, 2)
        && toml_int(b, "columns", (int32_t)sheet->sheet.columns)
        && (frame_count == 0
            || (toml_int_list(b, "frames", frames, frame_count)
                && toml_int_list(b, "ticks", ticks, tick_count)));
}
