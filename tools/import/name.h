/* Names as the original game's files store them: a field of a fixed
 * number of bytes, with the name at its start and a zero byte after it if
 * the name is shorter than the field. */
#ifndef ROM2_IMPORT_NAME_H
#define ROM2_IMPORT_NAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Copies the name in a field of `field_size` bytes into `dst` as a C
 * string. It stops at the first zero byte, at the end of the field, or
 * when `dst` is full, whichever comes first, and always ends `dst` with a
 * zero. Does nothing if `dst_size` is 0. */
void name_copy(char *dst, size_t dst_size, const uint8_t *field,
    size_t field_size);

/* The room a folder name from name_folder() needs, with its closing
 * zero. */
#define NAME_FOLDER_SIZE 64

/* Makes the name of a thing's folder from its ID and its name: the ID in
 * three digits, then the name in lower case with '_' for anything that
 * is not a letter or a digit. 3 and "Human Swordsman" give
 * "003_human_swordsman". A name that is too long is cut, and a thing
 * without a name is its number alone.
 *
 * Returns false, and leaves `out` alone, if the ID is not between 0 and
 * 999. */
bool name_folder(int32_t id, const char *name, char out[NAME_FOLDER_SIZE]);

/* Writes a name as it goes into the name of a file: in lower case, with
 * one '_' for every run of characters that are not letters or digits,
 * and none at the start or the end. "Magic Wood" gives "magic_wood". A
 * name that does not fit `size` is cut. Does nothing if `size` is 0. */
void name_label(const char *name, char *out, size_t size);

#endif
