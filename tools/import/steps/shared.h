/* What the steps of import share: opening the original's tables and
 * making a step's folder, going through the things of a registry,
 * writing a whole folder of an archive file by file, and writing a
 * sprite no registry describes as a sheet. The parts follow in that
 * order.
 */
#ifndef ROM2_IMPORT_STEPS_SHARED_H
#define ROM2_IMPORT_STEPS_SHARED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"
#include "base/rgb.h"
#include "import/databin.h"
#include "import/reg.h"
#include "import/res.h"
#include "import/sheet.h"
#include "import/sprite256.h"
#include "import/things.h"

/* What the steps of import have in common: opening the original's
 * tables, making a step's folder, writing a file into it, and going
 * through the things of a registry.
 */
/* Opens the registry at `path` in an archive. Says so on stderr, in the
 * name of `step`, and returns false if it is not there or not one. */
bool step_reg_open(const char *step, const ResArchive *archive,
    const char *path, Reg *out);

/* Opens data/data.bin in world.res, in the same way. */
bool step_databin_open(const char *step, const ResArchive *world,
    DataBin *out);

/* Opens the sprite file at `path` in an archive. Returns false, and
 * says nothing, if it is not there or not one. */
bool step_sprite_open(const ResArchive *archive, const char *path,
    Sprite256 *out);

/* Makes the folder `name` of the content folder, and the content folder
 * if need be, and writes its path to `out`, which holds `size`
 * characters. Says so on stderr and returns false if it cannot. */
bool step_folder(const char *step, const char *content_folder,
    const char *name, char *out, size_t size);

/* Makes the folder `name` in a folder that is there, and writes its
 * path to `out`. Returns false, and says nothing, if it cannot. */
bool step_subfolder(const char *parent, const char *name, char *out,
    size_t size);

/* Writes the bytes of a buffer as the file `name` in a folder. */
bool step_write(const char *folder, const char *name,
    const ByteBuffer *bytes);

/* Writes a sheet as the PNG file `name` in a folder, index 0 seen
 * through. A sheet without pixels, which is the soft edges of a thing
 * that has none, writes nothing and is no failure. */
bool step_sheet_write(const Sheet *sheet, const Rgb palette[256],
    const char *folder, const char *name);

/* Writes one thing of a registry into `folder`. Returns a message
 * saying what went wrong, or NULL. */
typedef const char *StepThingWriter(void *context, int32_t id,
    const char *folder);

/* Goes through the things of a registry and has `writer` write each, or
 * only the one with the ID `only` if that is not -1. `noun` is what one
 * of them is called, such as "object". Prints the folder, on stderr each
 * thing left out and why, and at the end how many were written.
 *
 * Returns how many were left out, or -1 if the registry does not say
 * how many things it has, or `only` names none of them. */
int step_things(const char *step, const char *noun, const Things *things,
    const char *folder, int32_t only, StepThingWriter *writer,
    void *context);

/* Writing a whole folder of an archive to disk, file by file, for the
 * steps whose files keep the names and the folders the original gives
 * them.
 */
typedef struct TreeCount {
    int written;
    int failed;
} TreeCount;

/* Writes the bytes of one of the original's files as the file at `path`,
 * in whatever form the step gives it. */
typedef bool TreeWriter(const uint8_t *data, size_t size, const char *path);

/* Writes a file as it is. */
bool tree_copy(const uint8_t *data, size_t size, const char *path);

/* Goes through the folder `inside` of the archive, or the whole archive
 * if `inside` is NULL, and the folders in it. Each file whose name ends
 * in `ending` is given to `writer` with a path in `folder`: the path it
 * has below `inside`, with `new_ending` in place of `ending`.
 *
 * A folder on disk is made when its first file turns up, so a folder
 * without such files leaves nothing behind. Each file written is noted
 * as read and counted in `count`, and so is each that failed. */
void tree_write(const ResArchive *archive, const char *inside,
    const char *folder, const char *ending, const char *new_ending,
    TreeWriter *writer, TreeCount *count);

/* Says whether a file of this name is to be passed over. */
typedef bool TreeSkip(const char *name);

/* The same, but for the files `skip` says to pass over, which are
 * neither written nor counted. */
void tree_write_but(const ResArchive *archive, const char *inside,
    const char *folder, const char *ending, const char *new_ending,
    TreeWriter *writer, TreeSkip *skip, TreeCount *count);

/* The same as tree_write, into the folder `to` of `folder`. */
void tree_write_in(const ResArchive *archive, const char *inside,
    const char *folder, const char *to, const char *ending,
    const char *new_ending, TreeWriter *writer, TreeCount *count);

/* Writing a sprite file of the original that no registry describes as
 * a sheet of its frames, for the steps that take such sprites as they
 * are. A sprite becomes three files named after it:
 *
 *     <name>.toml        how many frames it has and how they lie, and
 *                        for a cursor its `tip`: see import/cursor.h
 *     <name>_sheet.png   its frames in a grid, as indices with the
 *                        sprite's palette
 *     <name>_alpha.png   for a .16a, how solid each pixel is
 */
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
