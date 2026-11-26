#include "import/unit_toml.h"

#include <stdio.h>
#include <string.h>

#include "import/toml.h"
#include "import/units.h"

bool unit_toml_folder_name(const Reg *reg, int32_t id,
    char out[UNIT_TOML_FOLDER_SIZE])
{
    char section[UNITS_SECTION_SIZE];
    if (id < 0 || id > 999 || !units_section(reg, id, section)) {
        return false;
    }
    const char *text = "";
    units_text(reg, id, "DescText", &text);

    char name[UNIT_TOML_FOLDER_SIZE];
    size_t length = (size_t)snprintf(name, sizeof name, "%03d", (int)id);
    /* A '_' is owed before the next letter or digit: after the ID, and
     * after any run of other characters. */
    bool gap = true;
    for (const char *p = text; *p != '\0'; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
            gap = true;
            continue;
        }
        if (length + (gap ? 2u : 1u) >= sizeof name) {
            break;
        }
        if (gap) {
            name[length++] = '_';
            gap = false;
        }
        name[length++] = c;
    }
    name[length] = '\0';
    memcpy(out, name, length + 1);
    return true;
}

/* Adds a whole number of the unit under another name, if the unit or a
 * parent states it. */
static bool copy_int(ByteBuffer *b, const Reg *reg, int32_t id,
    const char *key, const char *name)
{
    int32_t value = 0;
    return !units_int(reg, id, key, &value) || toml_int(b, name, value);
}

static bool copy_text(ByteBuffer *b, const Reg *reg, int32_t id,
    const char *key, const char *name)
{
    const char *text = NULL;
    return !units_text(reg, id, key, &text) || toml_text(b, name, text);
}

/* Reads a list of the unit. A list that nobody states has `*count` 0.
 * Returns false for a list that is stated but cannot be read, which is
 * one longer than UNIT_MAX_PHASES. */
static bool read_list(const Reg *reg, int32_t id, const char *key,
    int32_t out[UNIT_MAX_PHASES], size_t *count)
{
    *count = 0;
    return units_int_list(reg, id, key, out, UNIT_MAX_PHASES, count)
        || !units_has(reg, id, key);
}

/* Adds a pair of numbers as a list: "key = [first, second]". */
static bool put_pair(ByteBuffer *b, const char *key, int32_t first,
    int32_t second)
{
    const int32_t pair[2] = {first, second};
    return toml_int_list(b, key, pair, 2);
}

/* Adds what every sheet has: its file, the size of a cell, and where the
 * unit's centre is in a cell. */
static bool put_sheet(ByteBuffer *b, const char *file,
    const UnitSheet *sheet)
{
    return toml_text(b, "sheet", file)
        && put_pair(b, "cell", (int32_t)sheet->sheet.cell_width,
            (int32_t)sheet->sheet.cell_height)
        && put_pair(b, "anchor", sheet->anchor_x, sheet->anchor_y);
}

/* Adds the picture a sheet's palettes are in, and how many there are. */
static bool put_palettes(ByteBuffer *b, const char *file, uint32_t rows)
{
    return toml_text(b, "palettes", file)
        && toml_int(b, "palette_rows", (int32_t)rows);
}

/* Adds the box to click on, counted from the unit's centre. */
static bool put_selection(ByteBuffer *b, const Reg *reg, int32_t id,
    int32_t center_x, int32_t center_y)
{
    int32_t box[4];
    if (!units_int(reg, id, "SelectionX1", &box[0])
        || !units_int(reg, id, "SelectionY1", &box[1])
        || !units_int(reg, id, "SelectionX2", &box[2])
        || !units_int(reg, id, "SelectionY2", &box[3])) {
        return true;
    }
    box[0] -= center_x;
    box[1] -= center_y;
    box[2] -= center_x;
    box[3] -= center_y;
    return toml_int_list(b, "selection", box, 4);
}

/* Adds the header of an animation, where it is on the sheet, and its
 * frame and tick lists. `prefix` is the start of the registry's names
 * for the lists: "Move" for "MoveAnimFrame" and "MoveAnimTime". */
