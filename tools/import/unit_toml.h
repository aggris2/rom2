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
 */
#ifndef ROM2_IMPORT_UNIT_TOML_H
#define ROM2_IMPORT_UNIT_TOML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/reg.h"
#include "import/unit_layout.h"
#include "import/unit_sheet.h"

/* The room a folder name needs, with its closing zero. */
#define UNIT_TOML_FOLDER_SIZE 64

/* Makes the name of a unit's folder from its ID and its "DescText": the
 * ID in three digits, then the text in lower case with '_' for anything
 * that is not a letter or a digit. Unit 3, "Human Swordsman", gives
 * "003_human_swordsman". A text that is too long is cut.
 *
 * Returns false, and leaves `out` alone, if no unit has that ID or the ID
 * is not between 0 and 999. */
bool unit_toml_folder_name(const Reg *reg, int32_t id,
    char out[UNIT_TOML_FOLDER_SIZE]);

/* Adds the text of unit.toml for the unit with the given ID to `b`.
 * `layout` is the unit's own layout and `sheet` its main sheet. `dying`
 * is its dying sheet, or NULL for a unit without dying frames.
 *
 * Returns false if the unit has no "CenterX" and "CenterY", a frame list
 * and its tick list are not the same length, a list is longer than
 * UNIT_MAX_PHASES, or memory runs out. `b` may then hold part of the
 * text. */
bool unit_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const UnitLayout *layout, const UnitSheet *sheet,
    const UnitSheet *dying);

#endif
