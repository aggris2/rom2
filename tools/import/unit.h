/* Everything about units: reading them from the original game's
 * units.reg, working out which frame of a sprite shows what, laying the
 * frames out as sheets, and writing unit.toml. The parts follow in that
 * order.
 *
 * The registry has one section for each unit, named "Unit0", "Unit1" and
 * so on. The number in the name is only a position. A unit is known by
 * its "ID", which is the number maps use for it.
 *
 * A unit may name a "Parent", the ID of another unit. A value the unit
 * does not state itself is then taken from its parent.
 */
#ifndef ROM2_IMPORT_UNIT_H
#define ROM2_IMPORT_UNIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/databin.h"
#include "import/name.h"
#include "import/reg.h"
#include "import/sheet.h"
#include "import/sprite256.h"
#include "import/things.h"

/* The room a section name needs: "Unit", up to ten digits and the closing
 * zero. */
#define UNITS_SECTION_SIZE 16

/* Finds the section of the unit with the given ID and writes its name,
 * such as "Unit8", to `out`.
 *
 * Returns false, and leaves `out` alone, if no unit has that ID. */
bool units_section(const Reg *r, int32_t id, char out[UNITS_SECTION_SIZE]);

/* Says whether the unit with the given ID, or one of its parents, states
 * a value called `key`, of any kind. */
bool units_has(const Reg *r, int32_t id, const char *key);

/* Reads the whole number `key` of the unit with the given ID. If the unit
 * does not state it, its parent's value is used, and so on up.
 *
 * Returns false, and leaves `*out` alone, if no unit has that ID, or
 * neither the unit nor any parent states the value. */
bool units_int(const Reg *r, int32_t id, const char *key, int32_t *out);

/* Reads the text `key` of the unit with the given ID, from the unit or
 * its parents as units_int() does. `*out` is set to point at the text
 * inside the registry's own bytes. */
bool units_text(const Reg *r, int32_t id, const char *key,
    const char **out);

/* Reads the list of whole numbers `key` of the unit with the given ID,
 * from the unit or its parents as units_int() does. `out` has room for
 * `capacity` numbers, and `*out_count` is set to how many there are.
 *
 * A list that a unit states but that does not fit `capacity` makes this
 * fail. The parent's list is not used instead. */
bool units_int_list(const Reg *r, int32_t id, const char *key,
    int32_t *out, size_t capacity, size_t *out_count);

/* The units of a registry as things, for code that works on any kind
 * of thing. */
Things units_things(const Reg *r);

/* Which frame of a unit's sprite file shows what.
 *
 * A unit's sprite file is one long row of frames. The numbers in
 * units.reg say how to cut it up:
 *
 *     the rotation frames: the unit standing, turned in 16 steps
 *     moving:    MoveBeginPhases + MovePhases frames for each direction
 *     attacking: AttackPhases frames for each direction
 *     dying:     DyingPhases frames for each direction
 *     bones:     BonePhases frames for each direction
 *     idle:      IdlePhases frames for each direction
 *
 * Within a block, all frames of direction 0 come first, then all of
 * direction 1, and so on.
 *
 * A unit with "Flip" stores only half the circle: 9 of the 16 rotation
 * steps and 5 of the 8 directions. The other half is the mirror image of
 * the stored half.
 *
 * The dying and bones frames are in the file only if the unit's "Dying"
 * is its own ID. Otherwise they are in the file of the unit that "Dying"
 * names.
 */
#define UNIT_ROTATION_STEPS 16u
#define UNIT_DIRECTIONS 8u

/* The most frames one animation may have for one direction. The original
 * game's longest is 20. */
#define UNIT_MAX_PHASES 64u

typedef enum UnitAnim {
    UNIT_ANIM_MOVE,    /* the begin frames first, then the walk cycle */
    UNIT_ANIM_ATTACK,
    UNIT_ANIM_DYING,
    UNIT_ANIM_BONES,
    UNIT_ANIM_IDLE,
    UNIT_ANIM_COUNT
} UnitAnim;

