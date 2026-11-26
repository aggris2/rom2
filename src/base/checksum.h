/* The two checksums that the PNG and zlib formats use.
 *
 * Both can be worked out over data that arrives in pieces: pass the result
 * of one call as the start value of the next.
 *
 *     uint32_t crc = crc32_update(CRC32_START, first, first_size);
 *     crc = crc32_update(crc, second, second_size);
 */
#ifndef ROM2_BASE_CHECKSUM_H
#define ROM2_BASE_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

#define CRC32_START 0u
#define ADLER32_START 1u

/* CRC-32 as used by PNG, zip and Ethernet. The CRC of "123456789" is
 * 0xcbf43926. */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size);

/* Adler-32 as used by zlib. The Adler-32 of "Wikipedia" is 0x11e60398. */
uint32_t adler32_update(uint32_t adler, const uint8_t *data, size_t size);

#endif
