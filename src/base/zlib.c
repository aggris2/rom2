#include "base/zlib.h"

#include <stdlib.h>
#include <string.h>

#include "base/checksum.h"
#include "base/reader.h"

/* ---- What the deflate format fixes ---- */

/* Packed data is a row of symbols. Symbols 0 to 255 are plain bytes, 256
 * ends a block, and 257 to 285 mean "repeat bytes that came before": a
 * length, followed by how far back to go. */
#define SYMBOL_END 256
#define SYMBOL_FIRST_LENGTH 257
#define LENGTH_CODES 29
#define DISTANCE_CODES 30
#define MAX_LITERAL_CODES 288
#define MAX_BITS 15

#define MIN_MATCH 3
#define MAX_MATCH 258
#define MAX_DISTANCE 32768

/* A length symbol stands for a base length plus the value of some extra
 * bits that follow it. Distances work the same way. */
static const uint16_t LENGTH_BASE[LENGTH_CODES] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t LENGTH_EXTRA[LENGTH_CODES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const uint16_t DISTANCE_BASE[DISTANCE_CODES] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};
static const uint8_t DISTANCE_EXTRA[DISTANCE_CODES] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

/* ---- Packing ---- */

/* Collects bits and adds them to a buffer a byte at a time. The first bit
 * written goes into the lowest bit of a byte. Once an add has failed,
 * `ok` stays false. */
typedef struct BitWriter {
    ByteBuffer *out;
    uint32_t bits;  /* bits waiting to be written, lowest first */
    int count;      /* how many are waiting; less than 8 between calls */
    bool ok;
} BitWriter;

/* Writes the lowest `count` bits of `value`, lowest bit first. `count` is
 * at most 16. */
static void write_bits(BitWriter *w, uint32_t value, int count)
{
    w->bits |= value << w->count;
    w->count += count;
    while (w->count >= 8) {
        w->ok = w->ok && buffer_push(w->out, (uint8_t)(w->bits & 0xff));
        w->bits >>= 8;
        w->count -= 8;
    }
}

/* Writes a Huffman code. Unlike everything else, these go in highest bit
 * first, so the bits are turned round on the way. */
static void write_code(BitWriter *w, uint32_t code, int length)
{
    uint32_t reversed = 0;
    for (int i = 0; i < length; i++) {
        reversed = (reversed << 1) | ((code >> i) & 1u);
    }
    write_bits(w, reversed, length);
}

/* Writes one symbol with the codes the format fixes for blocks that carry
 * no code table of their own. */
static void write_symbol(BitWriter *w, uint32_t symbol)
{
    if (symbol <= 143) {
        write_code(w, 0x30 + symbol, 8);
    } else if (symbol <= 255) {
        write_code(w, 0x190 + (symbol - 144), 9);
    } else if (symbol <= 279) {
        write_code(w, symbol - 256, 7);
    } else {
        write_code(w, 0xc0 + (symbol - 280), 8);
    }
}

/* Writes "repeat `length` bytes from `distance` bytes back". */
static void write_match(BitWriter *w, uint32_t length, uint32_t distance)
{
    uint32_t code = LENGTH_CODES - 1;
    while (LENGTH_BASE[code] > length) {
        code--;
    }
    write_symbol(w, SYMBOL_FIRST_LENGTH + code);
    write_bits(w, length - LENGTH_BASE[code], LENGTH_EXTRA[code]);

    code = DISTANCE_CODES - 1;
    while (DISTANCE_BASE[code] > distance) {
        code--;
    }
    write_code(w, code, 5);
    write_bits(w, distance - DISTANCE_BASE[code], DISTANCE_EXTRA[code]);
}

/* To find repeats, the packer remembers where it last saw each group of
 * three bytes. The three bytes are mixed into a table position. */
#define SEEN_BITS 15
#define SEEN_SIZE (1u << SEEN_BITS)

static uint32_t seen_slot(const uint8_t *p)
{
    uint32_t three = (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16);
    return (three * 2654435761u) >> (32 - SEEN_BITS);
}

