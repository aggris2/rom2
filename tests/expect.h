/* Small things many tests share: whether a buffer holds a text, and a
 * number written low byte first.
 */
#ifndef ROM2_TESTS_EXPECT_H
#define ROM2_TESTS_EXPECT_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "base/buffer.h"

/* True if the buffer holds exactly the text `expected`. */
static inline bool holds(const ByteBuffer *b, const char *expected)
{
    size_t length = strlen(expected);
    return b->size == length
        && (length == 0 || memcmp(b->data, expected, length) == 0);
}

/* True if the text `part` is somewhere in the buffer. */
static inline bool has(ByteBuffer *b, const char *part)
{
    if (!buffer_push(b, 0)) {
        return false;
    }
    b->size--;
    return strstr((const char *)b->data, part) != NULL;
}

/* Adds a number of 16 bits, low byte first. */
static inline void put_u16(ByteBuffer *b, uint32_t value)
{
    buffer_push(b, (uint8_t)(value & 0xff));
    buffer_push(b, (uint8_t)((value >> 8) & 0xff));
}

#endif
