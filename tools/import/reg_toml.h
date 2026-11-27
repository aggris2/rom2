/* Writing a whole registry out as TOML, as it is: for the small
 * registries of the original that are plain settings, where ROM2 has no
 * names of its own for the values yet.
 */
#ifndef ROM2_IMPORT_REG_TOML_H
#define ROM2_IMPORT_REG_TOML_H

#include <stdbool.h>

#include "base/buffer.h"
#include "import/reg.h"

/* Adds every section of the registry to `b` as a table, in the order of
 * the registry, and in each its values in their order:
 *
 *     [Terrain]
 *     CostLand = 8
 *     PassLand = 0
 *
 * The names are the original's, letter for letter. One that is not made
 * of letters, digits, '_' and '-' alone is put in quotes, as TOML asks:
 * the section "Path Finding" becomes the table ["Path Finding"]. A text
 * is written as it is, with its '\'.
 *
 * Returns false if an entry cannot be read, a list has more than
 * REG_TOML_MAX_LIST numbers, or memory runs out. `b` may then hold part
 * of the text. */
bool reg_toml_write(ByteBuffer *b, const Reg *reg);

/* The longest list of numbers that can be written. */
#define REG_TOML_MAX_LIST 256

#endif
