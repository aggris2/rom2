/* Writing a TOML file, the text format ROM2 keeps its definitions in.
 *
 * A TOML file is lines of `key = value`, grouped under headers in square
 * brackets:
 *
 *     id = 3
 *     name = "Human Swordsman"
 *
 *     [anim.attack]
 *     ticks = [2, 2, 1]
 *
 * Each function here adds one line to a buffer. This writes only the
 * small part of TOML that ROM2 needs: whole numbers, texts, and lists of
 * either.
 *
 * A key or a header is written as it is given, so it may hold only
 * letters, digits, '_' and '-', and a header also '.' between names.
 *
 * Every function returns false if memory runs out. The buffer may then
 * hold part of a line.
 */
#ifndef ROM2_IMPORT_TOML_H
#define ROM2_IMPORT_TOML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"

/* Adds a comment line: "# text". */
bool toml_comment(ByteBuffer *b, const char *text);

/* Adds a header, with an empty line before it: "[name]". The lines after
 * it belong to it. */
bool toml_table(ByteBuffer *b, const char *name);

/* Adds the header of one entry of a list of tables, with an empty line
 * before it: "[[name]]". Each such header starts another entry, and the
 * lines after it belong to that entry. */
bool toml_table_list(ByteBuffer *b, const char *name);

/* Adds a whole number: "key = 3". */
bool toml_int(ByteBuffer *b, const char *key, int32_t value);

/* Adds a text in quotes: key = "text". A quote or a backslash in the text
 * gets a backslash in front, and a control character is written as its
 * code, so any text can be given. The text has to be UTF-8, which plain
 * ASCII is. */
bool toml_text(ByteBuffer *b, const char *key, const char *text);

/* Adds a list of whole numbers: "key = [2, 2, 1]". `values` may be NULL
 * when `count` is 0, which gives "key = []". */
bool toml_int_list(ByteBuffer *b, const char *key, const int32_t *values,
    size_t count);

/* Adds a list of texts: key = ["a", "b"]. Each text is written as
 * toml_text() writes one. `texts` may be NULL when `count` is 0. */
bool toml_text_list(ByteBuffer *b, const char *key,
    const char *const *texts, size_t count);

#endif
