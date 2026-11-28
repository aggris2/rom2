#include "view/pointer.h"

#include "view/frame.h"

void pointer_scroll(int32_t x, int32_t y, int32_t width, int32_t height,
                    int32_t *across, int32_t *down)
{
    *across = x <= 0 ? -1 : x >= width - 1 ? 1 : 0;
    *down = y <= 0 ? -1 : y >= height - 1 ? 1 : 0;
}

PointerKind pointer_kind(int32_t across, int32_t down)
{
    /* A row for up, none and down, and in it left, none and right. */
    static const PointerKind kinds[3][3] = {
        {POINTER_UP_LEFT, POINTER_UP, POINTER_UP_RIGHT},
        {POINTER_LEFT, POINTER_PLAIN, POINTER_RIGHT},
        {POINTER_DOWN_LEFT, POINTER_DOWN, POINTER_DOWN_RIGHT}
    };
    int32_t row = down < 0 ? 0 : down > 0 ? 2 : 1;
    int32_t column = across < 0 ? 0 : across > 0 ? 2 : 1;
    return kinds[row][column];
}

Sprite pointer_sprite(const PointerLook *look, int32_t x, int32_t y)
{
    Sprite sprite;
    sprite.sheet = look->sheet;
    sprite.source_x = 0;
    sprite.source_y = 0;
    sprite.width = look->width;
    sprite.height = look->height;
    sprite.x = (int16_t)(x - look->tip_x);
    sprite.y = (int16_t)(y - look->tip_y);
    sprite.light = FRAME_LIGHT_WHOLE;
    sprite.palette = 0;
    sprite.order = SPRITE_ORDER_POINTER;
    return sprite;
}
