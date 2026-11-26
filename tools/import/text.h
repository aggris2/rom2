/* Turning the original game's text files into plain UTF-8.
 *
 * The original's .txt files are in the Windows code page 1251, which is
 * ASCII with Cyrillic letters in its upper half, and end their lines
 * with a carriage return and a line feed.
 */
#ifndef ROM2_IMPORT_TEXT_H
#define ROM2_IMPORT_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"

/* Appends `size` bytes of code page 1251 text to `out` as UTF-8, with
 * each line ended by a line feed alone: a carriage return right before
 * a line feed is dropped. The one byte the code page gives no meaning,
 * 0x98, becomes U+FFFD, the mark for a character that is not known.
 *
 * Returns false if `out` could not grow. Part of the text may then
 * have been appended. */
bool text_cp1251_to_utf8(const uint8_t *data, size_t size, ByteBuffer *out);

#endif