typedef struct UnitLayout {
    bool flip;                          /* half the circle is mirrored */
    uint32_t move_begin;                /* how many move frames are "begin" */
    uint32_t phases[UNIT_ANIM_COUNT];   /* frames for one direction */
    uint32_t first[UNIT_ANIM_COUNT];    /* where each block starts */
    int32_t dying_id;                   /* the unit that has the dying frames */
    bool own_dying;                     /* they are in this unit's own file */
    uint32_t frame_count;               /* frames the file must have */
} UnitLayout;

/* Works out the layout of the unit with the given ID from units.reg.
 *
 * Returns false, and leaves `*out` alone, if no unit has that ID, or one
 * of its phase counts is negative or more than UNIT_MAX_PHASES. */
bool unit_layout_read(const Reg *r, int32_t id, UnitLayout *out);

/* Says which frame shows the unit standing, turned to `step` of the
 * UNIT_ROTATION_STEPS steps, and whether that frame has to be mirrored.
 *
 * Returns false, and leaves the out-parameters alone, if `step` is out of
 * range. */
bool unit_layout_rotation(const UnitLayout *layout, uint32_t step,
    uint32_t *out_frame, bool *out_mirror);

/* Says which frame shows `phase` of an animation for `direction`, one of
 * UNIT_DIRECTIONS, and whether that frame has to be mirrored.
 *
 * Returns false, and leaves the out-parameters alone, if the direction or
 * the phase is out of range, or the frames are not in this unit's file,
 * which is so for dying and bones when `own_dying` is false. */
bool unit_layout_frame(const UnitLayout *layout, UnitAnim anim,
    uint32_t direction, uint32_t phase, uint32_t *out_frame,
    bool *out_mirror);

/* Makes a layout fit a sprite file that has more frames than the
 * registry describes, where the frames over are dying frames: the unit
 * keeps its dying frames in its own file, and the file is longer by a
 * whole number of dying phases, one frame for each stored direction. The
 * dying then gets those phases more, and the bones and what follows
 * start that much later.
 *
 * The original has one such unit, the WarSheep: its registry gives its
 * dying 4 phases and its file has 5.
 *
 * Returns true, with the layout changed, if that makes it fit
 * `file_frames`. Returns false, and leaves the layout alone, if it fits
 * already, the file is shorter, the unit's dying frames are elsewhere,
 * or the frames over are not a whole number of phases. */
bool unit_layout_fit_dying(UnitLayout *layout, uint32_t file_frames);

/* Laying a unit's frames out as one sprite sheet.
 *
 * The frames in a unit's sprite file are cut close around the figure and
 * all have different sizes. The registry gives the size of the box the
 * unit was drawn in, and each frame is the middle of that box. On the
 * sheet every frame sits in a cell of one size at the place it had in the
 * box, so the figure stands still from frame to frame.
 *
 * The cell is not the whole box, which is mostly empty, but the smallest
 * part of it that holds every frame on the sheet. It is kept the same
 * distance from the left and the right edge of the box, so that a
 * mirrored frame fits as well.
 *
 * A unit has two sheets. The main one has 9 rows:
 *
 *     row 0         the unit standing, turned in 16 steps: 16 columns
 *     rows 1 to 8   one row for each direction, and in it the frames of
 *                   moving, attacking and idle, one after the other
 *
 * The dying sheet has 8 rows, one for each direction, with the dying
 * frames and then the bones frames. It is a picture of its own because a
 * unit may take these frames from another unit's sprite file, which has
 * another palette.
 *
 * A unit that mirrors half its directions has those frames written out
 * mirrored, so that whoever draws from a sheet need not know.
 *
 * A sprite file has a partner whose name ends in "b", with as many
 * frames: the soft edge of each figure, the pixels just outside its
 * outline, which the game draws half see-through. They go on a sheet of
 * their own with the same cells as the figure's, so the two are drawn at
 * the same place: the edge first, the figure over it.
 */
/* The largest box a unit may be drawn in. The original game's largest is
 * 384 pixels. */
#define UNIT_SHEET_MAX_BOX 2048u

