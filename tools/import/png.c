#include "import/png.h"

#include "base/pack.h"

/* Adds one chunk: the size of its data, its four-letter type, the data,
 * and a CRC of the type and the data. Numbers in a PNG are stored high
 * byte first. */
static bool add_chunk(ByteBuffer *out, const char type[4],
    const uint8_t *data, size_t size)
{
    uint32_t crc = pack_crc32(PACK_CRC32_START, (const uint8_t *)type, 4);
    crc = pack_crc32(crc, data, size);

    return buffer_u32_be(out, (uint32_t)size)
        && buffer_append(out, (const uint8_t *)type, 4)
        && buffer_append(out, data, size)
        && buffer_u32_be(out, crc);
}

/* Colour types: what the bytes of a pixel are. */
#define COLOUR_RGB 2u      /* three bytes: red, green, blue */
#define COLOUR_INDEXED 3u  /* one byte: an index into the palette */

/* Every PNG file starts with these eight bytes. */
static const uint8_t SIGNATURE[8] = {
    0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
};

/* IHDR: the size of the picture and how its pixels are stored. */
static bool add_header(ByteBuffer *out, uint32_t width, uint32_t height,
    uint8_t colour_type)
{
    ByteBuffer data = {0};
    bool ok = buffer_u32_be(&data, width)
        && buffer_u32_be(&data, height)
        && buffer_push(&data, 8)  /* bits for each number of a pixel */
        && buffer_push(&data, colour_type)
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
 * are". `row_size` is the number of bytes in one row of `pixels`. */
static bool add_pixels(ByteBuffer *out, size_t row_size, uint32_t height,
    const uint8_t *pixels)
{
    ByteBuffer rows = {0};
    ByteBuffer stream = {0};
    bool ok = true;

    for (uint32_t y = 0; ok && y < height; y++) {
        ok = buffer_push(&rows, 0)
            && buffer_append(&rows, pixels + (size_t)y * row_size, row_size);
    }
    ok = ok
        && pack_zlib(&stream, rows.data, rows.size)
        && add_chunk(out, "IDAT", stream.data, stream.size);

    buffer_free(&rows);
    buffer_free(&stream);
    return ok;
}

bool png_write_indexed(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels, const Rgb palette[256])
{
    if (width == 0 || width > PNG_MAX_SIDE
        || height == 0 || height > PNG_MAX_SIDE) {
        return false;
    }

    size_t size_before = out->size;
    bool ok = buffer_append(out, SIGNATURE, sizeof SIGNATURE)
        && add_header(out, width, height, COLOUR_INDEXED)
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

bool png_write_rgb(ByteBuffer *out, uint32_t width, uint32_t height,
    const Rgb *pixels)
{
    if (width == 0 || width > PNG_MAX_SIDE
        || height == 0 || height > PNG_MAX_SIDE) {
        return false;
    }

    /* The pixels as the file wants them: three bytes each, red first. */
    size_t count = (size_t)width * height;
    ByteBuffer bytes = {0};
    bool ok = true;
    for (size_t i = 0; ok && i < count; i++) {
        ok = buffer_push(&bytes, pixels[i].r)
            && buffer_push(&bytes, pixels[i].g)
            && buffer_push(&bytes, pixels[i].b);
    }

    size_t size_before = out->size;
    ok = ok
        && buffer_append(out, SIGNATURE, sizeof SIGNATURE)
        && add_header(out, width, height, COLOUR_RGB)
        && add_pixels(out, (size_t)width * 3, height, bytes.data)
        && add_chunk(out, "IEND", NULL, 0);
    if (!ok) {
        out->size = size_before;
    }
    buffer_free(&bytes);
    return ok;
}
