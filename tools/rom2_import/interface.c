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

#include "base/buffer.h"
#include "base/file.h"
#include "import/bmp.h"
#include "import/png.h"
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
    return total.failed;
}
