/* Writing a spell's definition, the file spell.toml in its folder.
 *
 * A spell is a row of data.bin's spells, and its number is the place of
 * its row, counted from 1: that is the number a monster names the spells
 * it casts by. The file has the row's numbers under ROM2's names, in the
 * table's own order:
 *
 *     # Fire Arrow
 *     id = 1
 *     name = "Fire Arrow"
 *     level = 3
 *     mana_cost = 3
 *     sphere = 1
 *     max_range = 7
 *     damage_min = 5
 *     damage_max = 10
 *     scroll_cost = 50
 *     book_cost = 1000
 *
 * and, for a spell that has it, `effects`, the row's text of what the
 * spell does to its target, as the original writes it:
 *
 *     effects = "speed=+2: duration 30"
 *
 * A column this does not know is left out.
 */
#ifndef ROM2_IMPORT_SPELL_TOML_H
#define ROM2_IMPORT_SPELL_TOML_H

#include <stdbool.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/databin.h"
#include "import/name.h"

/* Makes the name of a spell's folder from its number and its name, as
 * name_folder() does: spell 1, "Fire Arrow", gives "001_fire_arrow".
 *
 * Returns false, and leaves `out` alone, if the number is not between 0
 * and 999. */
bool spell_toml_folder_name(int32_t id, const DataBinRow *row,
    char out[NAME_FOLDER_SIZE]);

/* Adds the text of spell.toml to `b`, for the spell with the given
 * number, whose row of data.bin's spells is `row`.
 *
 * Returns false if memory runs out. `b` may then hold part of the
 * text. */
bool spell_toml_write(ByteBuffer *b, const DataBin *bin, int32_t id,
    const DataBinRow *row);

#endif
