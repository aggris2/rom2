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

/* Fills the last byte with zero bits, so that what follows starts on a
 * byte boundary. */
static void write_to_byte_boundary(BitWriter *w)
{
    if (w->count > 0) {
        write_bits(w, 0, 8 - w->count);
    }
}

/* One step of the packed data: either a plain byte, or "repeat `length`
 * bytes from `distance` bytes back". A plain byte has a length of 0 and
 * keeps its value in `distance`. */
typedef struct Token {
    uint16_t length;
    uint16_t distance;
} Token;

/* The kinds of block a deflate stream is made of. */
#define BLOCK_STORED 0u
#define BLOCK_FIXED 1u
#define BLOCK_DYNAMIC 2u

/* A block covers at most this many bytes of input, which is the most a
 * stored block can hold. Every block can then be written as stored if
 * packing does not make it smaller. */
#define BLOCK_MAX_BYTES 65535u

/* The symbol and extra bits that stand for a length, and for a distance. */
static int length_code(uint32_t length)
{
    int code = LENGTH_CODES - 1;
    while (LENGTH_BASE[code] > length) {
        code--;
    }
    return code;
}

static int distance_code(uint32_t distance)
{
    int code = DISTANCE_CODES - 1;
    while (DISTANCE_BASE[code] > distance) {
        code--;
    }
    return code;
}

/* ---- Packing: finding repeats ---- */

/* To find repeats, the packer keeps a chain, for each group of three
 * bytes, of the places where it has seen them, newest first.
 *
 * `head` holds the newest place for each group and `prev` the place
 * before each place. Places are stored plus one, so that 0 can mean
 * "none". `prev` only has room for the last 32768 places, which is as far
 * back as a repeat can reach anyway. */
#define CHAIN_BITS 15
#define CHAIN_SLOTS (1u << CHAIN_BITS)
#define CHAIN_MASK (MAX_DISTANCE - 1u)

/* How many earlier places are tried for one repeat. More finds slightly
 * longer repeats and takes longer. */
#define CHAIN_TRIES 64

/* A repeat this long is taken at once, without looking for a better one
 * starting at the next byte. */
#define GOOD_ENOUGH 32u

typedef struct Finder {
    const uint8_t *data;
    size_t *head;
    size_t *prev;
} Finder;

static uint32_t chain_slot(const uint8_t *p)
{
    uint32_t three = (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16);
    return (three * 2654435761u) >> (32 - CHAIN_BITS);
}

/* Records that the three bytes at `pos` have been seen there. `end` is
 * where the data stops. */
static void finder_add(Finder *f, size_t pos, size_t end)
{
    if (end - pos < MIN_MATCH) {
        return;
    }
    uint32_t slot = chain_slot(f->data + pos);
    f->prev[pos & CHAIN_MASK] = f->head[slot];
    f->head[slot] = pos + 1;
}

/* Looks for the longest earlier run of bytes that matches the bytes at
 * `pos`, going no further than `end`. Returns its length, 0 if there is
 * none of at least 3 bytes, and stores how far back it is. */
static uint32_t finder_longest(const Finder *f, size_t pos, size_t end,
    uint32_t *out_distance)
{
    size_t limit = end - pos;
    if (limit > MAX_MATCH) {
        limit = MAX_MATCH;
    }
    if (limit < MIN_MATCH) {
        return 0;
    }

    size_t best = 0;
    size_t best_distance = 0;
    size_t place = f->head[chain_slot(f->data + pos)];
    for (int tries = 0; place != 0 && tries < CHAIN_TRIES; tries++) {
        size_t earlier = place - 1;
        if (earlier >= pos || pos - earlier > MAX_DISTANCE) {
            break;
        }
        const uint8_t *a = f->data + earlier;
        const uint8_t *b = f->data + pos;
        size_t length = 0;
        while (length < limit && a[length] == b[length]) {
            length++;
        }
        if (length > best) {
            best = length;
            best_distance = pos - earlier;
            if (best == limit) {
                break;
            }
        }
        /* The chain must keep going backwards. A place that does not is
         * a leftover from more than 32768 bytes ago. */
        size_t next = f->prev[earlier & CHAIN_MASK];
        if (next == 0 || next - 1 >= earlier) {
            break;
        }
        place = next;
    }

    if (best < MIN_MATCH) {
        return 0;
    }
    *out_distance = (uint32_t)best_distance;
    return (uint32_t)best;
}

/* Turns the bytes from `start` up to `end` into tokens. Returns how many
 * tokens there are. `tokens` has room for one per byte. */
