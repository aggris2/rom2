#include "import/unit.h"

#include <stdio.h>
#include <string.h>

#include "base/toml.h"
#include "import/frames.h"
#include "import/name.h"
#include "import/things.h"

/* The units are the things of units.reg: "UnitCount" of them, in the
 * sections "Unit0", "Unit1" and so on. */
Things units_things(const Reg *r)
{
    Things t = {r, "UnitCount", "Unit"};
    return t;
}

bool units_section(const Reg *r, int32_t id, char out[UNITS_SECTION_SIZE])
{
    Things t = units_things(r);
    char name[THINGS_SECTION_SIZE];
    if (!things_section(&t, id, name)
        || strlen(name) >= UNITS_SECTION_SIZE) {
        return false;
    }
    strcpy(out, name);
    return true;
}

bool units_has(const Reg *r, int32_t id, const char *key)
{
    Things t = units_things(r);
    return things_has(&t, id, key);
}

bool units_int(const Reg *r, int32_t id, const char *key, int32_t *out)
{
    Things t = units_things(r);
    return things_int(&t, id, key, out);
}

bool units_text(const Reg *r, int32_t id, const char *key,
    const char **out)
{
    Things t = units_things(r);
    return things_text(&t, id, key, out);
}

bool units_int_list(const Reg *r, int32_t id, const char *key,
    int32_t *out, size_t capacity, size_t *out_count)
{
    Things t = units_things(r);
    return things_int_list(&t, id, key, out, capacity, out_count);
}

/* A unit that is not flipped stores every step and direction. A flipped
 * one stores the first half of the circle and both of its ends. */
#define STORED_STEPS_FLIPPED 9u
#define STORED_DIRECTIONS_FLIPPED 5u

static uint32_t stored_steps(const UnitLayout *layout)
{
    return layout->flip ? STORED_STEPS_FLIPPED : UNIT_ROTATION_STEPS;
}

static uint32_t stored_directions(const UnitLayout *layout)
{
    return layout->flip ? STORED_DIRECTIONS_FLIPPED : UNIT_DIRECTIONS;
}

/* Reads a phase count. A count the unit and its parents do not state is
 * 0. Returns false if the count is negative or too large. */
static bool read_phases(const Reg *r, int32_t id, const char *key,
    uint32_t *out)
{
    int32_t value = 0;
    if (!units_int(r, id, key, &value)) {
        *out = 0;
        return true;
    }
    if (value < 0 || (uint32_t)value > UNIT_MAX_PHASES) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

bool unit_layout_read(const Reg *r, int32_t id, UnitLayout *out)
{
    char section[UNITS_SECTION_SIZE];
    if (!units_section(r, id, section)) {
        return false;
    }

    UnitLayout layout = {0};
    int32_t flip = 0;
    if (units_int(r, id, "Flip", &flip)) {
        layout.flip = flip != 0;
    }

    uint32_t move = 0;
    if (!read_phases(r, id, "MoveBeginPhases", &layout.move_begin)
        || !read_phases(r, id, "MovePhases", &move)
        || !read_phases(r, id, "AttackPhases",
            &layout.phases[UNIT_ANIM_ATTACK])
        || !read_phases(r, id, "DyingPhases",
            &layout.phases[UNIT_ANIM_DYING])
        || !read_phases(r, id, "BonePhases",
            &layout.phases[UNIT_ANIM_BONES])
        || !read_phases(r, id, "IdlePhases",
            &layout.phases[UNIT_ANIM_IDLE])) {
        return false;
    }
    layout.phases[UNIT_ANIM_MOVE] = layout.move_begin + move;

    /* A unit that does not say whose dying frames it uses has its own. */
    layout.dying_id = id;
    units_int(r, id, "Dying", &layout.dying_id);
    layout.own_dying = layout.dying_id == id;

    /* The blocks follow each other in the order of the enum, after the
     * rotation frames. Dying and bones take no room when the frames are
     * in another unit's file. */
    uint32_t next = stored_steps(&layout);
    for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
        bool in_file = layout.own_dying
            || (anim != UNIT_ANIM_DYING && anim != UNIT_ANIM_BONES);
        layout.first[anim] = next;
        if (in_file) {
            next += layout.phases[anim] * stored_directions(&layout);
        }
    }
    layout.frame_count = next;

    *out = layout;
    return true;
}

