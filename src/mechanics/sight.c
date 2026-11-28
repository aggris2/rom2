#include "mechanics/sight.h"

#include <stdbool.h>
#include <string.h>

/* The squares each way from the eye that are worked on, and where
 * the eye is among them. */
#define SIDE (2 * SIGHT_FURTHEST + 3)
#define EYE (SIGHT_FURTHEST + 1)

/* The whole part of a square root. */
static int32_t root(int32_t number)
{
    int32_t low = 0;
    int32_t high = 46341;
    while (high - low > 1) {
        int32_t middle = (low + high) / 2;
        if (middle * middle <= number) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low;
}

/* What the square `across` and `down` from the eye uses up before
 * its height is counted. */
static int32_t cost_of(int32_t across, int32_t down)
{
    int32_t wide = across < 0 ? -across : across;
    int32_t deep = down < 0 ? -down : down;
    int32_t rings = wide > deep ? wide : deep;
    return root((wide * wide + deep * deep) * SIGHT_SQUARE * SIGHT_SQUARE)
        / rings;
}

/* The way from a square towards the eye: one step across if it lies
 * much more across than down, one down if much more down, and else
 * both. */
static void towards_eye(int32_t across, int32_t down, int32_t *step_x,
                        int32_t *step_y)
{
    int32_t wide = across < 0 ? -across : across;
    int32_t deep = down < 0 ? -down : down;
    /* Right beside the eye across there is nothing to slant to. */
    bool flat = deep < wide / 2 || (wide == 1 && deep == 0);
    bool steep = wide * 2 < deep;
    *step_x = steep ? 0 : across > 0 ? -1 : 1;
    *step_y = flat ? 0 : down > 0 ? -1 : 1;
}

/* Works out what is left of sight on the square `across` and `down`
 * from the eye, in `left`, and marks it if it is seen. Returns
 * whether it is. */
static bool look(const Terrain *terrain, int32_t x, int32_t y,
                 int32_t across, int32_t down, int32_t left[SIDE][SIDE],
                 uint8_t *seen)
{
    int32_t at_x = x + across;
    int32_t at_y = y + down;
    int32_t step_x = 0;
    int32_t step_y = 0;
    if (at_x < 0 || at_y < 0 || (uint32_t)at_x >= terrain->width
        || (uint32_t)at_y >= terrain->height) {
        return false;
    }
    towards_eye(across, down, &step_x, &step_y);
    size_t at = (size_t)at_y * terrain->width + (size_t)at_x;
    size_t eye = (size_t)y * terrain->width + (size_t)x;
    int32_t rest = left[EYE + down + step_y][EYE + across + step_x]
        - cost_of(across, down)
        - ((int32_t)terrain->raised[at] - (int32_t)terrain->raised[eye]);
    left[EYE + down][EYE + across] = rest;
    if (rest < 1) {
        return false;
    }
    seen[at] = 1;
    return true;
}

void sight_mark(const Terrain *terrain, int32_t x, int32_t y,
                uint32_t sight, uint8_t *seen)
{
    int32_t left[SIDE][SIDE];
    if (x < 0 || y < 0 || (uint32_t)x >= terrain->width
        || (uint32_t)y >= terrain->height) {
        return;
    }
    memset(left, 0, sizeof left);
    left[EYE][EYE] = (int32_t)(sight > 255 ? 255 : sight) * SIGHT_SQUARE
        + SIGHT_SQUARE / 2;
    seen[(size_t)y * terrain->width + (size_t)x] = 1;
    for (int32_t ring = 1; ring <= SIGHT_FURTHEST; ring++) {
        bool any = false;
        /* The four sides of the ring, a corner to each. */
        for (int32_t along = -ring; along < ring; along++) {
            any |= look(terrain, x, y, along, -ring, left, seen);
            any |= look(terrain, x, y, ring, along, left, seen);
            any |= look(terrain, x, y, -along, ring, left, seen);
            any |= look(terrain, x, y, -ring, -along, left, seen);
        }
        if (!any) {
            return;
        }
    }
}
