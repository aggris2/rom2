#include "import/bmp.h"

#include <stdlib.h>
#include <string.h>

#include "base/reader.h"

#define FILE_HEADER_SIZE 14u
#define INFO_HEADER_SIZE 40u

/* What the two headers of a BMP say. */
typedef struct Header {
    uint32_t pixels_at;
    uint32_t header_size;
    uint32_t width;
    uint32_t height;
    uint32_t bits;
    uint32_t colours;
} Header;

/* Reads the two headers and leaves the reader after them. Fails for a
 * file that is not a BMP, is compressed, is stored from the top down,
 * or has no size or one over the limit. */
static bool read_header(Reader *r, Header *out)
{
    /* The file header: "BM", the file's size, 4 unused bytes, and where
     * the pixels start. */
    uint8_t b = reader_u8(r);
    uint8_t m = reader_u8(r);
    reader_skip(r, 8);
    uint32_t pixels_at = reader_u32(r);

    /* The picture header: its own size, width, height, planes, bits for
     * each pixel, compression, three numbers that do not matter here,
     * and the number of colours in the table, where 0 stands for all. */
    uint32_t header_size = reader_u32(r);
    int32_t width = reader_i32(r);
    int32_t height = reader_i32(r);
    reader_skip(r, 2);
    uint16_t bits = reader_u16(r);
    uint32_t compression = reader_u32(r);
    reader_skip(r, 12);
    uint32_t colours = reader_u32(r);
    reader_skip(r, 4);
    if (!reader_ok(r) || b != 'B' || m != 'M'
        || header_size < INFO_HEADER_SIZE || compression != 0
        || width <= 0 || (uint32_t)width > BMP_MAX_SIDE
        || height <= 0 || (uint32_t)height > BMP_MAX_SIDE) {
        return false;
    }
    reader_skip(r, header_size - INFO_HEADER_SIZE);
    if (!reader_ok(r)) {
        return false;
    }
    out->pixels_at = pixels_at;
    out->header_size = header_size;
    out->width = (uint32_t)width;
    out->height = (uint32_t)height;
    out->bits = bits;
    out->colours = colours;
    return true;
}

uint32_t bmp_bits(const uint8_t *data, size_t size)
{
    Reader r = reader_init(data, size);
    Header header;
    return read_header(&r, &header) ? header.bits : 0;
}

bool bmp8_read(const uint8_t *data, size_t size, Bmp8 *out)
{
    Reader r = reader_init(data, size);
    Header header;
    if (!read_header(&r, &header) || header.bits != 8
        || header.colours > 256) {
        return false;
    }
    uint32_t colours = header.colours == 0 ? 256 : header.colours;
    uint32_t width = header.width;
    uint32_t height = header.height;
    uint32_t pixels_at = header.pixels_at;

    /* The colour table comes right after the picture header: 4 bytes
     * for each colour, blue, green, red and one that is not used. */
    Bmp8 bmp;
    memset(&bmp, 0, sizeof bmp);
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
    if (pixels_at > size || stride > (size - pixels_at) / height) {
        return false;
    }
    bmp.width = width;
    bmp.height = height;
    bmp.pixels = malloc((size_t)width * height);
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

bool bmp24_read(const uint8_t *data, size_t size, Bmp24 *out)
{
    Reader r = reader_init(data, size);
    Header header;
    if (!read_header(&r, &header) || header.bits != 24) {
        return false;
    }
    /* A row is 3 bytes for each pixel, filled out to a multiple of 4. */
    size_t stride = ((size_t)header.width * 3 + 3) / 4 * 4;
    if (header.pixels_at > size
        || stride > (size - header.pixels_at) / header.height) {
        return false;
    }
    Bmp24 bmp;
    bmp.width = header.width;
    bmp.height = header.height;
    bmp.pixels = malloc((size_t)bmp.width * bmp.height * sizeof *bmp.pixels);
    if (bmp.pixels == NULL) {
        return false;
    }
    /* The file's first row is the picture's last, and a pixel is blue,
     * green, red. */
    for (uint32_t y = 0; y < bmp.height; y++) {
        const uint8_t *row = data + header.pixels_at
            + (size_t)(bmp.height - 1 - y) * stride;
        Rgb *to = bmp.pixels + (size_t)y * bmp.width;
        for (uint32_t x = 0; x < bmp.width; x++) {
            to[x].b = row[x * 3];
            to[x].g = row[x * 3 + 1];
            to[x].r = row[x * 3 + 2];
        }
    }
    *out = bmp;
    return true;
}

void bmp24_free(Bmp24 *bmp)
{
    free(bmp->pixels);
    memset(bmp, 0, sizeof *bmp);
}
