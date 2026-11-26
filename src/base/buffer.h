/* A block of bytes that grows as bytes are added to its end.
 *
 * Used to build a file in memory before it is written out. Start with a
 * zeroed buffer, add to it, and give the memory back with buffer_free():
 *
 *     ByteBuffer b = {0};
 *     buffer_push(&b, 0x89);
 *     ...
 *     buffer_free(&b);
 *
 * If memory runs out, an add returns false and the buffer is left exactly
 * as it was, still valid and still to be freed.
 */
#ifndef ROM2_BASE_BUFFER_H
#define ROM2_BASE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ByteBuffer {
    uint8_t *data;   /* the bytes; owned; NULL while nothing is allocated */
    size_t size;     /* bytes in use */
    size_t capacity; /* bytes allocated */
} ByteBuffer;

/* Adds one byte. */
bool buffer_push(ByteBuffer *b, uint8_t value);

/* Adds `count` bytes. `bytes` may be NULL when `count` is 0. */
bool buffer_append(ByteBuffer *b, const uint8_t *bytes, size_t count);

/* Adds a 32-bit number as 4 bytes, low byte first. */
bool buffer_u32_le(ByteBuffer *b, uint32_t value);

/* Adds a 32-bit number as 4 bytes, high byte first. */
bool buffer_u32_be(ByteBuffer *b, uint32_t value);

/* Frees the buffer's memory and resets it to empty. The buffer can be
 * used again afterwards. */
void buffer_free(ByteBuffer *b);

#endif