bool unit_layout_rotation(const UnitLayout *layout, uint32_t step,
    uint32_t *out_frame, bool *out_mirror)
{
    if (step >= UNIT_ROTATION_STEPS) {
        return false;
    }
    /* A step past the last stored one is the mirror image of the step
     * just as far from the start of the circle on the other side. */
    bool mirror = step >= stored_steps(layout);
    *out_frame = mirror ? UNIT_ROTATION_STEPS - step : step;
    *out_mirror = mirror;
    return true;
}

bool unit_layout_frame(const UnitLayout *layout, UnitAnim anim,
    uint32_t direction, uint32_t phase, uint32_t *out_frame,
    bool *out_mirror)
{
    if ((int)anim < 0 || anim >= UNIT_ANIM_COUNT
        || direction >= UNIT_DIRECTIONS
        || phase >= layout->phases[anim]) {
        return false;
    }
    if (!layout->own_dying
        && (anim == UNIT_ANIM_DYING || anim == UNIT_ANIM_BONES)) {
        return false;
    }
    bool mirror = direction >= stored_directions(layout);
    uint32_t stored = mirror ? UNIT_DIRECTIONS - direction : direction;
    *out_frame = layout->first[anim] + stored * layout->phases[anim] + phase;
    *out_mirror = mirror;
    return true;
}

bool unit_layout_fit_dying(UnitLayout *layout, uint32_t file_frames)
{
    uint32_t directions = stored_directions(layout);
    if (!layout->own_dying || file_frames <= layout->frame_count
        || directions == 0) {
        return false;
    }
    uint32_t over = file_frames - layout->frame_count;
    if (over % directions != 0) {
        return false;
    }
    layout->phases[UNIT_ANIM_DYING] += over / directions;
    layout->first[UNIT_ANIM_BONES] += over;
    layout->first[UNIT_ANIM_IDLE] += over;
    layout->frame_count = file_frames;
    return true;
}

/* What a sheet is being built from. */
typedef struct Plan {
    const UnitLayout *layout;
    const Frames *frames;
    uint32_t box_width;
    uint32_t box_height;
    bool standing_row;                /* row 0 holds the 16 steps */
    bool wanted[UNIT_ANIM_COUNT];     /* which animations go on it */
    uint32_t column[UNIT_ANIM_COUNT];
    uint32_t columns[UNIT_ANIM_COUNT];
} Plan;

/* Deals with one frame of the plan. With no sheet, the window is made
 * larger so that it holds the frame. With a sheet, the frame is put into
 * its cell, at the place it has in the box. */
static bool place(const Plan *plan, uint32_t index, bool mirror,
    uint32_t column, uint32_t row, FrameWindow *window, Sheet *sheet)
{
    if (sheet == NULL) {
        return frames_measure(plan->frames, index, plan->box_width,
            plan->box_height, window);
    }
    return frames_put(plan->frames, index, mirror, plan->box_width,
        plan->box_height, window, sheet, column, row);
}

/* Goes through every frame of the plan, in the order of the sheet. */
static bool place_all(const Plan *plan, FrameWindow *window, Sheet *sheet)
{
    uint32_t first_row = plan->standing_row ? 1 : 0;
    bool ok = true;

    for (uint32_t step = 0;
        ok && plan->standing_row && step < UNIT_ROTATION_STEPS; step++) {
        uint32_t frame = 0;
        bool mirror = false;
        ok = unit_layout_rotation(plan->layout, step, &frame, &mirror)
            && place(plan, frame, mirror, step, 0, window, sheet);
    }
    for (int anim = 0; ok && anim < UNIT_ANIM_COUNT; anim++) {
        for (uint32_t d = 0; ok && d < UNIT_DIRECTIONS; d++) {
            for (uint32_t p = 0; ok && p < plan->columns[anim]; p++) {
                uint32_t frame = 0;
                bool mirror = false;
                ok = unit_layout_frame(plan->layout, (UnitAnim)anim, d, p,
                        &frame, &mirror)
                    && place(plan, frame, mirror, plan->column[anim] + p,
                        first_row + d, window, sheet);
            }
        }
    }
    return ok;
}

