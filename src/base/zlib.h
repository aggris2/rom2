/* Packing and unpacking data in the zlib format.
 *
 * This is the lossless compression that PNG pictures use, the same kind as
 * in a zip file: unpacking gives back exactly the bytes that were packed.
 * A zlib stream is a 2-byte header, the data in "deflate" form, and an
 * Adler-32 checksum of the unpacked data.
 *
 * zlib_compress() looks for runs of bytes that came before and writes
 * them as "copy that again", then gives the values that occur most the
 * shortest codes. zlib_decompress() reads every kind of stream, so that
 * pictures made by other programs can be read as well.
 */
#ifndef ROM2_BASE_ZLIB_H
#define ROM2_BASE_ZLIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"

/* Packs `size` bytes and adds the zlib stream to the end of `out`. `data`
 * may be NULL when `size` is 0.
 *
 * Returns false, and leaves `out` as it was, if memory runs out. */
bool zlib_compress(ByteBuffer *out, const uint8_t *data, size_t size);

/* Unpacks a zlib stream and adds the unpacked bytes to the end of `out`.
 * At most `max_size` bytes are produced: a stream that would unpack to
 * more is refused, so a small damaged file cannot ask for endless memory.
 *
 * Returns false, and leaves `out` as it was, if the stream is damaged or
 * cut off, its checksum does not match, it would unpack to more than
 * `max_size` bytes, or memory runs out. */
bool zlib_decompress(ByteBuffer *out, const uint8_t *data, size_t size,
    size_t max_size);

#endif
