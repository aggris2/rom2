#include "rom2_import/sprite_files.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/plain_sheet.h"
#include "import/png.h"
#include "import/toml.h"

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
bool sprite_files_write(const Sprite256 *sprite,
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
static bool file_write(const uint8_t *data, size_t size,
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
    return sprite_files_write(&sprite, &sprite, is_16a, base);
}

bool sprite_files_write_256(const uint8_t *data, size_t size,
    const char *path)
{
    return file_write(data, size, path, false);
}

bool sprite_files_write_16a(const uint8_t *data, size_t size,
    const char *path)
{
    return file_write(data, size, path, true);
}
