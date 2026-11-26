/* Packing and unpacking data in the zlib format, and the CRC that goes
 * with it.
 *
 * This is the lossless compression that PNG pictures use, the same kind as
 * in a zip file: unpacking gives back exactly the bytes that were packed.
 *
 * The work is done by the zlib library. This file is the only place that
 * calls it, so the rest of the code deals in ByteBuffers and never sees
 * zlib's own types.
 */
#ifndef ROM2_BASE_PACK_H
#define ROM2_BASE_PACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"

/* The most bytes that can be packed or unpacked in one call: 1 GiB. */
#define PACK_MAX_SIZE ((size_t)1 << 30)

/* Packs `size` bytes and adds the zlib stream to the end of `out`. `data`
 * may be NULL when `size` is 0.
 *
 * Returns false, and leaves `out` as it was, if `size` is more than
 * PACK_MAX_SIZE or memory runs out. */
bool pack_zlib(ByteBuffer *out, const uint8_t *data, size_t size);

/* Unpacks a zlib stream and adds the unpacked bytes to the end of `out`.
 * At most `max_size` bytes are produced: a stream that would unpack to
 * more is refused, so a small damaged file cannot ask for endless memory.
 * Bytes after the end of the stream are ignored.
 *
 * Returns false, and leaves `out` as it was, if the stream is damaged or
 * cut off, its checksum does not match, it would unpack to more than
 * `max_size` bytes, or memory runs out. */
bool unpack_zlib(ByteBuffer *out, const uint8_t *data, size_t size,
    size_t max_size);

/* CRC-32 as used by PNG and zip. It can be worked out over data that
 * arrives in pieces: start with PACK_CRC32_START and pass the result of
 * one call to the next. The CRC of "123456789" is 0xcbf43926. */
#define PACK_CRC32_START 0u
uint32_t pack_crc32(uint32_t crc, const uint8_t *data, size_t size);

#endif
