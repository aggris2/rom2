/* Building a data.bin by hand, for tests.
 *
 * Tests never read a file of the original game, so a test that needs a
 * data.bin writes one into a buffer with these: the column names of a
 * table, its count, and its rows, piece by piece.
 */
#ifndef ROM2_TESTS_IMPORT_MAKE_DATABIN_H
#define ROM2_TESTS_IMPORT_MAKE_DATABIN_H

#include <stdint.h>
#include <string.h>

#include "base/buffer.h"

static inline void put_text(ByteBuffer *b, const char *text)
{
    buffer_push(b, (uint8_t)strlen(text));
    buffer_append(b, (const uint8_t *)text, strlen(text));
}

static inline void put_u16(ByteBuffer *b, uint32_t value)
{
    buffer_push(b, (uint8_t)(value & 0xff));
    buffer_push(b, (uint8_t)((value >> 8) & 0xff));
}

static inline void put_i32(ByteBuffer *b, int32_t value)
{
    buffer_u32_le(b, (uint32_t)value);
}

/* The 8 bytes of a fraction that is a small whole number: 0, 1 or 2. */
static inline void put_fraction(ByteBuffer *b, int value)
{
    static const uint8_t bytes[3][8] = {
        {0, 0, 0, 0, 0, 0, 0x00, 0x00},
        {0, 0, 0, 0, 0, 0, 0xf0, 0x3f},
        {0, 0, 0, 0, 0, 0, 0x00, 0x40},
    };
    buffer_append(b, bytes[value], 8);
}

/* Column names: "Name" and then the given ones. */
static inline void put_header(ByteBuffer *b, const char *const *names,
    uint32_t count)
{
    put_u16(b, count + 1);
    put_text(b, "Name");
    for (uint32_t i = 0; i < count; i++) {
        put_text(b, names[i]);
    }
}

/* A row of a table whose rows say how many numbers they have. */
static inline void put_row(ByteBuffer *b, const char *name,
    const int32_t *numbers, uint32_t count)
{
    put_text(b, name);
    put_u16(b, count);
    for (uint32_t i = 0; i < count; i++) {
        put_i32(b, numbers[i]);
    }
}

/* The tables in front of the monsters, all with no rows. */
static inline void put_empty_first_tables(ByteBuffer *b)
{
    const char *const columns[] = {"a"};
    put_header(b, columns, 1);  /* shapes and materials */
    put_i32(b, 0);
    put_i32(b, 0);
    put_header(b, columns, 1);  /* modifiers */
    put_i32(b, 0);
    put_header(b, columns, 1);  /* armour, shields and weapons */
    put_i32(b, 1);
    put_i32(b, 1);
    put_i32(b, 1);
    put_header(b, columns, 1);  /* magic items */
    put_i32(b, 1);
}

/* The tables behind the humans, with no rows. */
static inline void put_empty_last_tables(ByteBuffer *b)
{
    const char *const columns[] = {"a"};
    put_header(b, columns, 1);  /* buildings */
    put_i32(b, 1);
    put_header(b, columns, 1);  /* spells */
    put_i32(b, 2);
}

#endif
