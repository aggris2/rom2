/* Writing an object's definition, the file object.toml in its folder.
 *
 * The file says everything the registry knows about one object, with the
 * values of its parent written out. It looks like this:
 *
 *     # Pine 1
 *     id = 0
 *     name = "Pine 1"
 *     in_map_editor = 1
 *     icon_id = 187
 *     dead_object = 2
 *
 *     [sprite]
 *     sheet = "sprites.png"
 *     edges = "edges.png"
 *     cell = [64, 112]
 *     anchor = [32, 100]
 *     columns = 7
 *     frames = [0, 1, 2, 3, 4, 5, 6]
 *     ticks = [4, 4, 4, 4, 4, 4, 4]
 *
 * The sheet is one row of `columns` cells. `frames` and `ticks` are the
 * registry's lists: which cell to show, and for how long, one after the
 * other and then from the start again. An object without them shows its
 * first cell.
 *
 * `dead_object` is the ID of the object this one turns into when it is
 * destroyed. `fire_object` is written as the registry has it: -2 on
 * every dead tree and -1 on every burning one. `icon_id` is the
 * registry's "IconID".
 *
 * `edges` is the picture with the soft edges, if the object has them,
 * as unit_toml.h describes.
 */
#ifndef ROM2_IMPORT_OBJECT_TOML_H
#define ROM2_IMPORT_OBJECT_TOML_H

#include <stdbool.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/name.h"
#include "import/object_sheet.h"
#include "import/reg.h"

/* The most entries an object's frame and tick lists may have. The
 * original game's longest has 35. */
#define OBJECT_TOML_MAX_STEPS 128

/* Makes the name of an object's folder from its ID and its "DescText",
 * as name_folder() does: object 0, "Pine 1", gives "000_pine_1".
 *
 * Returns false, and leaves `out` alone, if no object has that ID or the
 * ID is not between 0 and 999. */
bool object_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE]);

/* Adds the text of object.toml for the object with the given ID to `b`.
 * `sheet` is the object's sheet.
 *
 * Returns false if no object has that ID, its frame list and its tick
 * list are not the same length or are longer than OBJECT_TOML_MAX_STEPS,
 * a frame in the list is not on the sheet, or memory runs out. `b` may
 * then hold part of the text. */
bool object_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const ObjectSheet *sheet);

#endif
