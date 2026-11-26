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
#ifndef ROM2_IMPORT_PROJECTILE_TOML_H
#define ROM2_IMPORT_PROJECTILE_TOML_H

#include <stdbool.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/name.h"
#include "import/projectile_sheet.h"
#include "import/reg.h"

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
