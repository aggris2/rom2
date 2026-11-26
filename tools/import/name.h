/* Names as the original game's files store them: a field of a fixed
 * number of bytes, with the name at its start and a zero byte after it if
 * the name is shorter than the field. */
#ifndef ROM2_IMPORT_NAME_H
#define ROM2_IMPORT_NAME_H

#include <stddef.h>
#include <stdint.h>

/* Copies the name in a field of `field_size` bytes into `dst` as a C
 * string. It stops at the first zero byte, at the end of the field, or
 * when `dst` is full, whichever comes first, and always ends `dst` with a
 * zero. Does nothing if `dst_size` is 0. */
void name_copy(char *dst, size_t dst_size, const uint8_t *field,
    size_t field_size);

#endif
