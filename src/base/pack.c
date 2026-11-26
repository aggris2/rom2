#include "base/pack.h"

#include <stdlib.h>
#include <string.h>

#include <zlib.h>

/* zlib counts with types that are 32 bits wide on some systems, so sizes
 * are handed to it in pieces no larger than this. */
#define PIECE_SIZE ((size_t)1 << 16)

bool pack_zlib(ByteBuffer *out, const uint8_t *data, size_t size)
{
    if (size > PACK_MAX_SIZE) {
        return false;
    }

    /* zlib says how large the packed data can be at most. Packing goes
     * into a block of that size first, and what was used is then added
     * to `out`. */
    uLong bound = compressBound((uLong)size);
    uint8_t *packed = malloc((size_t)bound);
    if (packed == NULL) {
        return false;
    }

    /* zlib wants a real address even for no data at all. */
    const uint8_t nothing = 0;
    uLongf packed_size = bound;
    int result = compress2(packed, &packed_size,
        size > 0 ? data : &nothing, (uLong)size, Z_DEFAULT_COMPRESSION);

    bool ok = result == Z_OK
        && buffer_append(out, packed, (size_t)packed_size);
    free(packed);
    return ok;
}

bool unpack_zlib(ByteBuffer *out, const uint8_t *data, size_t size,
    size_t max_size)
{
    if (size > PACK_MAX_SIZE || max_size > PACK_MAX_SIZE) {
        return false;
    }
    uint8_t *piece = malloc(PIECE_SIZE);
    if (piece == NULL) {
        return false;
    }

    z_stream stream;
    memset(&stream, 0, sizeof stream);
    if (inflateInit(&stream) != Z_OK) {
        free(piece);
        return false;
    }

    /* zlib takes its input through a pointer that is not const, though
     * it only reads from it. */
    const uint8_t nothing = 0;
    stream.next_in = (Bytef *)(size > 0 ? data : &nothing);
    stream.avail_in = (uInt)size;

    size_t size_before = out->size;
    bool ok = true;
    bool finished = false;
    while (ok && !finished) {
        stream.next_out = piece;
        stream.avail_out = (uInt)PIECE_SIZE;
        int result = inflate(&stream, Z_NO_FLUSH);
        size_t produced = PIECE_SIZE - stream.avail_out;

        if (result != Z_OK && result != Z_STREAM_END) {
            /* Damaged, or it needs input that is not there. */
            ok = false;
        } else if (produced > max_size - (out->size - size_before)) {
            ok = false;
        } else {
            ok = buffer_append(out, piece, produced);
            finished = result == Z_STREAM_END;
            /* No end reached, nothing produced and no input left: the
             * stream is cut off. */
            if (ok && !finished && produced == 0 && stream.avail_in == 0) {
                ok = false;
            }
        }
    }

    inflateEnd(&stream);
    free(piece);
    if (!ok) {
        out->size = size_before;
    }
    return ok;
}

uint32_t pack_crc32(uint32_t crc, const uint8_t *data, size_t size)
{
    /* In pieces, because zlib's count is 32 bits wide. */
    uLong value = crc;
    while (size > 0) {
        size_t piece = size < PIECE_SIZE ? size : PIECE_SIZE;
        value = crc32(value, data, (uInt)piece);
        data += piece;
        size -= piece;
    }
    return (uint32_t)value;
}
