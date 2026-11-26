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
#ifndef ROM2_IMPORT_UNIT_SHEET_H
#define ROM2_IMPORT_UNIT_SHEET_H

#include <stdbool.h>
#include <stdint.h>

#include "import/sheet.h"
#include "import/sprite256.h"
#include "import/unit_layout.h"

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
 * exactly the number of frames the layout says, a frame cannot be read or
 * is larger than the box, the box is larger than UNIT_SHEET_MAX_BOX, or
 * memory runs out. */
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

#endif