static size_t make_tokens(Finder *f, size_t start, size_t end, Token *tokens)
{
    size_t count = 0;
    size_t pos = start;
    while (pos < end) {
        uint32_t distance = 0;
        uint32_t length = finder_longest(f, pos, end, &distance);
        finder_add(f, pos, end);

        /* A repeat that starts one byte later may be longer. If it is,
         * this byte goes out as a plain byte and the next round takes
         * the longer repeat. */
        if (length >= MIN_MATCH && length < GOOD_ENOUGH) {
            uint32_t next_distance = 0;
            if (finder_longest(f, pos + 1, end, &next_distance) > length) {
                length = 0;
            }
        }

        if (length >= MIN_MATCH) {
            tokens[count].length = (uint16_t)length;
            /* 32768 does not fit 16 bits as it is, so distances are
             * kept less one. */
            tokens[count].distance = (uint16_t)(distance - 1);
            for (size_t i = 1; i < length; i++) {
                finder_add(f, pos + i, end);
            }
            pos += length;
        } else {
            tokens[count].length = 0;
            tokens[count].distance = f->data[pos];
            pos += 1;
        }
        count++;
    }
    return count;
}

/* ---- Packing: choosing codes ---- */

/* Works out a code length for each symbol from how often it is used, so
 * that often-used symbols get short codes: Huffman's method. A symbol
 * that is not used gets length 0. No length is more than `max_bits`.
 * `symbols` is at most MAX_LITERAL_CODES. */
static void choose_lengths(const uint32_t *used, int symbols, int max_bits,
    uint8_t *lengths)
{
    /* The tree is built bottom up: the two least used nodes are joined
     * under a new node again and again until one is left. Nodes 0 to
     * symbols - 1 are the symbols, later ones the joins. */
    uint32_t weight[2 * MAX_LITERAL_CODES];
    int parent[2 * MAX_LITERAL_CODES];
    bool waiting[2 * MAX_LITERAL_CODES];
    uint32_t scaled[MAX_LITERAL_CODES];

    int in_use = 0;
    int only = 0;
    for (int i = 0; i < symbols; i++) {
        scaled[i] = used[i];
        lengths[i] = 0;
        if (used[i] > 0) {
            in_use++;
            only = i;
        }
    }
    if (in_use == 0) {
        return;
    }
    if (in_use == 1) {
        lengths[only] = 1;
        return;
    }

    for (;;) {
        int nodes = symbols;
        for (int i = 0; i < symbols; i++) {
            weight[i] = scaled[i];
            parent[i] = -1;
            waiting[i] = scaled[i] > 0;
        }
        for (int joins = 0; joins < in_use - 1; joins++) {
            int least = -1;
            int second = -1;
            for (int i = 0; i < nodes; i++) {
                if (!waiting[i]) {
                    continue;
                }
                if (least < 0 || weight[i] < weight[least]) {
                    second = least;
                    least = i;
                } else if (second < 0 || weight[i] < weight[second]) {
                    second = i;
                }
            }
            weight[nodes] = weight[least] + weight[second];
            parent[nodes] = -1;
            waiting[nodes] = true;
            waiting[least] = false;
            waiting[second] = false;
            parent[least] = nodes;
            parent[second] = nodes;
            nodes++;
        }

        /* A symbol's code length is how many joins lie above it. */
        int longest = 0;
        for (int i = 0; i < symbols; i++) {
            int depth = 0;
            for (int node = i; scaled[i] > 0 && parent[node] >= 0;
                node = parent[node]) {
                depth++;
            }
            lengths[i] = (uint8_t)depth;
            if (depth > longest) {
                longest = depth;
            }
        }
        if (longest <= max_bits) {
            return;
        }

        /* Too long a code means the counts are very uneven. Halving
         * them, never down to 0, evens them out, and trying again must
         * end: equal counts give a tree as flat as it can be. */
        for (int i = 0; i < symbols; i++) {
            if (scaled[i] > 0) {
                scaled[i] = (scaled[i] + 1) / 2;
            }
        }
    }
}

/* Gives each symbol its code, from the code lengths alone. The format
 * fixes how: shorter codes come first, and symbols of one length get
 * codes in the order of the symbols. */
