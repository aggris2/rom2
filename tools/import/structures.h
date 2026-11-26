/* Reading structures from the original game's structures.reg.
 *
 * A structure is a building: a hut, a tower, a bridge. The registry has
 * them as things, in the sections "Structure0", "Structure1" and so on.
 *
 * A structure is not one picture but tiles of 32 by 32 pixels, like the
 * ground it stands on. It is "TileWidth" tiles wide and "FullHeight"
 * tiles high, and its sprite file holds, in this order:
 *
 *   - the tiles of the structure as it first looks, row after row;
 *   - for every further step of its animation, only the tiles that
 *     change, which "AnimMask" marks with a '+';
 *   - for a structure that can be destroyed, all the tiles of its ruin.
 *
 * See docs/formats/structures.md.
 */
#ifndef ROM2_IMPORT_STRUCTURES_H
#define ROM2_IMPORT_STRUCTURES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/reg.h"
#include "import/things.h"

/* The side of a tile in pixels. */
#define STRUCTURE_TILE 32u
/* The most tiles a structure's picture may have. The original game's
 * largest has 55. */
#define STRUCTURE_MAX_TILES 256u

/* How the tiles of a structure's sprite file make its pictures. */
typedef struct StructureLayout {
    uint32_t width;        /* tiles across */
    uint32_t height;       /* tiles down, the whole picture */
    /* The number of pictures of the animation. 1 for a structure that
     * stands still. */
    uint32_t phases;
    /* For each tile of the picture, row after row, whether it changes
     * from one picture of the animation to the next. */
    bool animated[STRUCTURE_MAX_TILES];
    uint32_t animated_count;
    /* True if the file ends with the tiles of the ruin. */
    bool has_ruin;
    /* True for a structure of variable size, a bridge. Its file is not a
     * picture but loose pieces, `pieces` tiles, to be put together by
     * the game. Then width, height and phases are 1. */
    bool is_pieces;
    uint32_t pieces;
} StructureLayout;

/* The structures of a registry, to be read with the functions of
 * things.h. */
Things structures_things(const Reg *reg);

/* Builds the archive path of a structure's sprite file: its "File" text
 * in lower case, with '\' turned into '/', "structures/" in front and
 * ".256" behind.
 *
 * Returns false, and leaves `out` alone, if the structure has no "File"
 * or the path does not fit `size`. */
bool structures_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size);

/* Works out the layout of a structure whose sprite file has
 * `tile_count` tiles.
 *
 * The registry's numbers say how many tiles the pictures of the
 * animation take. A file with exactly that many has no ruin, and one
 * with a whole picture more has one. A file with only the tiles of one
 * picture is a structure that stands still, whatever "Phases" says.
 *
 * Returns false, and leaves `*out` alone, if no structure has that ID,
 * its numbers are missing or not usable, its "AnimMask" is not one mark
 * for each tile, or the file's tiles fit none of the above. */
bool structures_layout(const Reg *reg, int32_t id, uint32_t tile_count,
    StructureLayout *out);

/* Says which tile of the file is at tile `index` of picture `phase` of
 * the animation. A tile that does not change is the one of the first
 * picture.
 *
 * Returns false if the layout has no such tile or picture, or is
 * pieces. */
bool structures_tile(const StructureLayout *layout, uint32_t phase,
    uint32_t index, uint32_t *out_tile);

/* Says which tile of the file is at tile `index` of the ruin.
 *
 * Returns false if the layout has no ruin or no such tile. */
bool structures_ruin_tile(const StructureLayout *layout, uint32_t index,
    uint32_t *out_tile);

#endif
