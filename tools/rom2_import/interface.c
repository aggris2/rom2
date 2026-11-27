/* The interface step of rom2_import: writes the pictures and the
 * sprites of the interface, the menus, the shops and the cursors.
 *
 * They all go into <content folder>/interface, each under the path it
 * has in its archive below the folder named here:
 *
 *     graphics.res  interface/    to  interface/
 *     graphics.res  infowindow/   to  interface/infowindow/
 *     graphics.res  global.map/   to  interface/global.map/
 *     graphics.res  cursors/      to  interface/cursors/
 *     graphics.res  backpack/     to  interface/backpack/
 *     main.res      graphics/     to  interface/
 *     movies.res    everything    to  interface/
 *
 * A picture, a .bmp file, becomes a PNG of the same name: one of 24
 * bits for each pixel a PNG in full colour, one of 8 bits an 8-bit
 * indexed PNG with its palette. No two come to the same place.
 *
 * A sprite, a .256 or .16a file, becomes three files named after it,
 * here for cursors/wait/sprites.16a:
 *
 *     sprites.toml        how many frames it has and how they lie
 *     sprites_sheet.png   its frames in a grid, as indices with the
 *                         sprite's palette
 *     sprites_alpha.png   for a .16a, how solid each pixel is
 *
 * The sheet has "_sheet" in its name because the original has pictures
 * and sprites of one name, such as shopframe.bmp and shopframe.256.
 *
 * The backpack's second sprite is its soft edges, as a unit has them,
 * and is written with the colours of the first.
 */
#include "rom2_import/steps.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/bmp.h"
#include "import/png.h"
#include "import/sprite256.h"
#include "rom2_import/sprite_files.h"
#include "rom2_import/tree.h"

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

/* Says whether `path` ends in `ending`. */
static bool path_ends_in(const char *path, const char *ending)
{
    size_t length = strlen(path);
    size_t end = strlen(ending);
    return length >= end && strcmp(path + length - end, ending) == 0;
}

static bool sprite256_file_write(const uint8_t *data, size_t size,
    const char *path)
{
    /* Two of the old cursors are broken files: each pixel is three
     * bytes of colour where a .256 has one byte, in frames that are
     * still counted out as one byte for a pixel, so a frame holds the
     * top third of its picture and reads as noise. They are left out.
     * The cursors the game shows are the .16a ones beside them. */
    if (path_ends_in(path, "/cursors/attack.png")
        || path_ends_in(path, "/cursors/pickup.png")) {
        return false;
    }
    return sprite_files_write_256(data, size, path);
}

/* Writes the sprites of the archive's folder `inside` into the folder
 * `to` of the interface folder, or into the interface folder itself if
 * `to` is NULL. */
static void sprites_write(const ResArchive *archive, const char *inside,
    const char *folder, const char *to, TreeCount *count)
{
    if (to == NULL) {
        tree_write(archive, inside, folder, ".256", ".png",
            sprite256_file_write, count);
        tree_write(archive, inside, folder, ".16a", ".png",
            sprite_files_write_16a, count);
    } else {
        tree_write_in(archive, inside, folder, to, ".256", ".png",
            sprite256_file_write, count);
        tree_write_in(archive, inside, folder, to, ".16a", ".png",
            sprite_files_write_16a, count);
    }
}

/* Writes the backpack that lies on the ground: a sprite and its soft
 * edges, which have the sprite's colours. */
static void backpack_write(const ResArchive *archive, const char *folder,
    TreeCount *count)
{
    char base[1100];
    ResEntry entry;
    ResEntry edges_entry;
    Sprite256 sprite;
    Sprite256 edges;
    int written = snprintf(base, sizeof base, "%s/backpack", folder);
    if (written < 0 || (size_t)written >= sizeof base - 20
        || !file_make_folder(base)
        || !res_find(archive, "backpack/sprites.256", &entry)
        || entry.is_folder
        || !sprite256_open(archive->data + entry.offset, entry.size,
            &sprite)) {
        fprintf(stderr, "  the backpack could not be written\n");
        count->failed++;
        return;
    }
    strcat(base, "/sprites");
    if (sprite_files_write(&sprite, &sprite, false, base)) {
        count->written++;
    } else {
        count->failed++;
    }
    if (res_find(archive, "backpack/spritesb.256", &edges_entry)
        && !edges_entry.is_folder
        && sprite256_open(archive->data + edges_entry.offset,
            edges_entry.size, &edges)) {
        strcat(base, "_edges");
        if (sprite_files_write(&edges, &sprite, false, base)) {
            count->written++;
        } else {
            count->failed++;
        }
    }
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

    TreeCount total = {0, 0};
    printf("%s\n", folder);
    tree_write(graphics, "interface", folder, ".bmp", ".png",
        picture_write, &total);
    tree_write_in(graphics, "infowindow", folder, "infowindow", ".bmp",
        ".png", picture_write, &total);
    tree_write_in(graphics, "global.map", folder, "global.map", ".bmp",
        ".png", picture_write, &total);
    if (main_archive != NULL) {
        tree_write(main_archive, "graphics", folder, ".bmp", ".png",
            picture_write, &total);
    }
    if (movies != NULL) {
        tree_write(movies, NULL, folder, ".bmp", ".png", picture_write,
            &total);
    }
    printf("%d pictures written, %d left out\n", total.written,
        total.failed);

    TreeCount sprites = {0, 0};
    sprites_write(graphics, "interface", folder, NULL, &sprites);
    sprites_write(graphics, "global.map", folder, "global.map", &sprites);
    sprites_write(graphics, "cursors", folder, "cursors", &sprites);
    backpack_write(graphics, folder, &sprites);
    printf("%d sprites written, %d left out\n", sprites.written,
        sprites.failed);
    return total.failed + sprites.failed;
}
