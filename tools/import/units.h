/* Reading units from the original game's units.reg.
 *
 * The registry has one section for each unit, named "Unit0", "Unit1" and
 * so on. The number in the name is only a position. A unit is known by
 * its "ID", which is the number maps use for it.
 *
 * A unit may name a "Parent", the ID of another unit. A value the unit
 * does not state itself is then taken from its parent.
 */
#ifndef ROM2_IMPORT_UNITS_H
#define ROM2_IMPORT_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/reg.h"

/* The room a section name needs: "Unit", up to ten digits and the closing
 * zero. */
#define UNITS_SECTION_SIZE 16

/* Finds the section of the unit with the given ID and writes its name,
 * such as "Unit8", to `out`.
 *
 * Returns false, and leaves `out` alone, if no unit has that ID. */
bool units_section(const Reg *r, int32_t id, char out[UNITS_SECTION_SIZE]);

/* Says whether the unit with the given ID, or one of its parents, states
 * a value called `key`, of any kind. */
bool units_has(const Reg *r, int32_t id, const char *key);

/* Reads the whole number `key` of the unit with the given ID. If the unit
 * does not state it, its parent's value is used, and so on up.
 *
 * Returns false, and leaves `*out` alone, if no unit has that ID, or
 * neither the unit nor any parent states the value. */
bool units_int(const Reg *r, int32_t id, const char *key, int32_t *out);

/* Reads the text `key` of the unit with the given ID, from the unit or
 * its parents as units_int() does. `*out` is set to point at the text
 * inside the registry's own bytes. */
bool units_text(const Reg *r, int32_t id, const char *key,
    const char **out);

/* Reads the list of whole numbers `key` of the unit with the given ID,
 * from the unit or its parents as units_int() does. `out` has room for
 * `capacity` numbers, and `*out_count` is set to how many there are.
 *
 * A list that a unit states but that does not fit `capacity` makes this
 * fail. The parent's list is not used instead. */
bool units_int_list(const Reg *r, int32_t id, const char *key,
    int32_t *out, size_t capacity, size_t *out_count);

#endif
