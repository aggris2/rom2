/* A kind of structure: a hut, a windmill, a tower. It is what the game
 * needs of a structure's definition, `structure.toml`.
 *
 * A structure stands on a base of squares and its picture rises over
 * the squares behind it: the picture is as wide as the base and never
 * less high.
 */
#ifndef ROM2_DATA_STRUCTURE_KIND_H
#define ROM2_DATA_STRUCTURE_KIND_H

#include <stdbool.h>
#include <stdint.h>

#include "base/toml.h"
#include "data/sprite_sheet.h"

/* The kinds there can be: a map names a kind by its ID, from 1. */
#define STRUCTURE_KINDS 256

/* The most squares a structure is wide or high. */
#define STRUCTURE_MAX_SIDE 64

typedef struct StructureKind {
    uint16_t width;         /* of its base, in squares */
    uint16_t depth;         /* of its base, from back to front */
    uint16_t height;        /* of its picture, in squares */
    SpriteSheet sheet;
} StructureKind;

/* Reads a kind from the table of its definition's top.
 *
 * Returns false, and leaves `*out` alone, if the definition lacks its
 * size, its height or a usable [sprite] part, a side is less than 1 or
 * more than STRUCTURE_MAX_SIDE, or the picture is less high than the
 * base is deep. */
bool structure_kind_read(const TomlTable *top, StructureKind *out);

#endif
