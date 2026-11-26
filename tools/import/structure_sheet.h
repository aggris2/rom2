/* Putting a structure's tiles together into pictures.
 *
 * The original keeps a structure as tiles, and for its animation only
 * the tiles that change. ROM2 writes whole pictures instead: a sheet
 * with one cell for each picture of the animation, each cell the size
 * of the whole structure. The ruin is a sheet of one cell. The soft
 * edges go on sheets of their own with the same cells, as for units.
 *
 * A structure of variable size, a bridge, has no picture: its sheet is
 * its loose pieces, one tile in each cell.
 */
#ifndef ROM2_IMPORT_STRUCTURE_SHEET_H
#define ROM2_IMPORT_STRUCTURE_SHEET_H

#include <stdbool.h>

#include "import/sheet.h"
#include "import/sprite256.h"
#include "import/structures.h"

typedef struct StructureSheet {
    /* The pictures of the animation, or the pieces. */
    Sheet sheet;
    Sheet edges;
    /* The ruin. Its pixels are NULL for a structure without one. */
    Sheet ruin;
    Sheet ruin_edges;
} StructureSheet;

/* Builds the sheets of a structure from its layout and its sprite file.
 * `edges` is the sprite file with the soft edges, or NULL to build
 * without them. Give the sheets back with structure_sheet_free().
 *
 * Returns false, and leaves `*out` alone, if the file does not have the
 * tiles the layout needs, the edges do not have as many tiles as the
 * sprite, a tile cannot be read or is not 32 by 32 pixels, or memory
 * runs out. */
bool structure_sheet_build(const StructureLayout *layout,
    const Sprite256 *sprite, const Sprite256 *edges, StructureSheet *out);

/* Frees the sheets and clears them. */
void structure_sheet_free(StructureSheet *sheet);

#endif
