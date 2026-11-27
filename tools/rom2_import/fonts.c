/* The fonts step of rom2_import: writes the fonts of the original game
 * as ROM2 content.
 *
 * The original has four, font1 to font4 in graphics.res. Each is a
 * sprite file with a frame for each character, and a .dat file with the
 * width of each. Every font gets a folder of its own in
 * <content folder>/fonts, with these files in it:
 *
 *     font.toml    the size of a cell, the character of the first
 *                  glyph, and the width of every glyph
 *     glyphs.png   all the glyphs in a grid 16 across
 *     alpha.png    for the font in colours: how solid each pixel is
 *
 * font1, font2 and font3 are of one colour, .16 files: their glyphs.png
 * is a picture in shades of grey that says how solid each pixel is, and
 * the game gives the text its colour. font4 is in colours, a .16a file:
 * its glyphs.png is indices with the font's palette, as a sprite's is.
 */
#include "rom2_import/steps.h"

#include <stdio.h>

#include "base/buffer.h"
#include "base/file.h"
#include "base/reader.h"
#include "import/font_sheet.h"
#include "import/png.h"
#include "import/sprite256.h"
#include "import/toml.h"

/* The fonts are font1, font2 and so on, as far as they go. */
#define MAX_FONTS 16
/* The most glyphs a font may have. The original has 224. */
#define MAX_GLYPHS 1024
/* The first glyph of a font is the space. */
#define FIRST_CHARACTER 32

/* Writes the bytes of a buffer as the file `name` in a folder. */
static bool write_in(const char *folder, const char *name,
    const ByteBuffer *bytes)
{
    char path[1100];
    int written = snprintf(path, sizeof path, "%s/%s", folder, name);
    return written > 0 && (size_t)written < sizeof path
        && file_write_all(path, bytes->data, bytes->size);
}

/* Finds the file "font<n>/font<n><ending>" in the archive. */
static bool font_file(const ResArchive *archive, int number,
    const char *ending, ResEntry *out)
{
    char path[64];
    snprintf(path, sizeof path, "font%d/font%d%s", number, number, ending);
    return res_find(archive, path, out) && !out->is_folder;
}

/* Writes font number `number`. Returns a message saying what went
 * wrong, NULL if all is well, and "" if the archive has no such font. */
static const char *font_write(const ResArchive *archive, int number,
    const char *fonts_folder)
{
    ResEntry file;
    ResEntry dat;
    Sprite256 sprite;
    bool coloured = false;
    if (!font_file(archive, number, ".16", &file)) {
        if (!font_file(archive, number, ".16a", &file)) {
            return "";
        }
        coloured = true;
    }
    if (!sprite256_open(archive->data + file.offset, file.size, &sprite)) {
        return "its sprite file cannot be read";
    }
    /* The widths: a number of 32 bits for each glyph. */
    static int32_t widths[MAX_GLYPHS];
    if (sprite.frame_count > MAX_GLYPHS
        || !font_file(archive, number, ".dat", &dat)
        || dat.size != sprite.frame_count * 4u) {
        return "its .dat file does not have a width for each glyph";
    }
    Reader r = reader_init(archive->data + dat.offset, dat.size);
    for (uint32_t i = 0; i < sprite.frame_count; i++) {
        widths[i] = reader_i32(&r);
    }

    Rgb palette[256];
    FontSheet sheet;
    if (coloured && !sprite256_palette(&sprite, palette)) {
        return "its sprite file has no palette";
    }
    if (!(coloured ? font_sheet_build_16a(&sprite, &sheet)
            : font_sheet_build_16(&sprite, &sheet))) {
        return "its glyphs could not be read";
    }

    char folder[1040];
    char comment[120];
    snprintf(folder, sizeof folder, "%s/font%d", fonts_folder, number);
    snprintf(comment, sizeof comment, "The original's font%d. The glyphs "
        "are in the order of the characters.", number);
    int32_t cell[2] = {(int32_t)sheet.cell_width,
        (int32_t)sheet.cell_height};
    uint32_t width = font_sheet_width(&sheet);
    uint32_t height = font_sheet_height(&sheet);

    ByteBuffer out = {0};
    const char *problem = NULL;
    /* In colours: indices and their solidness apart, as for every .16a.
     * In one colour the solidness is all there is. */
    bool ok = file_make_folder(folder)
        && (coloured
            ? png_write_indexed_solid(&out, width, height, sheet.pixels,
                palette)
            : png_write_grey(&out, width, height, sheet.solid))
        && write_in(folder, "glyphs.png", &out);
    out.size = 0;
    ok = ok && (!coloured
        || (png_write_grey(&out, width, height, sheet.solid)
            && write_in(folder, "alpha.png", &out)));
    out.size = 0;
    ok = ok && toml_comment(&out, comment)
        && toml_text(&out, "glyphs", "glyphs.png")
        && (!coloured || toml_text(&out, "alpha", "alpha.png"))
        && toml_bool(&out, "coloured", coloured)
        && toml_int_list(&out, "cell", cell, 2)
        && toml_int(&out, "columns", (int32_t)FONT_SHEET_COLUMNS)
        && toml_int(&out, "first_character", FIRST_CHARACTER)
        && toml_int(&out, "count", (int32_t)sheet.glyphs)
        && toml_int_list(&out, "widths", widths, sheet.glyphs)
        && write_in(folder, "font.toml", &out);
    if (!ok) {
        problem = "its folder could not be written";
    } else {
        printf("  font%d, %u glyphs of %u by %u%s\n", number, sheet.glyphs,
            sheet.cell_width, sheet.cell_height,
            coloured ? ", in colours" : "");
    }
    buffer_free(&out);
    font_sheet_free(&sheet);
    return problem;
}

int import_fonts(const ResArchive *graphics, const char *content_folder)
{
    char folder[1000];
    int written = snprintf(folder, sizeof folder, "%s/fonts",
        content_folder);
    if (written < 0 || (size_t)written >= sizeof folder
        || !file_make_folder(content_folder) || !file_make_folder(folder)) {
        fprintf(stderr, "fonts: cannot make the folder %s/fonts\n",
            content_folder);
        return -1;
    }
    int done = 0;
    int failed = 0;
    printf("%s\n", folder);
    for (int number = 1; number <= MAX_FONTS; number++) {
        const char *problem = font_write(graphics, number, folder);
        if (problem == NULL) {
            done++;
        } else if (problem[0] == '\0') {
            break;
        } else {
            fprintf(stderr, "  font%d left out: %s\n", number, problem);
            failed++;
        }
    }
    printf("%d fonts written, %d left out\n", done, failed);
    return done + failed == 0 ? -1 : failed;
}