static void assign_codes(const uint8_t *lengths, int symbols,
    uint16_t *codes)
{
    uint16_t count[MAX_BITS + 1] = {0};
    uint16_t next[MAX_BITS + 2] = {0};

    for (int i = 0; i < symbols; i++) {
        count[lengths[i]]++;
    }
    count[0] = 0;
    for (int length = 1; length <= MAX_BITS; length++) {
        next[length + 1] = (uint16_t)((next[length] + count[length]) << 1);
    }
    for (int i = 0; i < symbols; i++) {
        codes[i] = 0;
        if (lengths[i] != 0) {
            codes[i] = next[lengths[i]];
            next[lengths[i]]++;
        }
    }
}

/* The codes of one block: for plain bytes and lengths, and for
 * distances. */
typedef struct BlockCodes {
    uint8_t literal_lengths[MAX_LITERAL_CODES];
    uint16_t literal_codes[MAX_LITERAL_CODES];
    uint8_t distance_lengths[DISTANCE_CODES];
    uint16_t distance_codes[DISTANCE_CODES];
} BlockCodes;

/* The codes the format fixes for blocks that carry no tables. */
static void fixed_codes(BlockCodes *c)
{
    for (int i = 0; i < MAX_LITERAL_CODES; i++) {
        c->literal_lengths[i] =
            (uint8_t)(i <= 143 ? 8 : i <= 255 ? 9 : i <= 279 ? 7 : 8);
    }
    for (int i = 0; i < DISTANCE_CODES; i++) {
        c->distance_lengths[i] = 5;
    }
    assign_codes(c->literal_lengths, MAX_LITERAL_CODES, c->literal_codes);
    assign_codes(c->distance_lengths, DISTANCE_CODES, c->distance_codes);
}

/* How often each symbol is used by a block's tokens. */
typedef struct Usage {
    uint32_t literals[MAX_LITERAL_CODES];
    uint32_t distances[DISTANCE_CODES];
    uint32_t extra_bits; /* the extra bits of all lengths and distances */
} Usage;

static void count_usage(const Token *tokens, size_t count, Usage *u)
{
    memset(u, 0, sizeof *u);
    for (size_t i = 0; i < count; i++) {
        if (tokens[i].length == 0) {
            u->literals[tokens[i].distance]++;
        } else {
            int lc = length_code(tokens[i].length);
            int dc = distance_code((uint32_t)tokens[i].distance + 1);
            u->literals[SYMBOL_FIRST_LENGTH + lc]++;
            u->distances[dc]++;
            u->extra_bits += (uint32_t)LENGTH_EXTRA[lc] + DISTANCE_EXTRA[dc];
        }
    }
    u->literals[SYMBOL_END] = 1;
}

/* How many bits the tokens take with the given codes, tables not
 * counted. */
static uint32_t token_bits(const Usage *u, const BlockCodes *c)
{
    uint32_t bits = u->extra_bits;
    for (int i = 0; i < MAX_LITERAL_CODES; i++) {
        bits += u->literals[i] * c->literal_lengths[i];
    }
    for (int i = 0; i < DISTANCE_CODES; i++) {
        bits += u->distances[i] * c->distance_lengths[i];
    }
    return bits;
}

/* ---- Packing: the code tables of a block ---- */

/* A block with its own codes starts by listing the code length of every
 * symbol. That list is shortened with three special entries, 16, 17 and
 * 18, that repeat a length, and the result is itself written with a
 * third, small set of codes. */
#define SMALL_CODES 19
#define SMALL_MAX_BITS 7

/* The order in which the lengths of the small set are stored. */
static const uint8_t SMALL_ORDER[SMALL_CODES] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

/* One entry of the shortened list: a symbol of the small set, and the
 * value of the extra bits that follow 16, 17 and 18. */
typedef struct TableEntry {
    uint8_t symbol;
    uint8_t extra;
} TableEntry;

typedef struct Tables {
    int literal_count;  /* how many literal lengths are listed */
    int distance_count; /* how many distance lengths are listed */
    int small_count;    /* how many small-set lengths are listed */
    uint8_t small_lengths[SMALL_CODES];
    uint16_t small_codes[SMALL_CODES];
    TableEntry entries[MAX_LITERAL_CODES + DISTANCE_CODES];
    int entry_count;
    uint32_t bits;      /* the size of all of this when written */
} Tables;

static void add_entry(Tables *t, int symbol, int extra)
{
    t->entries[t->entry_count].symbol = (uint8_t)symbol;
    t->entries[t->entry_count].extra = (uint8_t)extra;
    t->entry_count++;
}

