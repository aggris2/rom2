/* The bars over the units: how much health each has left, and mana if
 * it has any.
 *
 * A bar lies along the top of a unit's selection box, between two
 * small balls. It is four pixels high: a dark line, a bright one, one
 * between the two and a dark one again, as long as the unit has health
 * left of the most it can have, over a ground as long as the whole,
 * which has the same four lines in grey. Health is green, yellow
 * under half and red under a quarter. The bar of the mana lies under
 * it and is blue. The look is the original's.
 *
 * A player picks whether all units show their bars or only the one
 * that is selected. With all, the selected one is as told and the
 * others are faint: half see-through and without the grey ground.
 */
#ifndef ROM2_VIEW_BARS_H
#define ROM2_VIEW_BARS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/rgb.h"
#include "data/map.h"
#include "mechanics/units.h"
#include "view/camera.h"
#include "view/selection.h"
#include "view/sprite.h"
#include "view/units.h"

/* The lines a bar is made of. The sheet of the bars has a row for
 * each, of its colour from end to end; then the same rows again, half
 * see-through; and under those the ball. */
typedef enum BarLine {
    BAR_GROUND_DARK,
    BAR_GROUND_LIGHT,
    BAR_GROUND_MIDDLE,
    BAR_GREEN_BRIGHT,
    BAR_GREEN_MIDDLE,
    BAR_GREEN_DARK,
    BAR_YELLOW_BRIGHT,
    BAR_YELLOW_MIDDLE,
    BAR_YELLOW_DARK,
    BAR_RED_BRIGHT,
    BAR_RED_MIDDLE,
    BAR_RED_DARK,
    BAR_BLUE_BRIGHT,
    BAR_BLUE_MIDDLE,
    BAR_BLUE_DARK,
    BAR_LINES
} BarLine;

/* The colour of a line. */
Rgb bars_colour(BarLine line);

/* The sheet: as wide as the longest bar, a ball of BARS_BALL pixels
 * each way in its bottom left corner. */
#define BARS_BALL 4
#define BARS_LONGEST 256
#define BARS_SHEET_WIDTH BARS_LONGEST
#define BARS_SHEET_HEIGHT (2 * BAR_LINES + BARS_BALL)

/* The sheet of the bars on the card. Nothing is drawn if it is not
 * `known`. */
typedef struct BarsLook {
    bool known;
    uint16_t sheet;
} BarsLook;

/* The most sprites bars_sprites() writes for a list of units. */
size_t bars_most(const Units *units);

/* Writes the sprites of the bars and returns how many it wrote.
 * `selected` has the units that are selected, NULL for none. With
 * `all`, every unit shows its bars, the others faint; without, the
 * selected ones alone. `looks` and `between` are as units_sprites()
 * takes them: a bar goes with its unit as it walks.
 *
 * A unit has a bar of health if the most it can have is above 0, and
 * one of mana likewise. A bar is as long as the selection box is wide
 * less the two balls, and at least a pixel; what is left of it is cut
 * down to whole pixels, but a unit with any health left shows one.
 * The original shows a pixel of mana even when none is left, and of
 * health not: here the two go by one rule. The health lies with its
 * middle on the top of the box. */
size_t bars_sprites(const Map *map, const Units *units,
                    const UnitLook *looks, const BarsLook *bars,
                    const Selection *selected, bool all,
                    uint32_t between, const Camera *camera, Sprite *out);

#endif
