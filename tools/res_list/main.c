/* res_list: prints the path and size of every file in a .res archive.
 *
 *     res_list <archive.res>
 */
#include <stdio.h>
#include <stdlib.h>

#include "base/file.h"
#include "import/res.h"

/* Folders inside folders deeper than this are treated as a damaged
 * archive. The original game's archives go 5 deep. */
#define MAX_DEPTH 16

/* Prints every file in the folder made of `count` entries starting at
 * entry `first`, and in the folders inside it. `path` is the path of this
 * folder, ending in '/', or "" for the root. Returns false if an entry
 * could not be read or the folders are nested too deep. */
static bool list_folder(const ResArchive *a, uint32_t first, uint32_t count,
    const char *path, int depth)
{
    if (depth > MAX_DEPTH) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        ResEntry entry;
        if (!res_entry_at(a, first + i, &entry)) {
            return false;
        }
        if (entry.is_folder) {
            char inner[512];
            int length = snprintf(inner, sizeof inner, "%s%s/", path,
                entry.name);
            if (length < 0 || (size_t)length >= sizeof inner) {
                return false;
            }
            if (!list_folder(a, entry.offset, entry.size, inner, depth + 1)) {
                return false;
            }
        } else {
            printf("%s%s  %u\n", path, entry.name, entry.size);
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: res_list <archive.res>\n");
        return 2;
    }

    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(argv[1], &data, &size)) {
        fprintf(stderr, "res_list: cannot read %s\n", argv[1]);
        return 1;
    }

    ResArchive archive;
    if (!res_open(data, size, &archive)) {
        fprintf(stderr, "res_list: %s is not a .res archive\n", argv[1]);
        free(data);
        return 1;
    }

    bool ok = list_folder(&archive, archive.header.root_first,
        archive.header.root_count, "", 0);
    if (!ok) {
        fprintf(stderr, "res_list: %s is damaged\n", argv[1]);
    }

    free(data);
    return ok ? 0 : 1;
}
