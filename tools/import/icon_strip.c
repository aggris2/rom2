#include "import/icon_strip.h"

#include <stdlib.h>
#include <string.h>

#include "import/sprite16a.h"

bool icon_strip_create(IconStrip *out, uint32_t cell_width,
    uint32_t cell_height, uint32_t columns)
{
    if (cell_width == 0 || cell_width > ICON_STRIP_MAX_SIDE
        || cell_height == 0 || cell_height > ICON_STRIP_MAX_SIDE
        || columns == 0 || columns > ICON_STRIP_MAX_CELLS) {
        return false;
    }
    IconStrip strip;
    size_t total = (size_t)cell_width * columns * cell_height;
    strip.cell_width = cell_width;
    strip.cell_height = cell_height;
    strip.columns = columns;
    strip.pixels = calloc(total, 1);
    strip.solid = calloc(total, 1);
    strip.palettes = calloc((size_t)columns * 256, sizeof *strip.palettes);
    if (strip.pixels == NULL || strip.solid == NULL
        || strip.palettes == NULL) {
        icon_strip_free(&strip);
        return false;
    }
    *out = strip;
    return true;
}

bool icon_strip_put(IconStrip *strip, uint32_t column,
    const Sprite256 *icon)
{
    Rgb palette[256];
    Frame16a frame;
    if (column >= strip->columns || icon->frame_count == 0
        || !sprite256_palette(icon, palette)) {
        return false;
    }
    Reader r = sprite256_frames(icon);
    if (!sprite16a_frame_read(&r, &frame)) {
        return false;
    }
    if (frame.width > strip->cell_width
        || frame.height > strip->cell_height) {
        frame16a_free(&frame);
        return false;
    }

    uint32_t left = (strip->cell_width - frame.width) / 2;
    uint32_t top = (strip->cell_height - frame.height) / 2;
    size_t stride = (size_t)strip->cell_width * strip->columns;
    for (uint32_t y = 0; y < frame.height; y++) {
        size_t to = (size_t)(top + y) * stride
            + (size_t)column * strip->cell_width + left;
        size_t from = (size_t)y * frame.width;
        for (uint32_t x = 0; x < frame.width; x++) {
            strip->pixels[to + x] = frame.pixels[from + x];
            strip->solid[to + x] = (uint8_t)(frame.alpha[from + x] * 17);
        }
    }
    memcpy(strip->palettes + (size_t)column * 256, palette, sizeof palette);
    frame16a_free(&frame);
    return true;
}

void icon_strip_free(IconStrip *strip)
{
    free(strip->pixels);
    free(strip->solid);
    free(strip->palettes);
    memset(strip, 0, sizeof *strip);
}
