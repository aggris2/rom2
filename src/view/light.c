#include "view/light.h"

#include <math.h>

/* How strongly a slope changes the light, by day: the number the
 * original calls contrast. Like the darkness it changes with the time
 * of day, which is not used yet. */
#define CONTRAST 32.0

#define SIXTH_OF_PI 0.52359877559829887

/* The height at a corner. One outside the map counts as the nearest
 * inside it. */
static double height_at(const Map *map, int64_t x, int64_t y)
{
    int64_t width = map->info.width;
    int64_t height = map->info.height;
    x = x < 0 ? 0 : x >= width ? width - 1 : x;
    y = y < 0 ? 0 : y >= height ? height - 1 : y;
    return map->heights[y * width + x];
}

/* How dark a slope is that rises by `rise` towards the sun: 0 to 94. */
static double darkness_of(double rise, double reach)
{
    double base = (double)LIGHT_DARKNESS + CONTRAST / 2.0 + 32.0;
    double value = base - sin(SIXTH_OF_PI - atan2(rise, reach)) * CONTRAST;
    return value < 0.0 ? 0.0 : value > 94.0 ? 94.0 : value;
}

void light_ground(const Map *map, uint8_t *levels)
{
    double angle = map->info.sun_angle;
    double lean = tan(angle);
    double reach = (double)MAP_SQUARE / cos(angle);
    for (int64_t y = 0; y < (int64_t)map->info.height; y++) {
        for (int64_t x = 0; x < (int64_t)map->info.width; x++) {
            double here = height_at(map, x, y);
            double below = height_at(map, x, y + 1);
            double above = height_at(map, x, y - 1);
            /* The height the sun's ray comes from, in the row below and
             * in the row above. */
            double from_below;
            double from_above;
            if (angle >= 0.0) {
                from_below = below
                    - (below - height_at(map, x - 1, y + 1)) * lean;
                from_above = above
                    + (height_at(map, x + 1, y - 1) - above) * lean;
            } else {
                from_below = below
                    + (height_at(map, x + 1, y + 1) - below) * lean;
                from_above = above
                    - (above - height_at(map, x - 1, y - 1)) * lean;
            }
            double dark = (darkness_of(from_below - here, reach)
                + darkness_of(here - from_above, reach)) / 2.0;
            levels[y * (int64_t)map->info.width + x]
                = (uint8_t)(LIGHT_MAX - (unsigned)dark);
        }
    }
}
