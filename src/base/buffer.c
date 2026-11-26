#include "base/buffer.h"

#include <stdlib.h>
#include <string.h>

#define BUFFER_FIRST_CAPACITY 64u

/* Makes sure there is room for `extra` more bytes after the ones in use.
 * The capacity doubles until it is enough, so adding one byte at a time
 * does not copy the whole block each time. */
static bool buffer_reserve(ByteBuffer *b, size_t extra)
{
    if (extra <= b->capacity - b->size) {
        return true;
    }
    if (extra > SIZE_MAX - b->size) {
        return false;
    }
    size_t needed = b->size + extra;

    size_t capacity = b->capacity > 0 ? b->capacity : BUFFER_FIRST_CAPACITY;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    /* On failure realloc() leaves the old block alone, and so do we. */
    uint8_t *bigger = realloc(b->data, capacity);
    if (bigger == NULL) {
        return false;
    }
    b->data = bigger;
    b->capacity = capacity;
    return true;
}

bool buffer_push(ByteBuffer *b, uint8_t value)
{
    if (!buffer_reserve(b, 1)) {
        return false;
    }
    b->data[b->size] = value;
    b->size += 1;
    return true;
}

bool buffer_append(ByteBuffer *b, const uint8_t *bytes, size_t count)
{
    if (count == 0) {
        return true;
    }
    if (!buffer_reserve(b, count)) {
        return false;
    }
    memcpy(b->data + b->size, bytes, count);
    b->size += count;
    return true;
}

bool buffer_u32_le(ByteBuffer *b, uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)(value & 0xff),
        (uint8_t)((value >> 8) & 0xff),
        (uint8_t)((value >> 16) & 0xff),
        (uint8_t)((value >> 24) & 0xff),
    };
    return buffer_append(b, bytes, sizeof bytes);
}

bool buffer_u32_be(ByteBuffer *b, uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)((value >> 24) & 0xff),
        (uint8_t)((value >> 16) & 0xff),
        (uint8_t)((value >> 8) & 0xff),
        (uint8_t)(value & 0xff),
    };
    return buffer_append(b, bytes, sizeof bytes);
}

void buffer_free(ByteBuffer *b)
{
    free(b->data);
    b->data = NULL;
    b->size = 0;
    b->capacity = 0;
}
