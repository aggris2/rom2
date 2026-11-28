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

/* How a unit moves, by the numbers the original knows the ways by.
 * What hovers crosses water and stands on the ground among those that
 * walk; what flies is over them, in the air. */
typedef enum UnitMover {
    UNIT_WALKS = 1,
    UNIT_HOVERS = 2,
    UNIT_FLIES = 3
} UnitMover;

/* The most squares a unit covers along one side. */
#define UNIT_KIND_MAX_SIZE 3

typedef struct UnitKind {
    uint16_t id;
    /* Which colouring a monster of this kind has, counted from 1: the
     * first is the first of its sheet's palettes. For a human it is a
     * number of another meaning. */
    uint16_t face;
    /* How far it turns in a tick, in the 256 steps of a full turn:
     * never 0, so a record that is all zeros is no kind. */
    uint8_t rotation_speed;
    /* How fast it goes: see mechanics/walk.h for what a step makes of
     * it. From 1 to 255. */
    uint8_t speed;
    /* The most health and the most mana a unit of it has, which it
     * starts with. A kind without mana has 0 or -1, and a few have a
     * health of -1. */
    int32_t health;
    int32_t mana;
    /* How many squares it covers each way, and how it moves: a
     * UnitMover. A kind that says -1 for either, as those of people
     * do, covers one square and walks. */
    uint8_t size;
    uint8_t mover;
} UnitKind;

/* Reads a kind from its table.
 *
 * Returns false, and leaves `*out` alone, if the table lacks its ID,
 * its face, its rotation speed, its speed, its health or its mana,
 * the ID is not from 0 to UNIT_KIND_IDS - 1, the face not from 0 to
 * 65,535, the rotation speed or the speed not from 1 to 255, or the
 * health or the mana less than -1. It need not say its `token_size`
 * or its `movement_type`, but is refused if the one is not -1 or from
 * 1 to UNIT_KIND_MAX_SIZE, or the other not -1 or a UnitMover. */
bool unit_kind_read(const TomlTable *kind, UnitKind *out);

#endif
