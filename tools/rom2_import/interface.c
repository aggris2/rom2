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
#include "import/plain_sheet.h"
#include "import/png.h"
#include "import/sprite256.h"
#include "import/toml.h"
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

/* Writes the bytes of a buffer as the file whose path is `base` with
 * `ending` after it. */
static bool write_as(const char *base, const char *ending,
    const ByteBuffer *bytes)
{
    char path[1300];
    int written = snprintf(path, sizeof path, "%s%s", base, ending);
    return written > 0 && (size_t)written < sizeof path
        && file_write_all(path, bytes->data, bytes->size);
}

/* Returns the name of the file at `path`, without its folders. */
static const char *file_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

/* Writes a sprite as its sheet, its solidness if it is a .16a, and its
 * definition. `base` is the path of the files without their endings.
 * The colours are those of `palette_from`, which is the sprite itself
 * but for a sprite of soft edges, which has the colours of its figure. */
static bool sprite_write(const Sprite256 *sprite,
    const Sprite256 *palette_from, bool is_16a, const char *base)
{
    Rgb palette[256];
    PlainSheet sheet;
    if (!sprite256_palette(palette_from, palette)
        || !(is_16a ? plain_sheet_build_16a(sprite, &sheet)
            : plain_sheet_build_256(sprite, &sheet))) {
        return false;
    }
    uint32_t width = plain_sheet_width(&sheet);
    uint32_t height = plain_sheet_height(&sheet);
    char sheet_name[80];
    char alpha_name[80];
    snprintf(sheet_name, sizeof sheet_name, "%.60s_sheet.png",
        file_name(base));
    snprintf(alpha_name, sizeof alpha_name, "%.60s_alpha.png",
        file_name(base));
    int32_t cell[2] = {(int32_t)sheet.cell_width, (int32_t)sheet.cell_height};

    /* In a .256 the index 0 is a pixel that is not there. In a .16a it
     * is a colour like the others, and the solidness says what is
     * there. */
    ByteBuffer out = {0};
    bool ok = (is_16a
            ? png_write_indexed_solid(&out, width, height, sheet.pixels,
                palette)
            : png_write_indexed(&out, width, height, sheet.pixels, palette))
        && write_as(base, "_sheet.png", &out);
    out.size = 0;
    ok = ok && (!is_16a
        || (png_write_grey(&out, width, height, sheet.solid)
            && write_as(base, "_alpha.png", &out)));
    out.size = 0;
    ok = ok && toml_text(&out, "sheet", sheet_name)
        && (!is_16a || toml_text(&out, "alpha", alpha_name))
        && toml_int(&out, "frames", (int32_t)sheet.frames)
        && toml_int(&out, "columns", (int32_t)sheet.columns)
        && toml_int_list(&out, "cell", cell, 2)
        && (sheet.one_size
            || (toml_comment(&out, "The frames are not all as large as "
                    "a cell. Each lies in the top left corner of its "
                    "own.")
                && toml_int_list(&out, "widths", sheet.widths, sheet.frames)
                && toml_int_list(&out, "heights", sheet.heights,
                    sheet.frames)))
        && write_as(base, ".toml", &out);
    buffer_free(&out);
    plain_sheet_free(&sheet);
    return ok;
}

/* Writes the sprite in `data`. `path` is where the walk would put it,
 * with ".png" at its end: the files are named after it without that. */
static bool sprite_file_write(const uint8_t *data, size_t size,
    const char *path, bool is_16a)
{
    char base[1250];
    size_t length = strlen(path);
    Sprite256 sprite;
    /* The original has a few empty files among its cursors. There is
     * nothing to write for one, and nothing has gone wrong. */
    if (size == 0) {
        return true;
    }
    if (length < 4 || length - 4 >= sizeof base
        || !sprite256_open(data, size, &sprite)) {
        return false;
    }
    memcpy(base, path, length - 4);
    base[length - 4] = '\0';
    return sprite_write(&sprite, &sprite, is_16a, base);
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
    /* Two of the old cursors keep their pixels in some other way: read
     * as a .256 they have the right number of frames of the right size
     * and come out as noise. They are left out until that is known.
     * The cursors the game shows are the .16a ones beside them. */
    if (path_ends_in(path, "/cursors/attack.png")
        || path_ends_in(path, "/cursors/pickup.png")) {
        return false;
    }
    return sprite_file_write(data, size, path, false);
}

static bool sprite16a_file_write(const uint8_t *data, size_t size,
    const char *path)
{
    return sprite_file_write(data, size, path, true);
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
            sprite16a_file_write, count);
    } else {
        tree_write_in(archive, inside, folder, to, ".256", ".png",
            sprite256_file_write, count);
        tree_write_in(archive, inside, folder, to, ".16a", ".png",
            sprite16a_file_write, count);
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
    if (sprite_write(&sprite, &sprite, false, base)) {
        count->written++;
    } else {
        count->failed++;
    }
    if (res_find(archive, "backpack/spritesb.256", &edges_entry)
        && !edges_entry.is_folder
        && sprite256_open(archive->data + edges_entry.offset,
            edges_entry.size, &edges)) {
        strcat(base, "_edges");
        if (sprite_write(&edges, &sprite, false, base)) {
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