/* Builds a sheet with the wanted animations, and the standing row if
 * asked for. */
static bool build(const UnitLayout *layout, const Sprite256 *sprite,
    const Sprite256 *edges, uint32_t box_width, uint32_t box_height,
    int32_t center_x, int32_t center_y, const bool wanted[UNIT_ANIM_COUNT],
    bool standing_row, UnitSheet *out)
{
    if (box_width == 0 || box_width > UNIT_SHEET_MAX_BOX
        || box_height == 0 || box_height > UNIT_SHEET_MAX_BOX
        || sprite->frame_count != layout->frame_count
        || (edges != NULL && edges->frame_count != layout->frame_count)) {
        return false;
    }

    Frames frames;
    Frames edge_frames = {NULL, 0};
    if (!frames_read(sprite, &frames)) {
        return false;
    }
    if (edges != NULL && !frames_read(edges, &edge_frames)) {
        frames_free(&frames);
        return false;
    }

    /* The wanted animations side by side in the direction rows. One that
     * the unit does not have, or whose frames are in another unit's
     * file, takes no columns. */
    Plan plan;
    memset(&plan, 0, sizeof plan);
    plan.layout = layout;
    plan.frames = &frames;
    plan.box_width = box_width;
    plan.box_height = box_height;
    plan.standing_row = standing_row;
    uint32_t columns = 0;
    for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
        uint32_t frame = 0;
        bool mirror = false;
        bool on_sheet = wanted[anim] && unit_layout_frame(layout,
            (UnitAnim)anim, 0, 0, &frame, &mirror);
        plan.column[anim] = columns;
        plan.columns[anim] = on_sheet ? layout->phases[anim] : 0;
        columns += plan.columns[anim];
    }
    if (standing_row && columns < UNIT_ROTATION_STEPS) {
        columns = UNIT_ROTATION_STEPS;
    }
    uint32_t rows = UNIT_DIRECTIONS + (standing_row ? 1 : 0);

    /* The same plan for the edge frames, which go on a sheet of their
     * own with the same cells. */
    Plan edge_plan = plan;
    edge_plan.frames = &edge_frames;

    /* First find the window that holds every frame, of the figure and of
     * its edge, then fill sheets of that cell size. */
    FrameWindow window = frame_window_empty(box_width, box_height);
    bool ok = columns > 0 && place_all(&plan, &window, NULL)
        && (edges == NULL || place_all(&edge_plan, &window, NULL));
    frame_window_close(&window, box_width, box_height);

    UnitSheet result;
    memset(&result, 0, sizeof result);
    ok = ok && sheet_create(&result.sheet, window.right - window.left,
        window.bottom - window.top, columns, rows);
    ok = ok && place_all(&plan, &window, &result.sheet);
    if (ok && edges != NULL) {
        ok = sheet_create(&result.edges, window.right - window.left,
                window.bottom - window.top, columns, rows)
            && place_all(&edge_plan, &window, &result.edges);
    }
    frames_free(&frames);
    frames_free(&edge_frames);
    if (!ok) {
        sheet_free(&result.sheet);
        sheet_free(&result.edges);
        return false;
    }

    result.anchor_x = center_x - (int32_t)window.left;
    result.anchor_y = center_y - (int32_t)window.top;
    result.first_direction_row = standing_row ? 1 : 0;
    memcpy(result.column, plan.column, sizeof result.column);
    memcpy(result.columns, plan.columns, sizeof result.columns);
    *out = result;
    return true;
}

bool unit_sheet_build(const UnitLayout *layout, const Sprite256 *sprite,
    const Sprite256 *edges, uint32_t box_width, uint32_t box_height,
    int32_t center_x, int32_t center_y, UnitSheet *out)
{
    bool wanted[UNIT_ANIM_COUNT] = {false};
    wanted[UNIT_ANIM_MOVE] = true;
    wanted[UNIT_ANIM_ATTACK] = true;
    wanted[UNIT_ANIM_IDLE] = true;
    return build(layout, sprite, edges, box_width, box_height, center_x,
        center_y, wanted, true, out);
}

