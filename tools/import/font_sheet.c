#include "import/font_sheet.h"

#include <stdlib.h>
#include <string.h>

#include "import/png.h"
#include "import/sprite16.h"
#include "import/sprite16a.h"

/* One glyph as it was read. `pixels` is NULL for a font of one colour.
 * `alpha` is 0 to 15. */
typedef struct Glyph {
    uint32_t width;
    uint32_t height;
    uint8_t *pixels;
    uint8_t *alpha;
} Glyph;

static void glyphs_free(Glyph *glyphs, uint32_t count)
{
    for (uint32_t i = 0; glyphs != NULL && i < count; i++) {
        free(glyphs[i].pixels);
        free(glyphs[i].alpha);
    }
    free(glyphs);
}

/* Reads every glyph of the file. Returns NULL if one cannot be read. */
static Glyph *glyphs_read(const Sprite256 *sprite, bool coloured)
{
    Glyph *glyphs = calloc(sprite->frame_count, sizeof *glyphs);
    if (glyphs == NULL) {
        return NULL;
    }
    Reader r = sprite256_frames(sprite);
    for (uint32_t i = 0; i < sprite->frame_count; i++) {
        bool ok;
        if (coloured) {
            Frame16a frame;
            ok = sprite16a_frame_read(&r, &frame);
            if (ok) {
                Glyph glyph = {frame.width, frame.height, frame.pixels,
                    frame.alpha};
                glyphs[i] = glyph;
            }
        } else {
            Frame16 frame;
            ok = sprite16_frame_read(&r, &frame);
            if (ok) {
                Glyph glyph = {frame.width, frame.height, NULL,
                    frame.alpha};
                glyphs[i] = glyph;
            }
        }
        if (!ok) {
            glyphs_free(glyphs, sprite->frame_count);
            return NULL;
        }
    }
    return glyphs;
}

static bool build(const Sprite256 *sprite, bool coloured, FontSheet *out)
{
    if (sprite->frame_count == 0) {
        return false;
    }
    Glyph *glyphs = glyphs_read(sprite, coloured);
    if (glyphs == NULL) {
        return false;
    }
    FontSheet sheet;
    memset(&sheet, 0, sizeof sheet);
    sheet.glyphs = sprite->frame_count;
    for (uint32_t i = 0; i < sheet.glyphs; i++) {
        if (glyphs[i].width > sheet.cell_width) {
            sheet.cell_width = glyphs[i].width;
        }
        if (glyphs[i].height > sheet.cell_height) {
            sheet.cell_height = glyphs[i].height;
        }
    }
    sheet.rows = (sheet.glyphs + FONT_SHEET_COLUMNS - 1)
        / FONT_SHEET_COLUMNS;
    bool ok = sheet.cell_width > 0 && sheet.cell_height > 0
        && sheet.cell_width <= PNG_MAX_SIDE / FONT_SHEET_COLUMNS
        && sheet.rows <= PNG_MAX_SIDE / sheet.cell_height;
    size_t stride = (size_t)FONT_SHEET_COLUMNS * sheet.cell_width;
    if (ok) {
        size_t total = stride * sheet.rows * sheet.cell_height;
        sheet.solid = calloc(total, 1);
        sheet.pixels = coloured ? calloc(total, 1) : NULL;
        ok = sheet.solid != NULL && (!coloured || sheet.pixels != NULL);
    }
    for (uint32_t i = 0; ok && i < sheet.glyphs; i++) {
        const Glyph *glyph = &glyphs[i];
        size_t left = (size_t)(i % FONT_SHEET_COLUMNS) * sheet.cell_width;
        size_t top = (size_t)(i / FONT_SHEET_COLUMNS) * sheet.cell_height;
        for (uint32_t y = 0; y < glyph->height; y++) {
            size_t to = (top + y) * stride + left;
            size_t from = (size_t)y * glyph->width;
            for (uint32_t x = 0; x < glyph->width; x++) {
                /* The file's 15 levels, spread over 17 to 255. */
                sheet.solid[to + x] = (uint8_t)(glyph->alpha[from + x] * 17);
                if (coloured) {
                    sheet.pixels[to + x] = glyph->pixels[from + x];
                }
            }
        }
    }
    glyphs_free(glyphs, sprite->frame_count);
    if (!ok) {
        free(sheet.solid);
        free(sheet.pixels);
        return false;
    }
    *out = sheet;
    return true;
}

bool font_sheet_build_16(const Sprite256 *sprite, FontSheet *out)
{
    return build(sprite, false, out);
}

bool font_sheet_build_16a(const Sprite256 *sprite, FontSheet *out)
{
    return build(sprite, true, out);
}

void font_sheet_free(FontSheet *sheet)
{
    free(sheet->pixels);
    free(sheet->solid);
    memset(sheet, 0, sizeof *sheet);
}

uint32_t font_sheet_width(const FontSheet *sheet)
{
    return FONT_SHEET_COLUMNS * sheet->cell_width;
}

uint32_t font_sheet_height(const FontSheet *sheet)
{
    return sheet->rows * sheet->cell_height;
}
