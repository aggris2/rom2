#include "rom2_import/tree.h"

#include <stdio.h>
#include <string.h>

#include "base/file.h"

/* The deepest folders may be nested. The original goes 5 deep. */
#define MAX_DEPTH 16
#define PATH_SIZE 1200

typedef struct Walk {
    const ResArchive *archive;
    const char *ending;
    const char *new_ending;
    TreeWriter *writer;
    TreeSkip *skip;
    TreeCount *count;
} Walk;

bool tree_copy(const uint8_t *data, size_t size, const char *path)
{
    return file_write_all(path, data, size);
}

/* Says whether `name` ends in `ending` and has something before it. */
static bool ends_in(const char *name, const char *ending)
{
    size_t length = strlen(name);
    size_t end = strlen(ending);
    return length > end && strcmp(name + length - end, ending) == 0;
}

/* Makes the folder at `path` and every folder above it that is not
 * there yet. */
static bool make_folders(const char *path)
{
    char part[PATH_SIZE];
    size_t length = strlen(path);
    if (length >= sizeof part) {
        return false;
    }
    /* Each longer start of the path in turn. One that is there already,
     * or cannot be a folder at all, such as "C:", does no harm. */
    for (size_t i = 1; i < length; i++) {
        if (path[i] == '/') {
            memcpy(part, path, i);
            part[i] = '\0';
            file_make_folder(part);
        }
    }
    return file_make_folder(path);
}

/* Goes through the folder made of `count` entries starting at entry
 * `first`, and the folders in it. `folder` is the folder on disk that
 * is to hold its files. */
static void walk(const Walk *w, uint32_t first, uint32_t count,
    const char *folder, int depth)
{
    bool made = false;
    for (uint32_t i = 0; i < count; i++) {
        ResEntry entry;
        char path[PATH_SIZE];
        if (!res_entry_at(w->archive, first + i, &entry)) {
            continue;
        }
        if (!entry.is_folder && (!ends_in(entry.name, w->ending)
                || (w->skip != NULL && w->skip(entry.name)))) {
            continue;
        }
        int written = snprintf(path, sizeof path, "%s/%s", folder,
            entry.name);
        if (written < 0 || (size_t)written >= sizeof path
            || depth >= MAX_DEPTH) {
            w->count->failed++;
            continue;
        }
        if (entry.is_folder) {
            walk(w, entry.offset, entry.size, path, depth + 1);
            continue;
        }
        if (!made) {
            made = make_folders(folder);
        }
        /* The two endings are equally long. */
        strcpy(path + strlen(path) - strlen(w->ending), w->new_ending);
        if (w->writer(w->archive->data + entry.offset, entry.size, path)) {
            res_note(w->archive, first + i);
            w->count->written++;
        } else {
            fprintf(stderr, "  %s left out: it could not be written\n",
                path);
            w->count->failed++;
        }
    }
}

void tree_write(const ResArchive *archive, const char *inside,
    const char *folder, const char *ending, const char *new_ending,
    TreeWriter *writer, TreeCount *count)
{
    tree_write_but(archive, inside, folder, ending, new_ending, writer,
        NULL, count);
}

void tree_write_but(const ResArchive *archive, const char *inside,
    const char *folder, const char *ending, const char *new_ending,
    TreeWriter *writer, TreeSkip *skip, TreeCount *count)
{
    Walk w = {archive, ending, new_ending, writer, skip, count};
    uint32_t first = archive->header.root_first;
    uint32_t entries = archive->header.root_count;
    if (strlen(ending) != strlen(new_ending)) {
        count->failed++;
        return;
    }
    if (inside != NULL) {
        ResEntry entry;
        if (!res_find(archive, inside, &entry) || !entry.is_folder) {
            fprintf(stderr, "  no folder %s in the archive\n", inside);
            count->failed++;
            return;
        }
        first = entry.offset;
        entries = entry.size;
    }
    walk(&w, first, entries, folder, 0);
}

void tree_write_in(const ResArchive *archive, const char *inside,
    const char *folder, const char *to, const char *ending,
    const char *new_ending, TreeWriter *writer, TreeCount *count)
{
    char path[PATH_SIZE];
    int written = snprintf(path, sizeof path, "%s/%s", folder, to);
    if (written < 0 || (size_t)written >= sizeof path) {
        count->failed++;
        return;
    }
    tree_write(archive, inside, path, ending, new_ending, writer, count);
}
