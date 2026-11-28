/* One animation of a unit, such as the beating of a bat's wings: the
 * [anim.<name>] part of a unit's definition.
 *
 * A unit's sheet has a row for each of the eight ways an animation
 * shows it facing, after the row of its standing frames. In each of
 * those rows an animation takes some columns side by side, its frames,
 * and it says which of them to show and for how many ticks.
 */
#ifndef ROM2_DATA_UNIT_ANIMATION_H
#define ROM2_DATA_UNIT_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

#include "base/toml.h"
#include "data/sprite_sheet.h"

/* The ways an animation shows a unit facing: a full turn in this many
 * steps. */
#define UNIT_ANIMATION_FACINGS 8

typedef struct UnitAnimation {
    uint16_t column;        /* the first of its columns in a row */
    /* How many of its columns come before the frames its round
     * counts from: walking has a few frames of setting off first,
     * which the game does not show. 0 for the others. */
    uint16_t begin;
    /* Which of its frames it shows in each tick, counted from its
     * first column. Of no length for a unit that has no such
     * animation. */
    SpriteRound round;
} UnitAnimation;

/* Reads the animation `name` of a unit from the table of its
 * definition's top. A unit that has no such animation gets one of no
 * length.
 *
 * Returns false, and leaves `*out` alone, if the animation is there
 * and lacks its column or its columns, either is not from 0 to 4096,
 * it has no column at all, its `begin` is not from 0 to less than its
 * columns, or its lists are not a round of the columns after `begin`:
 * see sprite_round_read(). An animation with columns and no lists is
 * refused as well. It need not say its `begin`, which is then 0. */
bool unit_animation_read(const TomlTable *top, const char *name,
                         UnitAnimation *out);

#endif
