/* Writing a sprite file of the original that no registry describes as
 * a sheet of its frames, for the steps that take such sprites as they
 * are. A sprite becomes three files named after it:
 *
 *     <name>.toml        how many frames it has and how they lie
 *     <name>_sheet.png   its frames in a grid, as indices with the
 *                        sprite's palette
 *     <name>_alpha.png   for a .16a, how solid each pixel is
 */
#ifndef ROM2_ROM2_IMPORT_SPRITE_FILES_H
#define ROM2_ROM2_IMPORT_SPRITE_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/sprite256.h"

/* Writes a sprite as its files. `base` is their path without the
 * endings. The colours are those of `palette_from`, which is the sprite
 * itself but for a sprite of soft edges, which has the colours of its
 * figure. Returns false if the sprite cannot be read or a file cannot
 * be written. */
bool sprite_files_write(const Sprite256 *sprite,
    const Sprite256 *palette_from, bool is_16a, const char *base);

/* The same from the bytes of a .256 or a .16a file, for tree_write():
 * `path` is where the walk would put the file, with ".png" at its end,
 * and the files are named after it without that. An empty file writes
 * nothing and is no failure. */
bool sprite_files_write_256(const uint8_t *data, size_t size,
    const char *path);
bool sprite_files_write_16a(const uint8_t *data, size_t size,
    const char *path);

#endif