bool unit_sheet_build_dying(const UnitLayout *layout,
    const Sprite256 *sprite, const Sprite256 *edges, uint32_t box_width,
    uint32_t box_height, int32_t center_x, int32_t center_y, UnitSheet *out)
{
    bool wanted[UNIT_ANIM_COUNT] = {false};
    wanted[UNIT_ANIM_DYING] = true;
    wanted[UNIT_ANIM_BONES] = true;
    /* A layout whose dying frames are in another file has no columns to
     * give, and build() refuses a sheet with none. */
    return build(layout, sprite, edges, box_width, box_height, center_x,
        center_y, wanted, false, out);
}

void unit_sheet_free(UnitSheet *sheet)
{
    sheet_free(&sheet->sheet);
    sheet_free(&sheet->edges);
    memset(sheet, 0, sizeof *sheet);
}

bool unit_toml_folder_name(const Reg *reg, int32_t id,
    char out[UNIT_TOML_FOLDER_SIZE])
{
    Things t = units_things(reg);
    return thing_toml_folder_name(&t, id, "DescText", out);
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
    Things t = units_things(reg);
    char key[32];
    int32_t frames[UNIT_MAX_PHASES];
    int32_t ticks[UNIT_MAX_PHASES];
    size_t frame_count = 0;
    size_t tick_count = 0;
    snprintf(key, sizeof key, "%sAnimFrame", prefix);
    if (!thing_toml_list(&t, id, key, frames, UNIT_MAX_PHASES,
            &frame_count)) {
        return false;
    }
    snprintf(key, sizeof key, "%sAnimTime", prefix);
    if (!thing_toml_list(&t, id, key, ticks, UNIT_MAX_PHASES,
            &tick_count)
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
    Things t = units_things(reg);
    int32_t pairs[UNIT_MAX_PHASES];
    int32_t x[UNIT_DIRECTIONS];
    int32_t y[UNIT_DIRECTIONS];
    size_t count = 0;
    if (!thing_toml_list(&t, id, "ShootOffset", pairs, UNIT_MAX_PHASES,
            &count)) {
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
    Things t = units_things(reg);
    int32_t center_x = 0;
    int32_t center_y = 0;
    int32_t sounds[UNIT_MAX_PHASES];
    size_t sound_count = 0;
    const char *name = "";
    if (!units_int(reg, id, "CenterX", &center_x)
        || !units_int(reg, id, "CenterY", &center_y)
        || !thing_toml_list(&t, id, "Sound", sounds, UNIT_MAX_PHASES,
            &sound_count)) {
        return false;
    }
    units_text(reg, id, "DescText", &name);

    bool ok = toml_comment(b, name)
        && toml_int(b, "id", id)
        && toml_text(b, "name", name)
        && thing_toml_text(b, &t, id, "InfoPicture", "info_picture", true)
        && thing_toml_int(b, &t, id, "InMapEditor", "in_map_editor")
        && thing_toml_int(b, &t, id, "TileSize", "tile_size")
        && thing_toml_int(b, &t, id, "Z", "z")
        && (sound_count == 0
            || toml_int_list(b, "sounds", sounds, sound_count));
    return ok && unit_toml_write_sprite(b, reg, id, layout, sheet, dying,
        palettes);
}

bool unit_toml_write_sprite(ByteBuffer *b, const Reg *reg, int32_t id,
    const UnitLayout *layout, const UnitSheet *sheet,
    const UnitSheet *dying, const UnitTomlPalettes *palettes)
{
    Things t = units_things(reg);
    int32_t center_x = 0;
    int32_t center_y = 0;
    if (!units_int(reg, id, "CenterX", &center_x)
        || !units_int(reg, id, "CenterY", &center_y)) {
        return false;
    }

    bool ok = toml_table(b, "sprite")
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
            && thing_toml_int(b, &t, id, "AttackDelay", "delay")
            && thing_toml_int(b, &t, id, "ShootDelay", "shoot_delay")
            && thing_toml_int(b, &t, id, "Projectile", "projectile")
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
