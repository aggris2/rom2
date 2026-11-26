#include "import/plain_sheet.h"

#include <stdlib.h>
#include <string.h>

#include "import/png.h"
#include "import/sprite16a.h"

/* One frame as it was read, whichever kind of file it came from. `alpha`
 * is NULL for a frame of a .256 file. */
typedef struct Piece {
    uint32_t width;
    uint32_t height;
    uint8_t *pixels;
    uint8_t *alpha;
} Piece;

static void pieces_free(Piece *pieces, uint32_t count)
{
    for (uint32_t i = 0; pieces != NULL && i < count; i++) {
        free(pieces[i].pixels);
        free(pieces[i].alpha);
    }
    free(pieces);
}

/* Reads every frame of the sprite. Returns NULL if one cannot be read. */
static Piece *pieces_read(const Sprite256 *sprite, bool is_16a)
{
    Piece *pieces = calloc(sprite->frame_count, sizeof *pieces);
    if (pieces == NULL) {
        return NULL;
    }
    Reader r = sprite256_frames(sprite);
    for (uint32_t i = 0; i < sprite->frame_count; i++) {
        bool ok;
        if (is_16a) {
            Frame16a frame;
            ok = sprite16a_frame_read(&r, &frame);
            if (ok) {
                Piece piece = {frame.width, frame.height, frame.pixels,
                    frame.alpha};
                pieces[i] = piece;
            }
        } else {
            Frame256 frame;
            ok = sprite256_frame_read(&r, &frame);
            if (ok) {
                Piece piece = {frame.width, frame.height, frame.pixels,
                    NULL};
                pieces[i] = piece;
            }
        }
        if (!ok) {
            pieces_free(pieces, sprite->frame_count);
            return NULL;
        }
    }
    return pieces;
}

/* Sets the size of the grid for frames whose largest is `width` by
 * `height`, and gets its memory. */
static bool sheet_create(PlainSheet *sheet, uint32_t frames, uint32_t width,
    uint32_t height)
{
    memset(sheet, 0, sizeof *sheet);
    if (frames == 0 || width == 0 || height == 0
        || width > PNG_MAX_SIDE) {
        return false;
    }
    uint32_t columns = PLAIN_SHEET_WIDTH / width;
    if (columns == 0) {
        columns = 1;
    }
    if (columns > frames) {
        columns = frames;
    }
    uint32_t rows = (frames + columns - 1) / columns;
    if (rows > PNG_MAX_SIDE / height) {
        return false;
    }
    size_t total = (size_t)columns * width * rows * height;
    sheet->frames = frames;
    sheet->cell_width = width;
    sheet->cell_height = height;
    sheet->columns = columns;
    sheet->rows = rows;
    sheet->one_size = true;
    sheet->pixels = calloc(total, 1);
    sheet->solid = calloc(total, 1);
    sheet->widths = calloc(frames, sizeof *sheet->widths);
    sheet->heights = calloc(frames, sizeof *sheet->heights);
    if (sheet->pixels == NULL || sheet->solid == NULL
        || sheet->widths == NULL || sheet->heights == NULL) {
        plain_sheet_free(sheet);
        return false;
    }
    return true;
}

static bool build(const Sprite256 *sprite, bool is_16a, PlainSheet *out)
{
    if (sprite->frame_count == 0) {
        return false;
    }
    Piece *pieces = pieces_read(sprite, is_16a);
    if (pieces == NULL) {
        return false;
    }
    uint32_t width = 0;
    uint32_t height = 0;
    for (uint32_t i = 0; i < sprite->frame_count; i++) {
        if (pieces[i].width > width) {
            width = pieces[i].width;
        }
        if (pieces[i].height > height) {
            height = pieces[i].height;
        }
    }
    PlainSheet sheet;
    if (!sheet_create(&sheet, sprite->frame_count, width, height)) {
        pieces_free(pieces, sprite->frame_count);
        return false;
    }

    size_t stride = (size_t)sheet.columns * sheet.cell_width;
    for (uint32_t i = 0; i < sheet.frames; i++) {
        const Piece *piece = &pieces[i];
        size_t left = (size_t)(i % sheet.columns) * sheet.cell_width;
        size_t top = (size_t)(i / sheet.columns) * sheet.cell_height;
        for (uint32_t y = 0; y < piece->height; y++) {
            size_t to = (top + y) * stride + left;
            size_t from = (size_t)y * piece->width;
            for (uint32_t x = 0; x < piece->width; x++) {
                uint8_t index = piece->pixels[from + x];
                sheet.pixels[to + x] = index;
                /* The file's 15 levels, spread over 17 to 255. */
                sheet.solid[to + x] = piece->alpha != NULL
                    ? (uint8_t)(piece->alpha[from + x] * 17)
                    : (uint8_t)(index != 0 ? 255 : 0);
            }
        }
        sheet.widths[i] = (int32_t)piece->width;
        sheet.heights[i] = (int32_t)piece->height;
        if (piece->width != width || piece->height != height) {
            sheet.one_size = false;
        }
    }
    pieces_free(pieces, sprite->frame_count);
    *out = sheet;
    return true;
}

bool plain_sheet_build_256(const Sprite256 *sprite, PlainSheet *out)
{
    return build(sprite, false, out);
}

bool plain_sheet_build_16a(const Sprite256 *sprite, PlainSheet *out)
{
    return build(sprite, true, out);
}

void plain_sheet_free(PlainSheet *sheet)
{
    free(sheet->pixels);
    free(sheet->solid);
    free(sheet->widths);
    free(sheet->heights);
    memset(sheet, 0, sizeof *sheet);
}

uint32_t plain_sheet_width(const PlainSheet *sheet)
{
    return sheet->columns * sheet->cell_width;
}

uint32_t plain_sheet_height(const PlainSheet *sheet)
{
    return sheet->rows * sheet->cell_height;
}
