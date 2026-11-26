/* The interface step of rom2_import: writes the plain pictures of the
 * original game as PNG files.
 *
 * The pictures of the interface, of the menus and of the shops are .bmp
 * files in three archives. They all go into <content folder>/interface,
 * each under the path it has in its archive below the folder named
 * here:
 *
 *     graphics.res  interface/    to  interface/
 *     graphics.res  infowindow/   to  interface/infowindow/
 *     graphics.res  global.map/   to  interface/global.map/
 *     main.res      graphics/     to  interface/
 *     movies.res    everything    to  interface/
 *
 * so graphics.res interface/inn/1.bmp becomes interface/inn/1.png. No
 * two pictures of the original come to the same place.
 *
 * A picture of 24 bits for each pixel becomes a PNG in full colour, one
 * of 8 bits an 8-bit indexed PNG with its palette.
 */
#include "rom2_import/steps.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/bmp.h"
#include "import/png.h"

/* The deepest folders may be nested. The original goes 5 deep. */
#define MAX_DEPTH 16
#define PATH_SIZE 1200

typedef struct Count {
    int written;
    int failed;
} Count;

/* Says whether `name` ends in ".bmp". */
static bool is_bmp(const char *name)
{
    size_t length = strlen(name);
    return length > 4 && strcmp(name + length - 4, ".bmp") == 0;
}

/* Writes one picture as the PNG file at `path`. */
static bool picture_write(const uint8_t *data, size_t size,
    const char *path)
{
    ByteBuffer png = {0};
    bool ok = false;
    uint32_t bits = bmp_bits(data, size);
    if (bits == 24) {
        Bmp24 bmp;
        if (bmp24_read(data, size, &bmp)) {
            ok = png_write_rgb(&png, bmp.width, bmp.height, bmp.pixels);
            bmp24_free(&bmp);
        }
    } else if (bits == 8) {
        Bmp8 bmp;
        if (bmp8_read(data, size, &bmp)) {
            ok = png_write_indexed_solid(&png, bmp.width, bmp.height,
                bmp.pixels, bmp.palette);
            bmp8_free(&bmp);
        }
    }
    ok = ok && file_write_all(path, png.data, png.size);
    buffer_free(&png);
    return ok;
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
 * is to hold its pictures. It is made when the first one turns up, so
 * a folder without pictures leaves nothing behind. */
static void walk(const ResArchive *archive, uint32_t first, uint32_t count,
    const char *folder, int depth, Count *total)
{
    bool made = false;
    for (uint32_t i = 0; i < count; i++) {
        ResEntry entry;
        char path[PATH_SIZE];
        if (!res_entry_at(archive, first + i, &entry)) {
            continue;
        }
        if (!entry.is_folder && !is_bmp(entry.name)) {
            continue;
        }
        int written = snprintf(path, sizeof path, "%s/%s", folder,
            entry.name);
        if (written < 0 || (size_t)written >= sizeof path
            || depth >= MAX_DEPTH) {
            total->failed++;
            continue;
        }
        if (entry.is_folder) {
            walk(archive, entry.offset, entry.size, path, depth + 1,
                total);
            continue;
        }
        if (!made) {
            made = make_folders(folder);
        }
        /* ".bmp" becomes ".png". */
        strcpy(path + strlen(path) - 4, ".png");
        if (picture_write(archive->data + entry.offset, entry.size, path)) {
            res_note(archive, first + i);
            total->written++;
        } else {
            fprintf(stderr, "  %s could not be written\n", path);
            total->failed++;
        }
    }
}

/* Writes the pictures of the archive's folder `inside`, or of the whole
 * archive if `inside` is NULL, into the folder `to` of the interface
 * folder, or into the interface folder itself if `to` is NULL. */
static void folder_write(const ResArchive *archive, const char *inside,
    const char *interface_folder, const char *to, Count *total)
{
    uint32_t first = archive->header.root_first;
    uint32_t count = archive->header.root_count;
    if (inside != NULL) {
        ResEntry entry;
        if (!res_find(archive, inside, &entry) || !entry.is_folder) {
            fprintf(stderr, "  no folder %s in the archive\n", inside);
            total->failed++;
            return;
        }
        first = entry.offset;
        count = entry.size;
    }
    char folder[PATH_SIZE];
    int written = to != NULL
        ? snprintf(folder, sizeof folder, "%s/%s", interface_folder, to)
        : snprintf(folder, sizeof folder, "%s", interface_folder);
    if (written < 0 || (size_t)written >= sizeof folder) {
        total->failed++;
        return;
    }
    walk(archive, first, count, folder, 0, total);
}

int import_interface(const ResArchive *graphics,
    const ResArchive *main_archive, const ResArchive *movies,
    const char *content_folder)
{
    char folder[1000];
    int written = snprintf(folder, sizeof folder, "%s/interface",
        content_folder);
    if (written < 0 || (size_t)written >= sizeof folder
        || !file_make_folder(content_folder)
        || !file_make_folder(folder)) {
        fprintf(stderr, "interface: cannot make the folder %s/interface\n",
            content_folder);
        return -1;
    }

    Count total = {0, 0};
    printf("%s\n", folder);
    folder_write(graphics, "interface", folder, NULL, &total);
    folder_write(graphics, "infowindow", folder, "infowindow", &total);
    folder_write(graphics, "global.map", folder, "global.map", &total);
    if (main_archive != NULL) {
        folder_write(main_archive, "graphics", folder, NULL, &total);
    }
    if (movies != NULL) {
        folder_write(movies, NULL, folder, NULL, &total);
    }
    printf("%d pictures written, %d left out\n", total.written,
        total.failed);
    return total.failed;
}