/* Prepares the tables for a block's codes. */
static void make_tables(const BlockCodes *c, Tables *t)
{
    memset(t, 0, sizeof *t);

    /* Lengths of 0 at the end of each list need not be written, but at
     * least 257 literal lengths and 1 distance length must be. */
    t->literal_count = 286;
    while (t->literal_count > 257
        && c->literal_lengths[t->literal_count - 1] == 0) {
        t->literal_count--;
    }
    t->distance_count = DISTANCE_CODES;
    while (t->distance_count > 1
        && c->distance_lengths[t->distance_count - 1] == 0) {
        t->distance_count--;
    }

    uint8_t lengths[MAX_LITERAL_CODES + DISTANCE_CODES];
    int total = t->literal_count + t->distance_count;
    memcpy(lengths, c->literal_lengths, (size_t)t->literal_count);
    memcpy(lengths + t->literal_count, c->distance_lengths,
        (size_t)t->distance_count);

    /* Shorten runs of one value. */
    int i = 0;
    while (i < total) {
        int run = 1;
        while (i + run < total && lengths[i + run] == lengths[i]) {
            run++;
        }
        int value = lengths[i];
        i += run;

        if (value == 0) {
            while (run >= 11) {
                int times = run > 138 ? 138 : run;
                add_entry(t, 18, times - 11);
                run -= times;
            }
            if (run >= 3) {
                add_entry(t, 17, run - 3);
                run = 0;
            }
        } else {
            add_entry(t, value, 0);
            run--;
            while (run >= 3) {
                int times = run > 6 ? 6 : run;
                add_entry(t, 16, times - 3);
                run -= times;
            }
        }
        for (; run > 0; run--) {
            add_entry(t, value, 0);
        }
    }

    /* Codes for the small set, from how often each of its symbols is
     * used. This set may not have a single code only, so a second one is
     * added if need be. */
    uint32_t used[SMALL_CODES] = {0};
    for (int e = 0; e < t->entry_count; e++) {
        used[t->entries[e].symbol]++;
    }
    choose_lengths(used, SMALL_CODES, SMALL_MAX_BITS, t->small_lengths);
    int in_use = 0;
    for (int s = 0; s < SMALL_CODES; s++) {
        in_use += t->small_lengths[s] != 0;
    }
    if (in_use == 1) {
        t->small_lengths[t->small_lengths[0] != 0 ? 1 : 0] = 1;
    }
    assign_codes(t->small_lengths, SMALL_CODES, t->small_codes);

    t->small_count = SMALL_CODES;
    while (t->small_count > 4
        && t->small_lengths[SMALL_ORDER[t->small_count - 1]] == 0) {
        t->small_count--;
    }

    t->bits = 5 + 5 + 4 + 3 * (uint32_t)t->small_count;
    for (int e = 0; e < t->entry_count; e++) {
        int symbol = t->entries[e].symbol;
        t->bits += t->small_lengths[symbol];
        t->bits += symbol == 16 ? 2u : symbol == 17 ? 3u
            : symbol == 18 ? 7u : 0u;
    }
}

static void write_tables(BitWriter *w, const Tables *t)
{
    write_bits(w, (uint32_t)(t->literal_count - 257), 5);
    write_bits(w, (uint32_t)(t->distance_count - 1), 5);
    write_bits(w, (uint32_t)(t->small_count - 4), 4);
    for (int i = 0; i < t->small_count; i++) {
        write_bits(w, t->small_lengths[SMALL_ORDER[i]], 3);
    }
    for (int e = 0; e < t->entry_count; e++) {
        int symbol = t->entries[e].symbol;
        write_code(w, t->small_codes[symbol], t->small_lengths[symbol]);
        if (symbol == 16) {
            write_bits(w, t->entries[e].extra, 2);
        } else if (symbol == 17) {
            write_bits(w, t->entries[e].extra, 3);
        } else if (symbol == 18) {
            write_bits(w, t->entries[e].extra, 7);
        }
    }
}

/* ---- Packing: writing a block ---- */

static void write_tokens(BitWriter *w, const Token *tokens, size_t count,
    const BlockCodes *c)
{
    for (size_t i = 0; i < count; i++) {
        if (tokens[i].length == 0) {
            int symbol = tokens[i].distance;
            write_code(w, c->literal_codes[symbol],
                c->literal_lengths[symbol]);
            continue;
        }
        uint32_t length = tokens[i].length;
        uint32_t distance = (uint32_t)tokens[i].distance + 1;
        int lc = length_code(length);
        int dc = distance_code(distance);
        int symbol = SYMBOL_FIRST_LENGTH + lc;

        write_code(w, c->literal_codes[symbol], c->literal_lengths[symbol]);
        write_bits(w, length - LENGTH_BASE[lc], LENGTH_EXTRA[lc]);
        write_code(w, c->distance_codes[dc], c->distance_lengths[dc]);
        write_bits(w, distance - DISTANCE_BASE[dc], DISTANCE_EXTRA[dc]);
    }
    write_code(w, c->literal_codes[SYMBOL_END],
        c->literal_lengths[SYMBOL_END]);
}

