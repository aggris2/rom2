/* Laying a projectile's frames out as a sprite sheet.
 *
 * The sheet has a row for each direction the projectile is shown in, 16
 * or 1, and in each row the pictures of its animation. Mirrored
 * directions are written out. A cell is the whole box the projectile is
 * drawn in, with each frame in its middle.
 *
 * Most projectiles are fire and light, whose pixels are solid to
 * different degrees. So the sheet is two pictures with the same cells:
 * the colour of each pixel, as an index into the palette, and how solid
 * each pixel is, from 0 for not there to 255 for solid. Keeping the
 * colours as indices lets a projectile be recoloured by another palette,
 * as a unit is.
 *
 * An arrow or a catapult's stone, from a .256 file, has a solid figure
 * and a soft edge. Both go on the one sheet: the edge is its colours
 * with a solidness of about half.
 */
#ifndef ROM2_IMPORT_PROJECTILE_SHEET_H
#define ROM2_IMPORT_PROJECTILE_SHEET_H

#include <stdbool.h>
#include <stdint.h>

#include "import/projectiles.h"
#include "import/sprite256.h"

/* The largest box a projectile may be drawn in. The original game's
 * largest is 128. */
#define PROJECTILE_SHEET_MAX_BOX 1024u
/* How solid the soft edge of a .256 projectile is, of 255. */
#define PROJECTILE_SHEET_EDGE 128u

typedef struct ProjectileSheet {
    uint32_t cell_width;
    uint32_t cell_height;
    uint32_t columns;   /* the pictures of the animation */
    uint32_t rows;      /* the directions */
    /* Two blocks from malloc() of `cell_width * columns` by
     * `cell_height * rows` bytes, row after row from the top: the colour
     * of each pixel as an index into the palette, and how solid it is.
     * The colour of a pixel that is not there is 0. */
    uint8_t *pixels;
    uint8_t *solid;
} ProjectileSheet;

/* Builds the sheet of a projectile whose sprite file is a .16a. A pixel
 * of the file is as solid on the sheet as the file says: its 15 levels
 * are spread over 17 to 255.
 *
 * Returns false, and leaves `*out` alone, if the file has fewer frames
 * than the layout needs, a frame cannot be read or is larger than the
 * box, the box is larger than PROJECTILE_SHEET_MAX_BOX, or memory runs
 * out. */
bool projectile_sheet_build_16a(const ProjectileLayout *layout,
    const Sprite256 *sprite, ProjectileSheet *out);

/* Builds the sheet of a projectile whose sprite file is a .256. Its
 * pixels are solid, and its index 0 is no pixel. `edges` is the sprite
 * file with the soft edges, or NULL: its pixels are PROJECTILE_SHEET_EDGE
 * solid, wherever the figure has none.
 *
 * Returns false, and leaves `*out` alone, for the reasons above, and if
 * the edges do not have as many frames as the sprite. */
bool projectile_sheet_build_256(const ProjectileLayout *layout,
    const Sprite256 *sprite, const Sprite256 *edges, ProjectileSheet *out);

/* The width and the height of the whole sheet in pixels. */
uint32_t projectile_sheet_width(const ProjectileSheet *sheet);
uint32_t projectile_sheet_height(const ProjectileSheet *sheet);

/* Frees a sheet's two blocks and clears it. */
void projectile_sheet_free(ProjectileSheet *sheet);

#endif