static bool put_anim(ByteBuffer *b, const Reg *reg, int32_t id,
    const UnitLayout *layout, const UnitSheet *sheet, UnitAnim anim,
    const char *table, const char *prefix)
{
    char key[32];
    int32_t frames[UNIT_MAX_PHASES];
    int32_t ticks[UNIT_MAX_PHASES];
    size_t frame_count = 0;
    size_t tick_count = 0;
    snprintf(key, sizeof key, "%sAnimFrame", prefix);
    if (!read_list(reg, id, key, frames, &frame_count)) {
        return false;
    }
    snprintf(key, sizeof key, "%sAnimTime", prefix);
    if (!read_list(reg, id, key, ticks, &tick_count)
        || frame_count != tick_count) {
        return false;
    }
    if (!toml_table(b, table)
        || !toml_int(b, "column", (int32_t)sheet->column[anim])
        || !toml_int(b, "columns", (int32_t)sheet->columns[anim])) {
        return false;
    }
    if (anim == UNIT_ANIM_MOVE
        && !toml_int(b, "begin", (int32_t)layout->move_begin)) {
        return false;
    }
    return frame_count == 0
        || (toml_int_list(b, "frames", frames, frame_count)
            && toml_int_list(b, "ticks", ticks, tick_count));
}

/* Adds where a shot leaves the unit in each of the 8 directions, counted
 * from the unit's centre. The registry has them as 8 pairs of x and y
 * inside the unit's box. */
static bool put_shoot_offsets(ByteBuffer *b, const Reg *reg, int32_t id,
    int32_t center_x, int32_t center_y)
{
    int32_t pairs[UNIT_MAX_PHASES];
    int32_t x[UNIT_DIRECTIONS];
    int32_t y[UNIT_DIRECTIONS];
    size_t count = 0;
    if (!read_list(reg, id, "ShootOffset", pairs, &count)) {
        return false;
    }
    if (count == 0) {
        return true;
    }
    if (count != 2 * UNIT_DIRECTIONS) {
        return false;
    }
    for (size_t d = 0; d < UNIT_DIRECTIONS; d++) {
        x[d] = pairs[2 * d] - center_x;
        y[d] = pairs[2 * d + 1] - center_y;
    }
    return toml_int_list(b, "shoot_x", x, UNIT_DIRECTIONS)
        && toml_int_list(b, "shoot_y", y, UNIT_DIRECTIONS);
}

bool unit_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const UnitLayout *layout, const UnitSheet *sheet,
    const UnitSheet *dying, const UnitTomlPalettes *palettes)
{
    int32_t center_x = 0;
    int32_t center_y = 0;
    int32_t sounds[UNIT_MAX_PHASES];
    size_t sound_count = 0;
    const char *name = "";
    if (!units_int(reg, id, "CenterX", &center_x)
        || !units_int(reg, id, "CenterY", &center_y)
        || !read_list(reg, id, "Sound", sounds, &sound_count)) {
        return false;
    }
    units_text(reg, id, "DescText", &name);

    bool ok = toml_comment(b, name)
        && toml_int(b, "id", id)
        && toml_text(b, "name", name)
        && copy_text(b, reg, id, "InfoPicture", "info_picture")
        && copy_int(b, reg, id, "InMapEditor", "in_map_editor")
        && copy_int(b, reg, id, "TileSize", "tile_size")
        && copy_int(b, reg, id, "Z", "z")
        && (sound_count == 0
            || toml_int_list(b, "sounds", sounds, sound_count));

    ok = ok && toml_table(b, "sprite")
        && put_sheet(b, "sprites.png", sheet)
        && (palettes == NULL
            || (put_palettes(b, palettes->file, palettes->rows)
                && toml_text(b, "palette_by",
                    palettes->by_player ? "player" : "face")))
        && put_selection(b, reg, id, center_x, center_y);

    if (ok && sheet->columns[UNIT_ANIM_MOVE] > 0) {
        ok = put_anim(b, reg, id, layout, sheet, UNIT_ANIM_MOVE,
            "anim.move", "Move");
    }
    if (ok && sheet->columns[UNIT_ANIM_ATTACK] > 0) {
        ok = put_anim(b, reg, id, layout, sheet, UNIT_ANIM_ATTACK,
                "anim.attack", "Attack")
            && copy_int(b, reg, id, "AttackDelay", "delay")
            && copy_int(b, reg, id, "ShootDelay", "shoot_delay")
            && copy_int(b, reg, id, "Projectile", "projectile")
            && put_shoot_offsets(b, reg, id, center_x, center_y);
    }
    if (ok && sheet->columns[UNIT_ANIM_IDLE] > 0) {
        ok = put_anim(b, reg, id, layout, sheet, UNIT_ANIM_IDLE,
            "anim.idle", "Idle");
    }

    if (ok && dying != NULL) {
        ok = toml_table(b, "dying")
            && put_sheet(b, "dying.png", dying)
            && (palettes == NULL
                || put_palettes(b, palettes->dying_file,
                    palettes->dying_rows))
            && toml_int(b, "dying",
                (int32_t)dying->columns[UNIT_ANIM_DYING])
            && toml_int(b, "bones",
                (int32_t)dying->columns[UNIT_ANIM_BONES]);
    }
    return ok;
}
