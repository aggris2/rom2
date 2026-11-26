/* Laying an object's frames out as a sprite sheet.
 *
 * An object has a few frames that play one after the other, and no
 * directions. Its sheet is one row with a cell for each frame.
 *
 * As for a unit, a frame is the middle of the box the registry gives,
 * and a cell is the smallest part of the box that holds every frame of
 * the object. The soft edges go on a second sheet with the same cells.
 * See unit_sheet.h.
 */
#ifndef ROM2_IMPORT_OBJECT_SHEET_H
#define ROM2_IMPORT_OBJECT_SHEET_H

#include <stdbool.h>
#include <stdint.h>

#include "import/sheet.h"
#include "import/sprite256.h"

/* The largest box an object may be drawn in. */
#define OBJECT_SHEET_MAX_BOX 2048u

typedef struct ObjectSheet {
    Sheet sheet;
    /* The soft edges, cell for cell. Its pixels are NULL if the sheet was
     * built without an edge sprite. */
    Sheet edges;
    /* Where the object's centre, the registry's CenterX and CenterY,
     * lies inside a cell. */
    int32_t anchor_x;
    int32_t anchor_y;
} ObjectSheet;

/* Builds the sheet of an object from `count` frames of a sprite file,
 * starting at frame `first`. `edges` is the sprite file with the soft
 * edges, or NULL to build without them. `box_width` and `box_height` are
 * the registry's Width and Height, `center_x` and `center_y` its CenterX
 * and CenterY. Give the sheet back with object_sheet_free().
 *
 * Returns false, and leaves `*out` alone, if `count` is 0, the frames
 * are not all in the file, the edges do not have as many frames as the
 * sprite, a frame cannot be read, one of the object's frames is larger
 * than the box, the box is larger than OBJECT_SHEET_MAX_BOX, or memory
 * runs out. */
bool object_sheet_build(const Sprite256 *sprite, const Sprite256 *edges,
    uint32_t first, uint32_t count, uint32_t box_width,
    uint32_t box_height, int32_t center_x, int32_t center_y,
    ObjectSheet *out);

/* Frees an object sheet's pixels, with those of its edges, and clears
 * it. */
void object_sheet_free(ObjectSheet *sheet);

#endif
