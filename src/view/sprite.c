#include "view/sprite.h"

#include <string.h>

#include "data/map.h"

uint32_t sprite_order(int32_t column, int32_t row)
{
    return (uint32_t)row * MAP_MAX_SIDE
        + (MAP_MAX_SIDE - 1u - (uint32_t)column);
}

void sprites_sort(Sprite *sprites, size_t count, Sprite *spare)
{
    /* Runs that are in order are merged two by two into runs twice as
     * long, from the one list into the other and back. */
    Sprite *from = sprites;
    Sprite *to = spare;
    for (size_t run = 1; run < count; run *= 2) {
        for (size_t start = 0; start < count; start += 2 * run) {
            size_t middle = start + run < count ? start + run : count;
            size_t end = start + 2 * run < count ? start + 2 * run : count;
            size_t left = start;
            size_t right = middle;
            for (size_t i = start; i < end; i++) {
                /* The left one first when the two are equal. */
                if (left < middle && (right >= end
                        || from[left].order <= from[right].order)) {
                    to[i] = from[left++];
                } else {
                    to[i] = from[right++];
                }
            }
        }
        Sprite *swap = from;
        from = to;
        to = swap;
    }
    if (from != sprites) {
        memcpy(sprites, from, count * sizeof *sprites);
    }
}
