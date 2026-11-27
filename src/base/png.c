#include "base/png.h"

#include <stdlib.h>
#include <string.h>

#include "base/pack.h"
#include "base/reader.h"

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
#define COLOUR_GREY 0u     /* one byte: a shade from black to white */

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

/* Writes an indexed picture, with index 0 transparent or not. */
static bool write_indexed(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels, const Rgb palette[256], bool transparent)
{
    if (width == 0 || width > PNG_MAX_SIDE
        || height == 0 || height > PNG_MAX_SIDE) {
        return false;
    }

    size_t size_before = out->size;
    bool ok = buffer_append(out, SIGNATURE, sizeof SIGNATURE)
        && add_header(out, width, height, COLOUR_INDEXED)
        && add_palette(out, palette)
        && (!transparent || add_transparency(out))
        && add_pixels(out, width, height, pixels)
        && add_chunk(out, "IEND", NULL, 0);
    if (!ok) {
        /* Take back whatever part of the file was added. */
        out->size = size_before;
    }
    return ok;
}

bool png_write_indexed(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels, const Rgb palette[256])
{
    return write_indexed(out, width, height, pixels, palette, true);
}

bool png_write_indexed_solid(ByteBuffer *out, uint32_t width,
    uint32_t height, const uint8_t *pixels, const Rgb palette[256])
{
    return write_indexed(out, width, height, pixels, palette, false);
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

bool png_write_grey(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels)
{
    if (width == 0 || width > PNG_MAX_SIDE
        || height == 0 || height > PNG_MAX_SIDE) {
        return false;
    }
    size_t size_before = out->size;
    bool ok = buffer_append(out, SIGNATURE, sizeof SIGNATURE)
        && add_header(out, width, height, COLOUR_GREY)
        && add_pixels(out, width, height, pixels)
        && add_chunk(out, "IEND", NULL, 0);
    if (!ok) {
        out->size = size_before;
    }
    return ok;
}

/* ---- Reading ---- */

/* One chunk of a file: its four-letter type and its data. */
typedef struct Chunk {
    const uint8_t *type;
    const uint8_t *data;
    uint32_t size;
} Chunk;

/* Reads a number of 32 bits, high byte first. */
static uint32_t read_u32_be(Reader *r)
{
    uint32_t b0 = reader_u8(r);
    uint32_t b1 = reader_u8(r);
    uint32_t b2 = reader_u8(r);
    uint32_t b3 = reader_u8(r);
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

/* Reads the chunk the reader stands at and checks its CRC. */
static bool read_chunk(Reader *r, Chunk *out)
{
    uint32_t size = read_u32_be(r);
    const uint8_t *type = reader_bytes(r, 4);
    const uint8_t *data = reader_bytes(r, size);
    uint32_t crc = read_u32_be(r);
    if (!reader_ok(r)) {
        return false;
    }
    if (crc != pack_crc32(pack_crc32(PACK_CRC32_START, type, 4), data,
            size)) {
        return false;
    }
    out->type = type;
    out->data = data;
    out->size = size;
    return true;
}

static bool chunk_is(const Chunk *chunk, const char *type)
{
    return memcmp(chunk->type, type, 4) == 0;
}

/* PNG's guess at a byte from the three before it: the one of left, up
 * and up-left that is nearest to left + up - up-left. */
static uint8_t paeth(uint8_t left, uint8_t up, uint8_t up_left)
{
    int p = (int)left + (int)up - (int)up_left;
    int from_left = abs(p - (int)left);
    int from_up = abs(p - (int)up);
    int from_up_left = abs(p - (int)up_left);
    if (from_left <= from_up && from_left <= from_up_left) {
        return left;
    }
    return from_up <= from_up_left ? up : up_left;
}

/* Undoes the way the rows were stored. `rows` is `height` rows, each a
 * byte that says how and then `row_size` bytes. The plain bytes go to
 * `pixels`. `step` is the bytes of a pixel: a byte is stored as its
 * difference from the same byte of the pixel before, the row above, or
 * both. Returns false for a way that does not exist. */
static bool unfilter(const uint8_t *rows, uint32_t height, size_t row_size,
    size_t step, uint8_t *pixels)
{
    for (uint32_t y = 0; y < height; y++) {
        const uint8_t *in = rows + (size_t)y * (row_size + 1);
        uint8_t *row = pixels + (size_t)y * row_size;
        const uint8_t *above = y > 0 ? row - row_size : NULL;
        uint8_t how = in[0];
        if (how > 4) {
            return false;
        }
        for (size_t x = 0; x < row_size; x++) {
            uint8_t left = x >= step ? row[x - step] : 0;
            uint8_t up = above != NULL ? above[x] : 0;
            uint8_t up_left = above != NULL && x >= step
                ? above[x - step] : 0;
            uint8_t guess = 0;
            if (how == 1) {
                guess = left;
            } else if (how == 2) {
                guess = up;
            } else if (how == 3) {
                guess = (uint8_t)(((unsigned)left + (unsigned)up) / 2);
            } else if (how == 4) {
                guess = paeth(left, up, up_left);
            }
            row[x] = (uint8_t)(in[1 + x] + guess);
        }
    }
    return true;
}

bool png_read(const uint8_t *data, size_t size, Png *out)
{
    Reader r = reader_init(data, size);
    const uint8_t *signature = reader_bytes(&r, sizeof SIGNATURE);
    Chunk chunk;
    if (signature == NULL
        || memcmp(signature, SIGNATURE, sizeof SIGNATURE) != 0
        || !read_chunk(&r, &chunk) || !chunk_is(&chunk, "IHDR")
        || chunk.size != 13) {
        return false;
    }

    /* The header: the size, 8 bits, the kind, and three bytes that are
     * 0 in every file this reads: the packing, the row storing, and
     * no interlacing. */
    Reader header = reader_init(chunk.data, chunk.size);
    Png png;
    memset(&png, 0, sizeof png);
    memset(png.alpha, 255, sizeof png.alpha);
    png.width = read_u32_be(&header);
    png.height = read_u32_be(&header);
    uint8_t depth = reader_u8(&header);
    uint8_t colour = reader_u8(&header);
    uint8_t packing = reader_u8(&header);
    uint8_t storing = reader_u8(&header);
    uint8_t interlace = reader_u8(&header);
    if (png.width == 0 || png.height == 0 || png.width > PNG_MAX_SIDE
        || png.height > PNG_MAX_SIDE || depth != 8 || packing != 0
        || storing != 0 || interlace != 0) {
        return false;
    }
    size_t step = 1;
    if (colour == COLOUR_INDEXED) {
        png.kind = PNG_INDEXED;
    } else if (colour == COLOUR_GREY) {
        png.kind = PNG_GREY;
    } else if (colour == COLOUR_RGB) {
        png.kind = PNG_RGB;
        step = 3;
    } else {
        return false;
    }

    /* The other chunks, up to the one that ends the file. The packed
     * pixels may be cut into several chunks, which are joined. A chunk
     * of a kind this does not know is passed over. */
    ByteBuffer packed = {0};
    bool has_palette = false;
    bool ended = false;
    bool ok = true;
    while (ok && !ended) {
        ok = read_chunk(&r, &chunk);
        if (!ok) {
            break;
        }
        if (chunk_is(&chunk, "IEND")) {
            ended = true;
        } else if (chunk_is(&chunk, "IDAT")) {
            ok = buffer_append(&packed, chunk.data, chunk.size);
        } else if (chunk_is(&chunk, "PLTE")) {
            ok = chunk.size % 3 == 0 && chunk.size <= 768;
            for (uint32_t i = 0; ok && i < chunk.size / 3; i++) {
                png.palette[i].r = chunk.data[i * 3];
                png.palette[i].g = chunk.data[i * 3 + 1];
                png.palette[i].b = chunk.data[i * 3 + 2];
            }
            has_palette = true;
        } else if (chunk_is(&chunk, "tRNS") && png.kind == PNG_INDEXED) {
            ok = chunk.size <= 256;
            if (ok && chunk.size > 0) {
                memcpy(png.alpha, chunk.data, chunk.size);
            }
        }
    }
    ok = ok && ended && (png.kind != PNG_INDEXED || has_palette);

    /* Unpacked, the pixels are the rows one after the other, each with
     * a byte before it that says how the row is stored. */
    size_t row_size = (size_t)png.width * step;
    size_t rows_size = (row_size + 1) * png.height;
    ByteBuffer rows = {0};
    ok = ok && unpack_zlib(&rows, packed.data, packed.size, rows_size)
        && rows.size == rows_size;
    if (ok) {
        png.pixels = malloc(row_size * png.height);
        ok = png.pixels != NULL
            && unfilter(rows.data, png.height, row_size, step, png.pixels);
    }
    buffer_free(&packed);
    buffer_free(&rows);
    if (!ok) {
        free(png.pixels);
        return false;
    }
    *out = png;
    return true;
}

void png_free(Png *png)
{
    free(png->pixels);
    memset(png, 0, sizeof *png);
}
