/* Writing and reading a TOML file, the text format ROM2 keeps its
 * definitions in.
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
 * Each writing function adds one line to a buffer. This writes only the
 * small part of TOML that ROM2 needs: whole numbers, numbers with a
 * fraction, yes and no, texts, and lists of whole numbers or texts.
 *
 * A key or a header is written as it is given, so it may hold only
 * letters, digits, '_' and '-', and a header also '.' between names.
 *
 * Every writing function returns false if memory runs out. The buffer
 * may then hold part of a line.
 *
 * Reading takes a whole file and gives its tables, which are then asked
 * for their values by key. It reads what the writing functions write,
 * and no more of TOML than that.
 */
#ifndef ROM2_BASE_TOML_H
#define ROM2_BASE_TOML_H

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

/* Adds a yes or a no: "key = true" or "key = false". */
bool toml_bool(ByteBuffer *b, const char *key, bool value);

/* Adds a number with a fraction: "key = 0.25". It is written with as
 * many digits as it takes to read back the same 64-bit number, and
 * always with a point or an exponent, so that it is not taken for a
 * whole number. A number that is not finite is refused. */
bool toml_float(ByteBuffer *b, const char *key, double value);

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

/* A table that was read: the keys and values under one header, or at
 * the top of the file. */
typedef struct TomlTable TomlTable;

/* Reads a TOML file held in memory and gives the table of its top.
 * Free it with toml_free().
 *
 * It reads what this module writes: comments, headers of tables and of
 * lists of tables, and keys with a whole number, a number with a
 * fraction, true or false, a text in double quotes, or a list of whole
 * numbers or of texts on one line. A name may be in double quotes. A
 * header of a table may have names with '.' between; a header of a list
 * of tables has one name. Spaces, either end of line and a comma after
 * the last value of a list are taken as they come.
 *
 * Returns false, and leaves `*out` alone, if the text is anything else,
 * a key or a table is given twice, a whole number does not fit 32 bits,
 * or memory runs out. `error_line`, if not NULL, then gets the number
 * of the line that was refused, from 1, or 0 when memory ran out. */
bool toml_read(const uint8_t *data, size_t size, TomlTable **out,
    size_t *error_line);

/* Frees the table that toml_read() gave, with everything in it. NULL is
 * allowed. */
void toml_free(TomlTable *top);

/* Each of these looks up a key of a table. It returns false, and
 * leaves what it would give alone, if the table has no such key or the
 * key holds another kind of value. What it gives belongs to the table
 * and lasts until toml_free().
 *
 * A whole number is also given as a number with a fraction, and not
 * the other way round. A list with nothing in it is a list of either
 * kind. */
/* True if the table has the key, whatever it holds. For a key that
 * need not be there, to tell "not there" from "there and wrong". */
bool toml_has(const TomlTable *table, const char *key);

bool toml_get_int(const TomlTable *table, const char *key, int32_t *out);
bool toml_get_bool(const TomlTable *table, const char *key, bool *out);
bool toml_get_float(const TomlTable *table, const char *key, double *out);
bool toml_get_text(const TomlTable *table, const char *key,
    const char **out);
bool toml_get_int_list(const TomlTable *table, const char *key,
    const int32_t **out_values, size_t *out_count);
bool toml_get_text_list(const TomlTable *table, const char *key,
    const char *const **out_texts, size_t *out_count);

/* The table under a header inside this table, or NULL if there is
 * none: for "[anim.attack]", "anim" of the top and then "attack" of
 * that. */
const TomlTable *toml_get_table(const TomlTable *table, const char *key);

/* How many entries a list of tables has, 0 if there is no such list,
 * and one entry of it, counted from 0, or NULL if there is none. */
size_t toml_get_table_count(const TomlTable *table, const char *key);
const TomlTable *toml_get_table_at(const TomlTable *table, const char *key,
    size_t index);

#endif
