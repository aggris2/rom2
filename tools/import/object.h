/* Everything about objects: reading them from the original game's
 * objects.reg, laying their frames out as a sheet, and writing
 * object.toml. The parts follow in that order.
 *
 * An object is a piece of scenery: a tree, a bush, a rock. The registry
 * has them as things, in the sections "Object0", "Object1" and so on,
 * with "ID" and "Parent" as things.h describes.
 *
 * An object's frames are in a sprite file, which several objects may
 * share: "Index" is the first frame that is this object's, and "Phases"
 * is how many it has. See docs/formats/objects.md.
 */
#ifndef ROM2_IMPORT_OBJECT_H
#define ROM2_IMPORT_OBJECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/name.h"
#include "import/reg.h"
#include "import/sheet.h"
#include "import/sprite256.h"
#include "import/things.h"

/* The objects of a registry, to be read with the functions of
 * things.h. */
Things objects_things(const Reg *reg);

/* Builds the archive path of an object's sprite file: the text "FileN"
 * of the section "Files", where N is the object's "File", with '\'
 * turned into '/', "objects/" in front and ".256" behind.
 *
 * Returns false, and leaves `out` alone, if the object has no "File",
 * the registry has no such text, or the path does not fit `size`. */
bool objects_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size);

/* Says which frames of its sprite file are an object's: `*out_first` is
 * the first and `*out_count` how many. `file_frames` is the number of
 * frames the file has.
 *
 * An object without "Index" starts at frame 0, and one without "Phases"
 * has one frame. An object that states more frames than the file has
 * left gets those that are there: the dead trees of the original state 7
 * and have 1.
 *
 * Returns false, and leaves the out-values alone, if no object has that
 * ID, a number is negative, or the first frame is not in the file. */
bool objects_frame_range(const Reg *reg, int32_t id, uint32_t file_frames,
    uint32_t *out_first, uint32_t *out_count);

/* Laying an object's frames out as a sprite sheet.
 *
 * An object has a few frames that play one after the other, and no
 * directions. Its sheet is one row with a cell for each frame.
 *
 * As for a unit, a frame is the middle of the box the registry gives,
 * and a cell is the smallest part of the box that holds every frame of
 * the object. The soft edges go on a second sheet with the same cells.
 * See unit.h.
 */
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

/* Writing an object's definition, the file object.toml in its folder.
 *
 * The file says everything the registry knows about one object, with the
 * values of its parent written out. It looks like this:
 *
 *     # Pine 1
 *     id = 0
 *     name = "Pine 1"
 *     in_map_editor = 1
 *     icon_id = 187
 *     dead_object = 2
 *
 *     [sprite]
 *     sheet = "sprites.png"
 *     edges = "edges.png"
 *     cell = [64, 112]
 *     anchor = [32, 100]
 *     columns = 7
 *     frames = [0, 1, 2, 3, 4, 5, 6]
 *     ticks = [4, 4, 4, 4, 4, 4, 4]
 *
 * The sheet is one row of `columns` cells. `frames` and `ticks` are the
 * registry's lists: which cell to show, and for how long, one after the
 * other and then from the start again. An object without them shows its
 * first cell.
 *
 * `dead_object` is the ID of the object this one turns into when it is
 * destroyed. `fire_object` is written as the registry has it: -2 on
 * every dead tree and -1 on every burning one. `icon_id` is the
 * registry's "IconID".
 *
 * `edges` is the picture with the soft edges, if the object has them,
 * as unit.h describes.
 */
/* The most entries an object's frame and tick lists may have. The
 * original game's longest has 35. */
#define OBJECT_TOML_MAX_STEPS 128

/* Makes the name of an object's folder from its ID and its "DescText",
 * as name_folder() does: object 0, "Pine 1", gives "000_pine_1".
 *
 * Returns false, and leaves `out` alone, if no object has that ID or the
 * ID is not between 0 and 999. */
bool object_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE]);

/* Adds the text of object.toml for the object with the given ID to `b`.
 * `sheet` is the object's sheet.
 *
 * Returns false if no object has that ID, its frame list and its tick
 * list are not the same length or are longer than OBJECT_TOML_MAX_STEPS,
 * a frame in the list is not on the sheet, or memory runs out. `b` may
 * then hold part of the text. */
bool object_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const ObjectSheet *sheet);

#endif
