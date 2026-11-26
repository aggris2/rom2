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
#ifndef ROM2_IMPORT_UNIT_TOML_H
#define ROM2_IMPORT_UNIT_TOML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/databin.h"
#include "import/name.h"
#include "import/reg.h"
#include "import/unit_layout.h"
#include "import/unit_sheet.h"

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
