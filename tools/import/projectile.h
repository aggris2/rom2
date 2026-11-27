/* Everything about projectiles: reading them from the original game's
 * projectiles.reg, laying their frames out as a sheet, and writing
 * projectile.toml. The parts follow in that order.
 *
 * A projectile is what flies from an attacker to its target, and what
 * shows where a spell lands: an arrow, a fire bolt, a healing sparkle.
 * The registry has them as things, in the sections "Projectile0",
 * "Projectile1" and so on.
 *
 * A projectile has "Phases" pictures of an animation and may turn to
 * face where it flies. Its sprite file holds the pictures of the first
 * direction, then those of the next, and so on. An arrow has one picture
 * in each of 16 directions. A fire bolt has 4 in each of 9, and is
 * mirrored for the other 7, as a unit is. A sparkle has no direction.
 *
 * See docs/formats/projectiles.md.
 */
#ifndef ROM2_IMPORT_PROJECTILE_H
#define ROM2_IMPORT_PROJECTILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/name.h"
#include "import/reg.h"
#include "import/sprite256.h"
#include "import/things.h"

/* The directions a projectile that turns is shown in. */
#define PROJECTILE_STEPS 16u
/* The most pictures an animation may have. */
#define PROJECTILE_MAX_PHASES 256u

/* How the frames of a projectile's sprite file are used. */
typedef struct ProjectileLayout {
    /* The pictures of the animation, in each direction. */
    uint32_t phases;
    /* The directions the file has: 1 for a projectile that does not
     * turn, 16, or 9 for one that is mirrored for the rest. */
    uint32_t stored_steps;
    /* The directions the projectile is shown in: 1 or 16. */
    uint32_t steps;
    /* The box it is drawn in. A frame is the middle of it. */
    uint32_t width;
    uint32_t height;
    /* The frames of the file that the numbers do not account for. */
    uint32_t spare_frames;
} ProjectileLayout;

/* The projectiles of a registry, to be read with the functions of
 * things.h. */
Things projectiles_things(const Reg *reg);

/* Builds the archive path of a projectile's sprite file: its "File" text
 * in lower case, with '\' turned into '/', "projectiles/" in front, and
 * ".16a" behind if the projectile says "A16", ".256" otherwise.
 * `*out_is_16a` says which.
 *
 * Returns false, and leaves the out-values alone, if the projectile has
 * no "File" or the path does not fit `size`. */
bool projectiles_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size, bool *out_is_16a);

/* Works out the layout of a projectile whose sprite file has
 * `frame_count` frames.
 *
 * The directions are "RotationPhases" if the projectile states it. If
 * not, they are 9 for one that says "Flip" and 16 for one that does not.
 * The box is "Width" by "Height", and 64 by 64 for a projectile that
 * does not state them.
 *
 * Returns false, and leaves `*out` alone, if no projectile has that ID,
 * its numbers are missing or not usable, or the file has fewer frames
 * than they need. */
bool projectiles_layout(const Reg *reg, int32_t id, uint32_t frame_count,
    ProjectileLayout *out);

/* Says which frame of the file shows the projectile in direction `step`
 * at picture `phase`, and whether it has to be mirrored left to right.
 *
 * Returns false if the layout has no such direction or picture. */
bool projectiles_frame(const ProjectileLayout *layout, uint32_t step,
    uint32_t phase, uint32_t *out_frame, bool *out_mirror);

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

/* Writing a projectile's definition, the file projectile.toml in its
 * folder. It looks like this:
 *
 *     # firebolt/sprites
 *     id = 10
 *     name = "firebolt/sprites"
 *     homing = 1
 *     sfx = 1
 *
 *     [sprite]
 *     sheet = "sprites.png"
 *     alpha = "alpha.png"
 *     palettes = "palettes.png"
 *     palette_rows = 1
 *     cell = [64, 64]
 *     anchor = [32, 32]
 *     columns = 4
 *     rows = 16
 *
 * A projectile has no name in the registry. Its `name` is its "File"
 * text, the folder and file of its sprite, with '/' between them.
 *
 * The sheet has `rows` directions, 16 or 1, and in each `columns`
 * pictures of the animation. `anchor` is the middle of a cell. The
 * registry does not say how long a picture is shown.
 *
 * `sheet` has the colour of each pixel as a number from 0 to 255, and
 * `palettes` is a picture 256 pixels wide in which each row gives the
 * colours of those numbers, as for a unit. The original game has one
 * row for each projectile. More rows are more colourings.
 *
 * `alpha` is a picture in shades of grey with the same cells, which
 * says how solid each pixel is: black for not there, white for solid.
 * A pixel is there if `alpha` says so, whatever its colour number is.
 *
 * `homing` and `sfx` are the registry's "Homing" and "SFX", as numbers.
 */
/* Makes the name of a projectile's folder from its ID and its "File"
 * text, as name_folder() does: projectile 1, "archer\arrow", gives
 * "001_archer_arrow".
 *
 * Returns false, and leaves `out` alone, if no projectile has that ID or
 * the ID is not between 0 and 999. */
bool projectile_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE]);

/* Adds the text of projectile.toml for the projectile with the given ID
 * to `b`. `sheet` is the projectile's sheet.
 *
 * Returns false if no projectile has that ID or memory runs out. `b` may
 * then hold part of the text. */
bool projectile_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const ProjectileSheet *sheet);

#endif
