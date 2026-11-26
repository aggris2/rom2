/* Writing a whole folder of an archive to disk, file by file, for the
 * steps whose files keep the names and the folders the original gives
 * them.
 */
#ifndef ROM2_ROM2_IMPORT_TREE_H
#define ROM2_ROM2_IMPORT_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/res.h"

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
 * has below `inside`, with `new_ending` in place of `ending`. The two
 * endings must be equally long.
 *
 * A folder on disk is made when its first file turns up, so a folder
 * without such files leaves nothing behind. Each file written is noted
 * as read and counted in `count`, and so is each that failed. */
void tree_write(const ResArchive *archive, const char *inside,
    const char *folder, const char *ending, const char *new_ending,
    TreeWriter *writer, TreeCount *count);

/* The same, into the folder `to` of `folder`. */
void tree_write_in(const ResArchive *archive, const char *inside,
    const char *folder, const char *to, const char *ending,
    const char *new_ending, TreeWriter *writer, TreeCount *count);

#endif
