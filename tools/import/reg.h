/* Reading the original game's .reg registry files.
 *
 * A registry is a small tree of named values: sections, and in each
 * section names with a value. A value is a whole number, a decimal
 * number, a text, or a list of whole numbers.
 *
 * The file is a 24-byte header, then a table of 32-byte entries, then one
 * number giving the size of the values area, then the values area itself,
 * which holds the texts and the lists. All numbers are 32-bit, low byte
 * first.
 */
#ifndef ROM2_IMPORT_REG_H
#define ROM2_IMPORT_REG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* An open registry: its bytes and what its header says. Does not own the
 * bytes. */
typedef struct Reg {
    const uint8_t *data;
    size_t size;
    uint32_t root_first;   /* index of the first root entry */
    uint32_t root_count;   /* number of root entries */
    uint32_t entry_count;  /* number of entries in the table */
    size_t values_offset;  /* where the values area starts */
    uint32_t values_size;  /* number of bytes in the values area */
} Reg;

/* Opens a registry held in memory.
 *
 * Returns false, and leaves `*out` alone, if the data is too short, the
 * signature is wrong, the table and the values area do not add up to the
 * size of the data exactly, or the root entries are not all inside the
 * table. */
bool reg_open(const uint8_t *data, size_t size, Reg *out);

/* What kind of thing an entry is. The numbers are the ones in the file. */
typedef enum RegType {
    REG_TEXT = 0,     /* a text */
    REG_SECTION = 1,  /* a section holding other entries */
    REG_INT = 2,      /* a whole number */
    REG_FLOAT = 4,    /* a decimal number of 64 bits */
    REG_INT_LIST = 6  /* a list of whole numbers */
} RegType;

/* One entry of the table. What `a` and `b` mean depends on the type:
 *
 *     REG_TEXT      a: offset in the values area   b: bytes, with the
 *                                                     closing zero
 *     REG_SECTION   a: index of its first entry    b: number of entries
 *     REG_INT       a: the number itself           b: not used
 *     REG_FLOAT     a: the number's four bytes     b: not used
 *     REG_INT_LIST  a: offset in the values area   b: bytes, 4 for each
 *                                                     number
 */
typedef struct RegEntry {
    RegType type;
    uint32_t a;
    uint32_t b;
    char name[17];  /* up to 16 characters and the closing zero */
} RegEntry;

/* Reads entry number `index` of the table and checks it.
 *
 * Returns false, and leaves `*out` alone, if there is no such entry, its
 * type is not one of the five, or it points outside the table or the
 * values area. A text must also end with a zero byte, and the size of a
 * list must be a multiple of 4. */
bool reg_entry_at(const Reg *r, uint32_t index, RegEntry *out);

/* Looking a value up by name. A value is named by its section and its
 * key, for example section "Unit0", key "MovePhases". Names must match
 * exactly, capitals included.
 *
 * Each of these returns false, and leaves its out-parameters alone, if
 * there is no such section or key, the value is of another kind than was
 * asked for, or the registry is damaged. */

/* Finds the entry `key` in the section `section`. */
bool reg_find(const Reg *r, const char *section, const char *key,
    RegEntry *out);

/* Reads a whole number. */
bool reg_int(const Reg *r, const char *section, const char *key,
    int32_t *out);

/* Reads a text. `*out` is set to point at the text inside the registry's
 * own bytes: it is not a copy, and it is valid for as long as those bytes
 * are. */
bool reg_text(const Reg *r, const char *section, const char *key,
    const char **out);

/* Reads a decimal number the same way. It is a number of 64 bits, kept
 * in the entry itself: its lower half in `a` and its upper half in `b`.
 *
 * Returns false, and leaves `*out` alone, if there is no such value or
 * it is not a decimal number. */
bool reg_float(const Reg *r, const char *section, const char *key,
    double *out);

/* Reads a list of whole numbers into `out`, which has room for `capacity`
 * numbers, and stores how many there are in `*out_count`. Also returns
 * false if the list has more numbers than `capacity`. */
bool reg_int_list(const Reg *r, const char *section, const char *key,
    int32_t *out, size_t capacity, size_t *out_count);

#endif
