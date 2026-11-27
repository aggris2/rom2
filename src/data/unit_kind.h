/* A kind of unit: a weak goblin and a strong one are two kinds of the
 * one goblin, with their own numbers. A unit's definition, `unit.toml`,
 * lists its kinds as [[kind]] tables, and a map names the kind of each
 * of its units by the kind's ID.
 *
 * Only what the game uses so far is read.
 */
#ifndef ROM2_DATA_UNIT_KIND_H
#define ROM2_DATA_UNIT_KIND_H

#include <stdbool.h>
#include <stdint.h>

#include "base/toml.h"

/* The IDs a kind can have are less than this. */
#define UNIT_KIND_IDS 16384

typedef struct UnitKind {
    uint16_t id;
    /* Which colouring a monster of this kind has, counted from 1: the
     * first is the first of its sheet's palettes. For a human it is a
     * number of another meaning. */
    uint16_t face;
} UnitKind;

/* Reads a kind from its table.
 *
 * Returns false, and leaves `*out` alone, if the table lacks its ID or
 * its face, the ID is not from 0 to UNIT_KIND_IDS - 1, or the face not
 * from 0 to 65,535. */
bool unit_kind_read(const TomlTable *kind, UnitKind *out);

#endif
