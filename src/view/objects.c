#include "view/objects.h"

#include "view/ground.h"

/* The squares around the screen that an object may reach in from: how
 * far to the left, to the right, up and down, in squares. */
typedef struct Reach {
    int32_t left;
    int32_t right;
    int32_t up;
    int32_t down;
} Reach;

/* The square a pixel of the map is in, also left of and above the
 * map, where plain division would round the wrong way. */
static int32_t square_of(int32_t pixel)
{
    return pixel >= 0 ? pixel / MAP_SQUARE
        : -((-pixel + MAP_SQUARE - 1) / MAP_SQUARE);
}

/* Pixels as whole squares, rounded up, and never less than none. */
static int32_t squares(int32_t pixels)
{
    return pixels <= 0 ? 0 : (pixels + MAP_SQUARE - 1) / MAP_SQUARE;
}

/* How far the largest of the known objects reaches from the middle of
 * its square. An object is drawn from its anchor, and raised by up to
 * MAP_HIGHEST, so one below the screen reaches further up. */
static Reach reach_of(const ObjectLook *looks)
{
    int32_t left = 0;
    int32_t right = 0;
    int32_t up = 0;
    int32_t down = 0;
    for (int i = 0; i < OBJECT_KINDS; i++) {
        const ObjectLook *look = &looks[i];
        if (!look->known) {
            continue;
        }
        int32_t to_left = look->anchor_x;
        int32_t to_right = (int32_t)look->cell_width - look->anchor_x;
        int32_t to_top = look->anchor_y;
        int32_t to_bottom = (int32_t)look->cell_height - look->anchor_y;
        left = to_left > left ? to_left : left;
        right = to_right > right ? to_right : right;
        up = to_top > up ? to_top : up;
        down = to_bottom > down ? to_bottom : down;
    }
    /* A square is looked at if an object on it could show on the
     * screen: one to the right of the screen reaches in by its left
     * part, one below by its top and the height of the ground. */
    Reach reach;
    reach.left = squares(right - MAP_SQUARE / 2);
    reach.right = squares(left - MAP_SQUARE / 2);
    reach.up = squares(down - MAP_SQUARE / 2);
    reach.down = squares(up + MAP_HIGHEST - MAP_SQUARE / 2);
    return reach;
}

size_t objects_most(const Camera *camera, const ObjectLook *looks)
{
    if (camera->width <= 0 || camera->height <= 0) {
        return 0;
    }
    Reach reach = reach_of(looks);
    size_t across = (size_t)(camera->width / MAP_SQUARE) + 2
        + (size_t)reach.left + (size_t)reach.right;
    size_t down = (size_t)(camera->height / MAP_SQUARE) + 2
        + (size_t)reach.up + (size_t)reach.down;
    return across * down;
}

size_t objects_sprites(const Map *map, const ObjectLook *looks,
                       const Camera *camera, Sprite *out)
{
    if (camera->width <= 0 || camera->height <= 0) {
        return 0;
    }
    Reach reach = reach_of(looks);
    int32_t first_x = square_of(camera->left) - reach.left;
    int32_t first_y = square_of(camera->top) - reach.up;
    int32_t last_x = square_of(camera->left + camera->width - 1)
        + reach.right;
    int32_t last_y = square_of(camera->top + camera->height - 1)
        + reach.down;
    first_x = first_x < 0 ? 0 : first_x;
    first_y = first_y < 0 ? 0 : first_y;
    last_x = last_x >= (int32_t)map->info.width
        ? (int32_t)map->info.width - 1 : last_x;
    last_y = last_y >= (int32_t)map->info.height
        ? (int32_t)map->info.height - 1 : last_y;

    size_t count = 0;
    for (int32_t y = first_y; y <= last_y; y++) {
        for (int32_t x = last_x; x >= first_x; x--) {
            uint8_t named = map->objects[(size_t)y * map->info.width
                + (size_t)x];
            if (named == 0 || !looks[named - 1].known) {
                continue;
            }
            const ObjectLook *look = &looks[named - 1];
            int32_t middle_x = x * MAP_SQUARE + MAP_SQUARE / 2;
            int32_t middle_y = y * MAP_SQUARE + MAP_SQUARE / 2;
            int32_t left = middle_x - look->anchor_x - camera->left;
            int32_t top = middle_y - look->anchor_y - camera->top
                - ground_square_height(map, x, y);
            /* The squares looked at reach further than any one object:
             * leave out what is wholly off the screen. */
            if (left >= camera->width || top >= camera->height
                || left + (int32_t)look->cell_width <= 0
                || top + (int32_t)look->cell_height <= 0) {
                continue;
            }
            Sprite *sprite = &out[count++];
            sprite->sheet = look->sheet;
            sprite->source_x = 0;
            sprite->source_y = 0;
            sprite->width = look->cell_width;
            sprite->height = look->cell_height;
            sprite->x = (int16_t)left;
            sprite->y = (int16_t)top;
            sprite->light = LIGHT_STANDING;
            sprite->palette = 0;
            sprite->order = sprite_order(x, y);
        }
    }
    return count;
}