/* Writes the `size` bytes at `bytes` as one block, in whichever of the
 * three forms comes out smallest. */
static void write_block(BitWriter *w, const uint8_t *bytes, size_t size,
    const Token *tokens, size_t token_count, bool last)
{
    BlockCodes fixed;
    BlockCodes own;
    Tables tables;
    Usage usage;

    count_usage(tokens, token_count, &usage);

    fixed_codes(&fixed);
    uint32_t fixed_bits = token_bits(&usage, &fixed);

    choose_lengths(usage.literals, 286, MAX_BITS, own.literal_lengths);
    own.literal_lengths[286] = 0;
    own.literal_lengths[287] = 0;
    choose_lengths(usage.distances, DISTANCE_CODES, MAX_BITS,
        own.distance_lengths);
    /* The tables must list at least one distance code, used or not. */
    bool any_distance = false;
    for (int i = 0; i < DISTANCE_CODES; i++) {
        any_distance = any_distance || own.distance_lengths[i] != 0;
    }
    if (!any_distance) {
        own.distance_lengths[0] = 1;
    }
    assign_codes(own.literal_lengths, MAX_LITERAL_CODES, own.literal_codes);
    assign_codes(own.distance_lengths, DISTANCE_CODES, own.distance_codes);
    make_tables(&own, &tables);
    uint32_t own_bits = tables.bits + token_bits(&usage, &own);

    /* Stored: up to 7 bits to reach a byte boundary, the length twice,
     * and the bytes as they are. */
    uint32_t stored_bits = 7 + 32 + 8 * (uint32_t)size;

    write_bits(w, last ? 1 : 0, 1);
    if (stored_bits < fixed_bits && stored_bits < own_bits) {
        uint16_t length = (uint16_t)size;
        uint16_t flipped = (uint16_t)~length;
        write_bits(w, BLOCK_STORED, 2);
        write_to_byte_boundary(w);
        write_bits(w, length, 16);
        write_bits(w, flipped, 16);
        w->ok = w->ok && buffer_append(w->out, bytes, size);
    } else if (fixed_bits <= own_bits) {
        write_bits(w, BLOCK_FIXED, 2);
        write_tokens(w, tokens, token_count, &fixed);
    } else {
        write_bits(w, BLOCK_DYNAMIC, 2);
        write_tables(w, &tables);
        write_tokens(w, tokens, token_count, &own);
    }
}

bool zlib_compress(ByteBuffer *out, const uint8_t *data, size_t size)
{
    Finder finder = {.data = data, .head = NULL, .prev = NULL};
    finder.head = calloc(CHAIN_SLOTS, sizeof *finder.head);
    finder.prev = calloc(MAX_DISTANCE, sizeof *finder.prev);
    Token *tokens = malloc(BLOCK_MAX_BYTES * sizeof *tokens);
    if (finder.head == NULL || finder.prev == NULL || tokens == NULL) {
        free(finder.head);
        free(finder.prev);
        free(tokens);
        return false;
    }

    size_t size_before = out->size;
    BitWriter w = {.out = out, .bits = 0, .count = 0, .ok = true};

    /* The zlib header: "deflate, with a window of 32768 bytes". The two
     * bytes read as one number must divide by 31. */
    w.ok = buffer_push(out, 0x78) && buffer_push(out, 0x01);

    /* The data goes out a block at a time. Even no data at all needs one
     * block, to carry the end mark. */
    size_t pos = 0;
    do {
        size_t block_size = size - pos;
        if (block_size > BLOCK_MAX_BYTES) {
            block_size = BLOCK_MAX_BYTES;
        }
        size_t end = pos + block_size;
        size_t count = make_tokens(&finder, pos, end, tokens);
        write_block(&w, block_size > 0 ? data + pos : NULL, block_size,
            tokens, count, end == size);
        pos = end;
    } while (pos < size && w.ok);

    write_to_byte_boundary(&w);
    free(finder.head);
    free(finder.prev);
    free(tokens);

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