bool zlib_compress(ByteBuffer *out, const uint8_t *data, size_t size)
{
    /* For each slot, one more than the position where those three bytes
     * were last seen, or 0 for "not seen". */
    size_t *seen = calloc(SEEN_SIZE, sizeof *seen);
    if (seen == NULL) {
        return false;
    }

    size_t size_before = out->size;
    BitWriter w = {.out = out, .bits = 0, .count = 0, .ok = true};

    /* The zlib header: "deflate, with a window of 32768 bytes". The two
     * bytes read as one number must divide by 31. */
    w.ok = buffer_push(out, 0x78) && buffer_push(out, 0x01);

    /* One block, marked as the last, using the fixed codes. */
    write_bits(&w, 1, 1);
    write_bits(&w, 1, 2);

    size_t pos = 0;
    while (pos < size && w.ok) {
        size_t length = 0;
        size_t distance = 0;

        /* Is there an earlier place that starts with the same three
         * bytes? If so, count how many bytes match from there. */
        if (size - pos >= MIN_MATCH) {
            uint32_t slot = seen_slot(data + pos);
            size_t earlier = seen[slot];
            seen[slot] = pos + 1;
            if (earlier != 0 && pos - (earlier - 1) <= MAX_DISTANCE) {
                const uint8_t *a = data + (earlier - 1);
                const uint8_t *b = data + pos;
                size_t limit = size - pos;
                if (limit > MAX_MATCH) {
                    limit = MAX_MATCH;
                }
                while (length < limit && a[length] == b[length]) {
                    length++;
                }
                distance = pos - (earlier - 1);
            }
        }

        if (length >= MIN_MATCH) {
            write_match(&w, (uint32_t)length, (uint32_t)distance);
            /* Remember the places inside the repeat as well, so that
             * later data can refer to them. */
            for (size_t i = 1; i < length; i++) {
                if (size - (pos + i) >= MIN_MATCH) {
                    seen[seen_slot(data + pos + i)] = pos + i + 1;
                }
            }
            pos += length;
        } else {
            write_symbol(&w, data[pos]);
            pos += 1;
        }
    }

    write_symbol(&w, SYMBOL_END);
    write_bits(&w, 0, 7); /* push out the bits of the last, partial byte */
    free(seen);

    bool ok = w.ok && buffer_u32_be(out,
        adler32_update(ADLER32_START, data, size));
    if (!ok) {
        out->size = size_before;
    }
    return ok;
}

/* ---- Unpacking ---- */

/* Hands out bits from a Reader, lowest bit of each byte first. Running
 * out of bytes shows as a failed reader, and the bits read are then 0. */
typedef struct BitReader {
    Reader r;
    uint32_t bits; /* bits taken from the reader and not handed out yet */
    int count;     /* how many of those there are */
} BitReader;

/* Reads `count` bits, at most 16, as a number: first bit lowest. */
static uint32_t read_bits(BitReader *b, int count)
{
    while (b->count < count) {
        b->bits |= (uint32_t)reader_u8(&b->r) << b->count;
        b->count += 8;
    }
    uint32_t value = b->bits & ((1u << count) - 1u);
    b->bits >>= count;
    b->count -= count;
    return value;
}

/* A set of Huffman codes, kept the way that makes reading one easy:
 * how many codes there are of each length, and the symbols in the order
 * of their codes. */
typedef struct Codes {
    uint16_t count[MAX_BITS + 1];
    uint16_t symbol[MAX_LITERAL_CODES];
} Codes;

/* Builds a set of codes from the length of each symbol's code, where 0
 * means the symbol is not used. Returns false if the lengths describe
 * more codes than can exist. */
