#include "import/unit_toml.h"

#include <stdio.h>
#include <string.h>

#include "import/name.h"
#include "import/toml.h"
#include "import/units.h"

bool unit_toml_folder_name(const Reg *reg, int32_t id,
    char out[UNIT_TOML_FOLDER_SIZE])
{
    char section[UNITS_SECTION_SIZE];
    const char *text = "";
    if (!units_section(reg, id, section)) {
        return false;
    }
    units_text(reg, id, "DescText", &text);
    return name_folder(id, text, out);
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

/* Adds what every sheet has: its file, the file of its soft edges if it
 * has them, the size of a cell, and where the unit's centre is in a
 * cell. */
static bool put_sheet(ByteBuffer *b, const char *file,
    const char *edges_file, const UnitSheet *sheet)
{
    return toml_text(b, "sheet", file)
        && (sheet->edges.pixels == NULL
            || toml_text(b, "edges", edges_file))
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
        && put_sheet(b, "sprites.png", "edges.png", sheet)
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
            && put_sheet(b, "dying.png", "dying_edges.png", dying)
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

/* A column of data.bin and the name ROM2 gives it. */
typedef struct KindColumn {
    const char *column;
    const char *key;
} KindColumn;

static const KindColumn MONSTER_COLUMNS[] = {
    {"body", "body"},
    {"reaction", "reaction"},
    {"mind", "mind"},
    {"spirit", "spirit"},
    {"healthMax", "health"},
    {"HP regeneraton", "health_regen"},
    {"manaMax", "mana"},
    {"MP regeneraton", "mana_regen"},
    {"speed", "speed"},
    {"rotationSpeed", "rotation_speed"},
    {"scanRange", "scan_range"},
    {"physicalMin", "damage_min"},
    {"physicalMax", "damage_max"},
    {"attackKind", "attack_kind"},
    {"toHit", "to_hit"},
    {"defence", "defence"},
    {"absorbtion", "absorption"},
    {"attackChargeTime", "attack_charge_time"},
    {"attackRelaxTime", "attack_relax_time"},
    {"prot Fire", "protection_fire"},
    {"prot Water", "protection_water"},
    {"prot Air", "protection_air"},
    {"prot Earth", "protection_earth"},
    {"prot Astral", "protection_astral"},
    {"res.Blade", "resist_blade"},
    {"res.Axe", "resist_axe"},
    {"res.Bludgeon", "resist_bludgeon"},
    {"res.Pike", "resist_pike"},
    {"res.Shooting", "resist_shooting"},
    {"face", "face"},
    {"tokenSize", "token_size"},
    {"movementType", "movement_type"},
    {"dyingTime", "dying_time"},
    {"Withdraw", "withdraw"},
    {"Wimpy", "wimpy"},
    {"See invisible", "see_invisible"},
    {"XPvalue", "experience"},
    {"treasure.1 Gold", "treasure_gold"},
    {"treasureMin.1", "treasure_gold_min"},
    {"treasureMax.1", "treasure_gold_max"},
    {"treasure.2 Item", "treasure_item"},
    {"treasureMin.2", "treasure_item_min"},
    {"treasureMax.2", "treasure_item_max"},
    {"treasureMask.2", "treasure_item_mask"},
    {"Power", "power"},
    {"Spell 1", "spell_1"},
    {"Probability 1", "spell_1_chance"},
    {"Spell 2", "spell_2"},
    {"Probability 2", "spell_2_chance"},
    {"Spell 3", "spell_3"},
    {"Probability 3", "spell_3_chance"},
    {"Spell Power", "spell_power"},
    {"knownSpells", "known_spells"},
    {"skillFire", "skill_fire"},
    {"skillWater", "skill_water"},
    {"skillAir", "skill_air"},
    {"skillEarth", "skill_earth"},
    {"skillAstral", "skill_astral"},
};

static const KindColumn HUMAN_COLUMNS[] = {
    {"Body", "body"},
    {"Reaction", "reaction"},
    {"Mind", "mind"},
    {"Spirit", "spirit"},
    {"HealthMax", "health"},
    {"ManaMax", "mana"},
    {"Speed", "speed"},
    {"RotationSpeed", "rotation_speed"},
    {"ScanRange", "scan_range"},
    {"Defence", "defence"},
    {"Skill.General", "skill_general"},
    {"Skill.Blade (Fire)", "skill_blade_fire"},
    {"Skill.Axe (Water)", "skill_axe_water"},
    {"Skill.Bludgen (Air)", "skill_bludgeon_air"},
    {"Skill.Pike (Earth)", "skill_pike_earth"},
    {"Skill.Shooting (Astral)", "skill_shooting_astral"},
    {"face", "face"},
    {"gender ( is female? )", "female"},
    {"AttackChargeTime", "attack_charge_time"},
    {"AttackRelaxTime", "attack_relax_time"},
    {"TokenSize", "token_size"},
    {"MovementType", "movement_type"},
    {"DyingTime", "dying_time"},
    {"knownSpells", "known_spells"},
};

/* Adds one kind. `index[i]` is where the row has the number of
 * `columns[i]`, or UINT32_MAX if the table has no such column. */
static bool put_kind(ByteBuffer *b, const DataBinRow *row, int32_t kind_id,
    const char *group, const KindColumn *columns, const uint32_t *index,
    size_t column_count)
{
    if (!toml_table_list(b, "kind") || !toml_int(b, "id", kind_id)
        || !toml_text(b, "name", row->name)
        || !toml_text(b, "group", group)) {
        return false;
    }
    for (size_t i = 0; i < column_count; i++) {
        if (index[i] < row->number_count
            && !toml_int(b, columns[i].key, row->numbers[index[i]])) {
            return false;
        }
    }

    /* The items the kind has. An empty text is a slot with nothing. */
    const char *items[DATABIN_MAX_TEXTS];
    size_t item_count = 0;
    for (uint32_t i = 0; i < row->text_count; i++) {
        if (row->texts[i][0] != '\0') {
            items[item_count++] = row->texts[i];
        }
    }
    return item_count == 0
        || toml_text_list(b, "items", items, item_count);
}

/* Adds the kinds that one table has for a unit. */
static bool put_kinds(ByteBuffer *b, const DataBin *bin, DataBinTable table,
    const char *group, const KindColumn *columns, size_t column_count,
    int32_t id, uint32_t *count)
{
    /* static: a row is too large for the stack to be at ease. */
    static DataBinRow row;
    uint32_t index[64];
    uint32_t unit_column = 0;
    uint32_t id_column = 0;
    if (column_count > sizeof index / sizeof index[0]
        || !databin_column(bin, table, "typeID", &unit_column)
        || !databin_column(bin, table, "serverID", &id_column)) {
        return false;
    }
    for (size_t i = 0; i < column_count; i++) {
        index[i] = UINT32_MAX;
        databin_column(bin, table, columns[i].column, &index[i]);
    }

    DataBinRows rows = databin_rows(bin, table);
    while (databin_next(&rows, &row)) {
        /* An empty row has no numbers at all. */
        if (unit_column >= row.number_count
            || id_column >= row.number_count
            || row.numbers[unit_column] != id) {
            continue;
        }
        if (!put_kind(b, &row, row.numbers[id_column], group, columns,
                index, column_count)) {
            return false;
        }
        (*count)++;
    }
    return true;
}

bool unit_toml_write_kinds(ByteBuffer *b, const DataBin *bin, int32_t id,
    uint32_t *out_count)
{
    uint32_t count = 0;
    if (!put_kinds(b, bin, DATABIN_MONSTERS, "monster", MONSTER_COLUMNS,
            sizeof MONSTER_COLUMNS / sizeof MONSTER_COLUMNS[0], id, &count)
        || !put_kinds(b, bin, DATABIN_HUMANS, "human", HUMAN_COLUMNS,
            sizeof HUMAN_COLUMNS / sizeof HUMAN_COLUMNS[0], id, &count)) {
        return false;
    }
    if (out_count != NULL) {
        *out_count = count;
    }
    return true;
}
