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

#endif
