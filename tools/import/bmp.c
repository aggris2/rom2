#include "import/bmp.h"

#include <stdlib.h>
#include <string.h>

#include "base/reader.h"

#define FILE_HEADER_SIZE 14u
#define INFO_HEADER_SIZE 40u

bool bmp8_read(const uint8_t *data, size_t size, Bmp8 *out)
{
    Reader r = reader_init(data, size);

    /* The file header: "BM", the file's size, 4 unused bytes, and where
     * the pixels start. */
    uint8_t b = reader_u8(&r);
    uint8_t m = reader_u8(&r);
    reader_skip(&r, 8);
    uint32_t pixels_at = reader_u32(&r);

    /* The picture header: its own size, width, height, planes, bits for
     * each pixel, compression, three numbers that do not matter here,
     * and the number of colours in the table, where 0 stands for 256. */
    uint32_t header_size = reader_u32(&r);
    int32_t width = reader_i32(&r);
    int32_t height = reader_i32(&r);
    reader_skip(&r, 2);
    uint16_t bits = reader_u16(&r);
    uint32_t compression = reader_u32(&r);
    reader_skip(&r, 12);
    uint32_t colours = reader_u32(&r);
    reader_skip(&r, 4);
    if (!reader_ok(&r) || b != 'B' || m != 'M'
        || header_size < INFO_HEADER_SIZE || bits != 8 || compression != 0
        || width <= 0 || (uint32_t)width > BMP_MAX_SIDE
        || height <= 0 || (uint32_t)height > BMP_MAX_SIDE
        || colours > 256) {
        return false;
    }
    if (colours == 0) {
        colours = 256;
    }

    /* The colour table comes right after the picture header: 4 bytes
     * for each colour, blue, green, red and one that is not used. */
    Bmp8 bmp;
    memset(&bmp, 0, sizeof bmp);
    reader_skip(&r, header_size - INFO_HEADER_SIZE);
    const uint8_t *table = reader_bytes(&r, (size_t)colours * 4);
    if (!reader_ok(&r)) {
        return false;
    }
    for (uint32_t i = 0; i < colours; i++) {
        bmp.palette[i].b = table[i * 4];
        bmp.palette[i].g = table[i * 4 + 1];
        bmp.palette[i].r = table[i * 4 + 2];
    }

    /* A row is filled out to a multiple of 4 bytes. */
    size_t stride = ((size_t)width + 3) / 4 * 4;
    if (pixels_at > size || stride > (size - pixels_at) / (size_t)height) {
        return false;
    }
    bmp.width = (uint32_t)width;
    bmp.height = (uint32_t)height;
    bmp.pixels = malloc((size_t)width * (size_t)height);
    if (bmp.pixels == NULL) {
        return false;
    }
    /* The file's first row is the picture's last. */
    for (uint32_t y = 0; y < bmp.height; y++) {
        const uint8_t *row = data + pixels_at
            + (size_t)(bmp.height - 1 - y) * stride;
        memcpy(bmp.pixels + (size_t)y * bmp.width, row, bmp.width);
    }
    *out = bmp;
    return true;
}

void bmp8_free(Bmp8 *bmp)
{
    free(bmp->pixels);
    memset(bmp, 0, sizeof *bmp);
}
