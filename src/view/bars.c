#include "view/bars.h"

#include "view/frame.h"

/* A bar is this many lines high, and the sprites of one with its
 * ground and its balls. */
#define BAR_HEIGHT 4
#define BAR_SPRITES (2 * BAR_HEIGHT + 2)

Rgb bars_colour(BarLine line)
{
    static const Rgb colours[BAR_LINES] = {
        {64, 64, 64}, {128, 128, 128}, {96, 96, 96},
        {0, 255, 0}, {0, 192, 0}, {0, 128, 0},
        {255, 255, 0}, {192, 192, 0}, {128, 128, 0},
        {255, 0, 0}, {192, 0, 0}, {128, 0, 0},
        {0, 0, 255}, {0, 0, 192}, {0, 0, 128}
    };
    return colours[line];
}

size_t bars_most(const Units *units)
{
    /* One of health and one of mana. */
    return (size_t)units->living * 2 * BAR_SPRITES;
}

/* A part of the sheet at a place of the screen. */
static Sprite piece(const BarsLook *bars, int32_t row, int32_t width,
                    int32_t height, int32_t x, int32_t y)
{
    Sprite sprite;
    sprite.sheet = bars->sheet;
    sprite.source_x = 0;
    sprite.source_y = (uint16_t)row;
    sprite.width = (uint16_t)width;
    sprite.height = (uint16_t)height;
    sprite.x = (int16_t)x;
    sprite.y = (int16_t)y;
    sprite.light = FRAME_LIGHT_WHOLE;
    sprite.palette = 0;
    sprite.order = SPRITE_ORDER_BARS;
    return sprite;
}

/* Writes one bar: `left` of `most`, in the three lines from `bright`,
 * between `x` and `x + width` with its top at `y`. */
static size_t bar_sprites(const BarsLook *bars, BarLine bright,
                          int32_t left, int32_t most, bool faint,
                          int32_t x, int32_t width, int32_t y, Sprite *out)
{
    size_t count = 0;
    int32_t inner = width - 2 * BARS_BALL;
    inner = inner < 1 ? 1 : inner > BARS_LONGEST ? BARS_LONGEST : inner;
    left = left < 0 ? 0 : left > most ? most : left;
    int32_t filled = (int32_t)((int64_t)inner * left / most);
    filled = filled < 1 && left > 0 ? 1 : filled;
    int32_t from = x + BARS_BALL;
    if (!faint) {
        const BarLine ground[BAR_HEIGHT] = {BAR_GROUND_DARK,
            BAR_GROUND_LIGHT, BAR_GROUND_MIDDLE, BAR_GROUND_DARK};
        for (int32_t i = 0; i < BAR_HEIGHT; i++) {
            out[count++] = piece(bars, (int32_t)ground[i], inner, 1, from,
                y + i);
        }
    }
    if (filled > 0) {
        /* Dark, bright, the one between, dark. */
        int32_t first = (int32_t)bright;
        const int32_t lines[BAR_HEIGHT] = {first + 2, first, first + 1,
            first + 2};
        for (int32_t i = 0; i < BAR_HEIGHT; i++) {
            out[count++] = piece(bars, lines[i] + (faint ? BAR_LINES : 0),
                filled, 1, from, y + i);
        }
    }
    out[count++] = piece(bars, 2 * BAR_LINES, BARS_BALL, BARS_BALL, x, y);
    out[count++] = piece(bars, 2 * BAR_LINES, BARS_BALL, BARS_BALL,
        x + width - BARS_BALL, y);
    return count;
}

size_t bars_sprites(const Map *map, const Units *units,
                    const UnitLook *looks, const BarsLook *bars,
                    const UnitId *selected, bool all, uint32_t between,
                    const Camera *camera, Sprite *out)
{
    size_t count = 0;
    if (!bars->known) {
        return 0;
    }
    for (uint32_t slot = 0; slot < units->count; slot++) {
        UnitId id;
        int32_t at_x = 0;
        int32_t at_y = 0;
        const Unit *unit = units_at(units, slot, &id);
        bool chosen = unit != NULL && selected != NULL
            && selected->slot == id.slot && selected->age == id.age;
        const UnitLook *look = unit == NULL || (!all && !chosen) ? NULL
            : unit_on_screen(map, unit, looks, between, camera, &at_x,
                &at_y);
        if (look == NULL) {
            continue;
        }
        int32_t x = at_x + look->selection.left;
        int32_t width = look->selection.right - look->selection.left;
        int32_t y = at_y + look->selection.top - BAR_HEIGHT / 2;
        if (width < 2 * BARS_BALL || x >= camera->width || x + width <= 0
            || y >= camera->height || y + 2 * BAR_HEIGHT <= 0) {
            continue;
        }
        if (unit->health_most > 0) {
            BarLine bright = unit->health < unit->health_most / 4
                ? BAR_RED_BRIGHT : unit->health < unit->health_most / 2
                ? BAR_YELLOW_BRIGHT : BAR_GREEN_BRIGHT;
            count += bar_sprites(bars, bright, unit->health,
                unit->health_most, !chosen, x, width, y, out + count);
        }
        if (unit->mana_most > 0) {
            count += bar_sprites(bars, BAR_BLUE_BRIGHT, unit->mana,
                unit->mana_most, !chosen, x, width, y + BAR_HEIGHT,
                out + count);
        }
    }
    return count;
}
