#include "import/pal.h"

#include "base/reader.h"

#define PALETTE_SIZE 1024u
#define BMP_FILE_HEADER_SIZE 14u
#define BMP_INFO_HEADER_SIZE 40u

/* Reads 256 colours of 4 bytes: blue, green, red, unused. */
static bool read_colours(Reader *r, Rgb out[256])
{
    const uint8_t *bytes = reader_bytes(r, PALETTE_SIZE);
    if (!reader_ok(r)) {
        return false;
    }
    for (size_t i = 0; i < 256; i++) {
        out[i].b = bytes[i * 4];
        out[i].g = bytes[i * 4 + 1];
        out[i].r = bytes[i * 4 + 2];
    }
    return true;
}

bool pal_bmp_read(const uint8_t *data, size_t size, Rgb out[256])
{
    Reader r = reader_init(data, size);

    /* The file header: "BM", the file's size, 4 unused bytes, and where
     * the pixels start. */
    uint8_t b = reader_u8(&r);
    uint8_t m = reader_u8(&r);
    reader_skip(&r, 12);

    /* The picture header: its own size, the picture's width and height,
     * the number of planes, the bits for each pixel, the compression, 12
     * bytes that do not matter here, and the number of colours in the
     * table, where 0 stands for all 256. */
    uint32_t header_size = reader_u32(&r);
    reader_skip(&r, 10);
    uint16_t bits = reader_u16(&r);
    reader_skip(&r, 16);
    uint32_t colours = reader_u32(&r);
    reader_skip(&r, 4);
    if (!reader_ok(&r) || b != 'B' || m != 'M'
        || header_size < BMP_INFO_HEADER_SIZE || bits != 8
        || (colours != 0 && colours != 256)) {
        return false;
    }

    /* The colour table comes right after the picture header, which may
     * be longer than the 40 bytes read so far. */
    reader_skip(&r, header_size - BMP_INFO_HEADER_SIZE);
    return read_colours(&r, out);
}

uint32_t pal_table_count(size_t size)
{
    if (size % PALETTE_SIZE != 0 || size / PALETTE_SIZE > UINT32_MAX) {
        return 0;
    }
    return (uint32_t)(size / PALETTE_SIZE);
}

bool pal_table_read(const uint8_t *data, size_t size, uint32_t index,
    Rgb out[256])
{
    if (index >= pal_table_count(size)) {
        return false;
    }
    Reader r = reader_init(data + (size_t)index * PALETTE_SIZE,
        PALETTE_SIZE);
    return read_colours(&r, out);
}