typedef struct UnitSheet {
    Sheet sheet;
    /* The soft edges, cell for cell. Its pixels are NULL if the sheet was
     * built without an edge sprite. */
    Sheet edges;
    /* Where the unit's centre, the point the registry calls CenterX and
     * CenterY, lies inside a cell. */
    int32_t anchor_x;
    int32_t anchor_y;
    /* The row of direction 0. The other directions follow it. It is 1 on
     * the main sheet, below the standing row, and 0 on the dying sheet. */
    uint32_t first_direction_row;
    /* The column in the direction rows where each animation starts, and
     * how many columns it takes. An animation that is not on this sheet
     * takes none. */
    uint32_t column[UNIT_ANIM_COUNT];
    uint32_t columns[UNIT_ANIM_COUNT];
} UnitSheet;

/* Builds the main sheet of a unit from its layout and its sprite file:
 * standing, moving, attacking and idle. `edges` is the sprite file with
 * the soft edges, or NULL to build without them. `box_width` and
 * `box_height` are the registry's Width and Height, `center_x` and
 * `center_y` its CenterX and CenterY. Give the sheet back with
 * unit_sheet_free().
 *
 * Returns false, and leaves `*out` alone, if a sprite does not have
 * exactly the number of frames the layout says, a frame cannot be read,
 * a frame that goes on the sheet is larger than the box, the box is
 * larger than UNIT_SHEET_MAX_BOX, or memory runs out. */
bool unit_sheet_build(const UnitLayout *layout, const Sprite256 *sprite,
    const Sprite256 *edges, uint32_t box_width, uint32_t box_height,
    int32_t center_x, int32_t center_y, UnitSheet *out);

/* Builds the dying sheet: the dying and the bones frames. The layout and
 * the sprite are those of the unit that has these frames in its own file,
 * which is the unit itself or the one its "Dying" names.
 *
 * Returns false, and leaves `*out` alone, for the reasons
 * unit_sheet_build() does, and also if the layout's dying frames are in
 * another unit's file, or it has neither dying nor bones frames. */
bool unit_sheet_build_dying(const UnitLayout *layout,
    const Sprite256 *sprite, const Sprite256 *edges, uint32_t box_width,
    uint32_t box_height, int32_t center_x, int32_t center_y, UnitSheet *out);

/* Frees a unit sheet's pixels, with those of its edges, and clears it. */
void unit_sheet_free(UnitSheet *sheet);

/* Writing a unit's definition, the file unit.toml in the unit's folder.
 *
 * The file says everything the registry knows about one unit, with the
 * values of its parent written out, so a unit's folder does not depend on
 * any other. It looks like this:
 *
 *     # Human Swordsman
 *     id = 3
 *     name = "Human Swordsman"
 *     info_picture = "SwordsMan"
 *     sounds = [100, 200, 220, 221, 240]
 *
 *     [sprite]
 *     sheet = "sprites.png"
 *     cell = [88, 72]
 *     anchor = [44, 50]
 *     palettes = "palettes.png"
 *     palette_rows = 16
 *     palette_by = "player"
 *     selection = [-16, -30, 16, 12]
 *
 *     [anim.move]
 *     column = 0
 *     columns = 10
 *     begin = 2
 *     frames = [0, 1, 2, 3, 4, 5, 6, 7]
 *     ticks = [2, 2, 2, 2, 2, 2, 2, 2]
 *
 *     [anim.attack]
 *     column = 10
 *     columns = 7
 *     frames = [0, 1, 2, 3, 4, 5, 6]
 *     ticks = [2, 2, 1, 1, 1, 2, 2]
 *     delay = 4
 *
 *     [dying]
 *     sheet = "dying.png"
 *     cell = [48, 64]
 *     anchor = [24, 46]
 *     palettes = "palettes.png"
 *     palette_rows = 16
 *     dying = 4
 *     bones = 3
 *
 * `cell` is the size of one cell of the sheet and `anchor` the place in a
 * cell of the unit's centre. Places that the registry gives inside the
 * unit's box, which are `selection` and the `shoot_x` and `shoot_y` of an
 * attack, are written counted from that centre, so they are the same
 * whatever the sheet.
 *
 * An animation takes `columns` columns of the direction rows, starting at
 * `column`. Moving starts with `begin` columns that are shown once, when
 * the unit sets off. `frames` and `ticks` are the registry's lists: which
 * frame to show, counted from the first column after the begin ones, and
 * for how long.
 *
 * On the dying sheet the `dying` columns come first and the `bones`
 * columns after them.
 *
 * `edges`, where a sheet has it, is a second picture with the same
 * cells: the soft edge of each figure, to be drawn half see-through
 * under the figure. It is "edges.png" for the main sheet and
 * "dying_edges.png" for the dying sheet.
 *
 * `palettes` is a picture 256 pixels wide in which every row is one
 * palette, one colouring of the unit, and `palette_rows` is how many
 * there are. `palette_by` says what picks the row: "player" for a human,
 * who wears the colour of the player he belongs to, and "face" for a
 * monster, whose kind has a number. The pixels of a sheet are numbers
 * from 0 to 255, and a row gives the colour of each.
 */
