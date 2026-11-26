#include "import/png.h"

#include "base/checksum.h"
#include "base/zlib.h"

/* Adds one chunk: the size of its data, its four-letter type, the data,
 * and a CRC of the type and the data. Numbers in a PNG are stored high
 * byte first. */
static bool add_chunk(ByteBuffer *out, const char type[4],
    const uint8_t *data, size_t size)
{
    uint32_t crc = crc32_update(CRC32_START, (const uint8_t *)type, 4);
    crc = crc32_update(crc, data, size);

    return buffer_u32_be(out, (uint32_t)size)
        && buffer_append(out, (const uint8_t *)type, 4)
        && buffer_append(out, data, size)
        && buffer_u32_be(out, crc);
}

/* IHDR: the size of the picture and how its pixels are stored. */
static bool add_header(ByteBuffer *out, uint32_t width, uint32_t height)
{
    ByteBuffer data = {0};
    bool ok = buffer_u32_be(&data, width)
        && buffer_u32_be(&data, height)
        && buffer_push(&data, 8)  /* bits per pixel */
        && buffer_push(&data, 3)  /* colour type 3: palette indices */
        && buffer_push(&data, 0)  /* compression method: the only one */
        && buffer_push(&data, 0)  /* filter method: the only one */
        && buffer_push(&data, 0)  /* not interlaced */
        && add_chunk(out, "IHDR", data.data, data.size);
    buffer_free(&data);
    return ok;
}

/* PLTE: the 256 colours, three bytes each, red first. */
static bool add_palette(ByteBuffer *out, const Rgb palette[256])
{
    uint8_t data[256 * 3];
    for (size_t i = 0; i < 256; i++) {
        data[i * 3] = palette[i].r;
        data[i * 3 + 1] = palette[i].g;
        data[i * 3 + 2] = palette[i].b;
    }
    return add_chunk(out, "PLTE", data, sizeof data);
}

/* tRNS: how see-through each palette index is, starting from index 0.
 * Indices that are left out are solid, so one byte is enough to make
 * index 0 transparent and nothing else. */
static bool add_transparency(ByteBuffer *out)
{
    const uint8_t data[1] = {0};
    return add_chunk(out, "tRNS", data, sizeof data);
}

/* IDAT: the pixels, packed as a zlib stream.
 *
 * What gets packed is each row of pixels with one extra byte in front of
 * it, the row's "filter", which is 0 for "these are the pixels as they
 * are". */
static bool add_pixels(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels)
{
    ByteBuffer rows = {0};
    ByteBuffer stream = {0};
    bool ok = true;

    for (uint32_t y = 0; ok && y < height; y++) {
        ok = buffer_push(&rows, 0)
            && buffer_append(&rows, pixels + (size_t)y * width, width);
    }
    ok = ok
        && zlib_compress(&stream, rows.data, rows.size)
        && add_chunk(out, "IDAT", stream.data, stream.size);

    buffer_free(&rows);
    buffer_free(&stream);
    return ok;
}

bool png_write_indexed(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels, const Rgb palette[256])
{
    /* Every PNG file starts with these eight bytes. */
    static const uint8_t signature[8] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
    };

    if (width == 0 || width > PNG_MAX_SIDE
        || height == 0 || height > PNG_MAX_SIDE) {
        return false;
    }

    size_t size_before = out->size;
    bool ok = buffer_append(out, signature, sizeof signature)
        && add_header(out, width, height)
        && add_palette(out, palette)
        && add_transparency(out)
        && add_pixels(out, width, height, pixels)
        && add_chunk(out, "IEND", NULL, 0);
    if (!ok) {
        /* Take back whatever part of the file was added. */
        out->size = size_before;
    }
    return ok;
}
