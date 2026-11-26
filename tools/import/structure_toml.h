/* Writing a structure's definition, the file structure.toml in its
 * folder.
 *
 * The file has what the registry says of one structure and, with it,
 * the numbers data.bin has for it. It looks like this:
 *
 *     # Windmill 1
 *     id = 5
 *     name = "Windmill 1"
 *     picture = "windmill"
 *     icon_id = 230
 *     size = [3, 3]
 *     height = 3
 *     selection = [14, 6, 80, 83]
 *     shadow_y = 40
 *     scan_range = 4
 *     health = 1000
 *     passability = 186
 *     building_present = 511
 *
 *     [sprite]
 *     sheet = "sprites.png"
 *     edges = "edges.png"
 *     cell = [96, 96]
 *     columns = 6
 *     frames = [0, 1, 2, 3, 4, 5]
 *     ticks = [2, 2, 2, 2, 2, 2]
 *     ruin = "ruin.png"
 *     ruin_edges = "ruin_edges.png"
 *
 * `size` is the tiles the structure stands on, across and down, and
 * `height` how many tiles high its picture is, which may be more: the
 * picture's lowest rows are the ones it stands on. A cell of the sheet
 * is the whole picture, 32 pixels for each tile. `selection` is the box
 * to click on, in pixels from the picture's top left corner.
 *
 * The sheet is one row of `columns` pictures. `frames` and `ticks` are
 * the registry's lists: which picture to show, and for how long. `ruin`
 * is the picture of the structure destroyed, for those that have one.
 *
 * A structure of variable size, a bridge, says `variable_size = 1`. Its
 * sheet is not pictures but `columns` loose pieces of one tile each.
 *
 * `scan_range`, `health`, `passability` and `building_present` are
 * data.bin's columns scanRange, healthMax, Passability and
 * BuildingPresent, as numbers. The other keys are the registry's, under
 * ROM2's names.
 */
#ifndef ROM2_IMPORT_STRUCTURE_TOML_H
#define ROM2_IMPORT_STRUCTURE_TOML_H

#include <stdbool.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/databin.h"
#include "import/name.h"
#include "import/reg.h"
#include "import/structure_sheet.h"
#include "import/structures.h"

/* The most entries a structure's frame and tick lists may have. */
#define STRUCTURE_TOML_MAX_STEPS 128

/* Makes the name of a structure's folder from its ID and its
 * "DescText", as name_folder() does: structure 5, "Windmill 1", gives
 * "005_windmill_1".
 *
 * Returns false, and leaves `out` alone, if no structure has that ID or
 * the ID is not between 0 and 999. */
bool structure_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE]);

/* Adds the text of structure.toml for the structure with the given ID
 * to `b`. `layout` and `sheet` are the structure's. `bin` is the opened
 * data.bin, or NULL to leave its numbers out: the structure with ID n
 * is row n of its buildings, counted from 1.
 *
 * Returns false if no structure has that ID, its frame list and its
 * tick list are not the same length or are longer than
 * STRUCTURE_TOML_MAX_STEPS, a frame in the list is not on the sheet, or
 * memory runs out. `b` may then hold part of the text. */
bool structure_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const StructureLayout *layout, const StructureSheet *sheet,
    const DataBin *bin);

#endif