/* The room a folder name needs, with its closing zero. */
#define UNIT_TOML_FOLDER_SIZE NAME_FOLDER_SIZE

/* Makes the name of a unit's folder from its ID and its "DescText": the
 * ID in three digits, then the text in lower case with '_' for anything
 * that is not a letter or a digit. Unit 3, "Human Swordsman", gives
 * "003_human_swordsman". A text that is too long is cut.
 *
 * Returns false, and leaves `out` alone, if no unit has that ID or the ID
 * is not between 0 and 999. */
bool unit_toml_folder_name(const Reg *reg, int32_t id,
    char out[UNIT_TOML_FOLDER_SIZE]);

/* The palettes of a unit, as unit.toml names them. */
typedef struct UnitTomlPalettes {
    /* The picture with the palettes of the main sheet, and its rows. */
    const char *file;
    uint32_t rows;
    /* True if the player picks the row, false if the unit's face does. */
    bool by_player;
    /* The same for the dying sheet. It is another picture when the dying
     * frames come from a unit with other colours. */
    const char *dying_file;
    uint32_t dying_rows;
} UnitTomlPalettes;

/* Adds the text of unit.toml for the unit with the given ID to `b`.
 * `layout` is the unit's own layout and `sheet` its main sheet. `dying`
 * is its dying sheet, or NULL for a unit without dying frames.
 * `palettes` may be NULL, and then the file says nothing of palettes.
 *
 * Returns false if the unit has no "CenterX" and "CenterY", a frame list
 * and its tick list are not the same length, a list is longer than
 * UNIT_MAX_PHASES, or memory runs out. `b` may then hold part of the
 * text. */
bool unit_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const UnitLayout *layout, const UnitSheet *sheet,
    const UnitSheet *dying, const UnitTomlPalettes *palettes);

/* Adds the part of unit.toml that describes the pictures: the tables
 * "sprite", "anim.move" and the other animations, and "dying". This is
 * what unit_toml_write() writes after the unit's own lines, and it
 * fails where that does. It is of use alone for the pictures of a hero,
 * which are laid out as a unit's are. */
bool unit_toml_write_sprite(ByteBuffer *b, const Reg *reg, int32_t id,
    const UnitLayout *layout, const UnitSheet *sheet,
    const UnitSheet *dying, const UnitTomlPalettes *palettes);

/* Adds the kinds of the unit with the given ID to `b`, after the text
 * unit_toml_write() made. A kind is a row of data.bin's monsters or
 * humans: one set of numbers for a unit, such as a weak and a strong
 * goblin. Each is written as an entry of the list "kind":
 *
 *     [[kind]]
 *     id = 600
 *     name = "Goblin_Pike"
 *     group = "monster"
 *     body = 20
 *     health = 40
 *     face = 1
 *     items = ["Bronze Pike"]
 *
 * `id` is the number data.bin calls serverID, and `group` says which of
 * the two tables the kind is from. The other numbers are the table's
 * columns in its own order, under ROM2's names for them. `face` is the
 * row of the unit's palettes, counted from 1. A column this does not
 * know is left out, and so are the unit's ID and empty items.
 *
 * `*out_count`, if not NULL, is set to the number of kinds written.
 *
 * Returns false if a table has no "typeID" or "serverID" column, or
 * memory runs out. `b` may then hold part of the text. */
bool unit_toml_write_kinds(ByteBuffer *b, const DataBin *bin, int32_t id,
    uint32_t *out_count);

#endif
