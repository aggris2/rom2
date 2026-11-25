/* Checked reading from a block of bytes.
 *
 * Every parser reads through a Reader instead of touching a pointer. A read
 * that would go past the end fails, and the failure sticks: after one
 * failed read, every later read also fails. A parser can therefore read a
 * whole record without testing each call, and check reader_ok() once at
 * the end.
 *
 * A Reader does not own its bytes, never allocates and does no I/O.
 */
#ifndef ROM2_BASE_READER_H
#define ROM2_BASE_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Reader {
    const uint8_t *data; /* first byte; not owned */
    size_t size;         /* number of bytes at data */
    size_t pos;          /* number of bytes read so far */
    bool failed;         /* a read went past the end */
} Reader;

/* Returns a reader positioned at the first of `size` bytes at `data`.
 *
 * A size of 0 gives a valid, empty reader, and `data` may then be NULL.
 * A NULL `data` with a non-zero size gives an empty reader that has
 * already failed. */
Reader reader_init(const uint8_t *data, size_t size);

/* Returns the number of bytes that can still be read. Returns 0 once the
 * reader has failed, so a count taken from it can never ask for bytes
 * that will not be delivered. */
size_t reader_remaining(const Reader *r);

/* The read functions. Each reads one value and moves past it. Numbers
 * wider than one byte are stored low byte first.
 *
 * If fewer bytes are left than the value needs, or the reader has already
 * failed, the function returns 0, marks the reader as failed and does not
 * move. */

/* Reads an unsigned 8-bit number: 1 byte. */
uint8_t reader_u8(Reader *r);

/* Reads an unsigned 16-bit number: 2 bytes. */
uint16_t reader_u16(Reader *r);

/* Reads an unsigned 32-bit number: 4 bytes. */
uint32_t reader_u32(Reader *r);

/* Reads a signed 8-bit number: 1 byte. */
int8_t reader_i8(Reader *r);

/* Reads a signed 16-bit number: 2 bytes. */
int16_t reader_i16(Reader *r);

/* Reads a signed 32-bit number: 4 bytes. */
int32_t reader_i32(Reader *r);

/* Returns true if no read has failed since reader_init(). */
bool reader_ok(const Reader *r);

#endif