static bool codes_build(Codes *c, const uint8_t *lengths, int symbols)
{
    memset(c->count, 0, sizeof c->count);
    for (int i = 0; i < symbols; i++) {
        c->count[lengths[i]]++;
    }
    c->count[0] = 0;

    /* With each extra bit of length twice as many codes become possible,
     * and each code in use takes one away. */
    int unused = 1;
    for (int length = 1; length <= MAX_BITS; length++) {
        unused = unused * 2 - c->count[length];
        if (unused < 0) {
            return false;
        }
    }

    /* Where the symbols of each length start in the sorted list. */
    uint16_t start[MAX_BITS + 2];
    start[1] = 0;
    for (int length = 1; length <= MAX_BITS; length++) {
        start[length + 1] = (uint16_t)(start[length] + c->count[length]);
    }
    for (int i = 0; i < symbols; i++) {
        if (lengths[i] != 0) {
            c->symbol[start[lengths[i]]++] = (uint16_t)i;
        }
    }
    return true;
}

/* Reads one symbol. Returns -1 if the bits are not a code in this set. */
static int codes_read(BitReader *b, const Codes *c)
{
    int code = 0;  /* the bits read so far, as a number */
    int first = 0; /* the first code of the current length */
    int index = 0; /* where that length's symbols start */
    for (int length = 1; length <= MAX_BITS; length++) {
        code |= (int)read_bits(b, 1);
        int count = c->count[length];
        if (code - count < first) {
            return c->symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

/* What is being produced: the output buffer, where this stream's bytes
 * start in it, and how many it may hold at most. */
typedef struct Output {
    ByteBuffer *out;
    size_t start;
    size_t max_size;
} Output;

static size_t produced(const Output *o)
{
    return o->out->size - o->start;
}

/* A block with no packing: its length twice, the second time with every
 * bit flipped, and then the bytes. It starts on a byte boundary. */
static bool unpack_stored(BitReader *b, Output *o)
{
    b->bits = 0;
    b->count = 0;
    uint16_t length = reader_u16(&b->r);
    uint16_t flipped = reader_u16(&b->r);
    const uint8_t *bytes = reader_bytes(&b->r, length);
    if (!reader_ok(&b->r) || (uint16_t)~length != flipped
        || length > o->max_size - produced(o)) {
        return false;
    }
    return buffer_append(o->out, bytes, length);
}

/* The symbols of one block, read with the given codes until the symbol
 * that ends the block. */
static bool unpack_symbols(BitReader *b, Output *o, const Codes *literals,
    const Codes *distances)
{
    for (;;) {
        int symbol = codes_read(b, literals);
        if (symbol < 0 || !reader_ok(&b->r)) {
            return false;
        }
        if (symbol == SYMBOL_END) {
            return true;
        }
        if (symbol < SYMBOL_END) {
            if (produced(o) >= o->max_size
                || !buffer_push(o->out, (uint8_t)symbol)) {
                return false;
            }
            continue;
        }

        /* A repeat: a length, then how far back to copy from. */
        int length_code = symbol - SYMBOL_FIRST_LENGTH;
        if (length_code >= LENGTH_CODES) {
            return false;
        }
        size_t length = LENGTH_BASE[length_code]
            + read_bits(b, LENGTH_EXTRA[length_code]);

        int distance_code = codes_read(b, distances);
        if (distance_code < 0 || distance_code >= DISTANCE_CODES) {
            return false;
        }
        size_t distance = DISTANCE_BASE[distance_code]
            + read_bits(b, DISTANCE_EXTRA[distance_code]);

        if (!reader_ok(&b->r) || distance > produced(o)
            || length > o->max_size - produced(o)) {
            return false;
        }
        /* One byte at a time: the bytes being copied may include ones
         * this same copy has just written. */
        for (size_t i = 0; i < length; i++) {
            uint8_t byte = o->out->data[o->out->size - distance];
            if (!buffer_push(o->out, byte)) {
                return false;
            }
        }
    }
}

/* A block that uses the codes the format fixes. */
static bool unpack_fixed(BitReader *b, Output *o)
{
    uint8_t lengths[MAX_LITERAL_CODES];
    Codes literals;
    Codes distances;

    for (int i = 0; i < MAX_LITERAL_CODES; i++) {
        lengths[i] = (uint8_t)(i <= 143 ? 8 : i <= 255 ? 9 : i <= 279 ? 7 : 8);
    }
    codes_build(&literals, lengths, MAX_LITERAL_CODES);
    for (int i = 0; i < DISTANCE_CODES; i++) {
        lengths[i] = 5;
    }
    codes_build(&distances, lengths, DISTANCE_CODES);

    return unpack_symbols(b, o, &literals, &distances);
}

/* A block that starts with its own code tables. The tables are a list of
 * code lengths, and that list is itself packed with a third, small set of
 * codes. */
static bool unpack_dynamic(BitReader *b, Output *o)
{
    /* The order in which the lengths of the small set are stored. */
    static const uint8_t ORDER[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    int literal_count = (int)read_bits(b, 5) + 257;
    int distance_count = (int)read_bits(b, 5) + 1;
    int small_count = (int)read_bits(b, 4) + 4;
    if (!reader_ok(&b->r) || literal_count > 286
        || distance_count > DISTANCE_CODES) {
        return false;
    }

    uint8_t lengths[MAX_LITERAL_CODES + DISTANCE_CODES];
    memset(lengths, 0, sizeof lengths);
    for (int i = 0; i < small_count; i++) {
        lengths[ORDER[i]] = (uint8_t)read_bits(b, 3);
    }
    Codes small;
    if (!codes_build(&small, lengths, 19)) {
        return false;
    }

    /* The lengths of the literal codes and the distance codes, as one
     * list. Three special symbols repeat a length several times. */
    int total = literal_count + distance_count;
    int index = 0;
    while (index < total) {
        int symbol = codes_read(b, &small);
        if (symbol < 0 || !reader_ok(&b->r)) {
            return false;
        }
        if (symbol < 16) {
            lengths[index++] = (uint8_t)symbol;
            continue;
        }

        uint8_t repeated = 0;
        int times;
        if (symbol == 16) {
            if (index == 0) {
                return false;
            }
            repeated = lengths[index - 1];
            times = 3 + (int)read_bits(b, 2);
        } else if (symbol == 17) {
            times = 3 + (int)read_bits(b, 3);
        } else {
            times = 11 + (int)read_bits(b, 7);
        }
        if (times > total - index) {
            return false;
        }
        for (int i = 0; i < times; i++) {
            lengths[index++] = repeated;
        }
    }
    if (!reader_ok(&b->r) || lengths[SYMBOL_END] == 0) {
        return false;
    }

    Codes literals;
    Codes distances;
    if (!codes_build(&literals, lengths, literal_count)
        || !codes_build(&distances, lengths + literal_count,
            distance_count)) {
        return false;
    }
    return unpack_symbols(b, o, &literals, &distances);
}

bool zlib_decompress(ByteBuffer *out, const uint8_t *data, size_t size,
    size_t max_size)
{
    BitReader b = {.r = reader_init(data, size), .bits = 0, .count = 0};
    Output o = {.out = out, .start = out->size, .max_size = max_size};

    /* The header: the method must be deflate, the two bytes as one number
     * must divide by 31, and there must be no preset dictionary. */
    uint32_t method = reader_u8(&b.r);
    uint32_t flags = reader_u8(&b.r);
    bool ok = reader_ok(&b.r)
        && (method & 0x0f) == 8
        && ((method << 8) | flags) % 31 == 0
        && (flags & 0x20) == 0;

    bool last = false;
    while (ok && !last) {
        last = read_bits(&b, 1) == 1;
        uint32_t kind = read_bits(&b, 2);
        if (!reader_ok(&b.r)) {
            ok = false;
        } else if (kind == 0) {
            ok = unpack_stored(&b, &o);
        } else if (kind == 1) {
            ok = unpack_fixed(&b, &o);
        } else if (kind == 2) {
            ok = unpack_dynamic(&b, &o);
        } else {
            ok = false;
        }
    }

    /* The checksum follows on the next byte boundary, high byte first. */
    if (ok) {
        uint32_t stored = 0;
        for (int i = 0; i < 4; i++) {
            stored = (stored << 8) | reader_u8(&b.r);
        }
        uint32_t actual = adler32_update(ADLER32_START,
            out->size > o.start ? out->data + o.start : NULL, produced(&o));
        ok = reader_ok(&b.r) && stored == actual;
    }

    if (!ok) {
        out->size = o.start;
    }
    return ok;
}
