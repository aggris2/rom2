/* Everything about structures: reading them from the original game's
 * structures.reg, putting their pictures together from tiles, and
 * writing structure.toml. The parts follow in that order.
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
#ifndef ROM2_IMPORT_STRUCTURE_H
#define ROM2_IMPORT_STRUCTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/databin.h"
#include "import/name.h"
#include "import/reg.h"
#include "import/sheet.h"
#include "import/sprite256.h"
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
