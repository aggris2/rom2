#include "game/map_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "base/file.h"
#include "data/map.h"
#include "game/sheets.h"
#include "view/camera.h"
#include "view/ground.h"
#include "view/light.h"
#include "view/objects.h"

/* How fast the keys move the camera, in pixels a second. In a frame
 * it moves a whole number of pixels, the nearest to this speed: the
 * same number every frame, so that the ground glides and does not
 * stutter. */
#define SCROLL_SPEED 900.0f

struct MapScreen {
    Renderer *renderer;
    Map map;
    uint16_t tiles;             /* the sheet of ground tiles */
    uint32_t tiles_width;
    uint32_t tiles_height;
    /* Where the camera is, kept apart from the camera itself: the keys
     * move it by less than a pixel in a frame. */
    float left;
    float top;
    uint8_t *light;             /* the light at each corner of the map */
    ObjectLook objects[OBJECT_KINDS];   /* how each kind of object looks */
    GroundCorner *ground;       /* room for a frame's ground */
    size_t ground_room;
    Sprite *sprites;            /* room for a frame's sprites */
    size_t sprite_room;
};

/* Reads a whole file, with a message if it cannot. */
static bool read_file(const char *path, uint8_t **data, size_t *size)
{
    if (!file_read_all(path, data, size)) {
        fprintf(stderr, "rom2: cannot read %s\n"
            "The content is looked for in the folder named with"
            " --content, or in `content`.\n", path);
        return false;
    }
    return true;
}

/* Reads the map `name` names: a file if it ends in ".alm", and if not
 * the mission of that number. */
static bool read_map(const char *content, const char *name, Map *map)
{
    char path[1024];
    size_t length = strlen(name);
    int written;
    if (length >= 4 && strcmp(name + length - 4, ".alm") == 0) {
        written = snprintf(path, sizeof path, "%s", name);
    } else if (length > 0 && length <= 3
        && strspn(name, "0123456789") == length) {
        written = snprintf(path, sizeof path,
            "%s/campaign/missions/%03d/map.alm", content, atoi(name));
    } else {
        fprintf(stderr, "rom2: %s is neither the number of a mission nor"
            " a map's file\n", name);
        return false;
    }
    uint8_t *data = NULL;
    size_t size = 0;
    if (written < 0 || (size_t)written >= sizeof path
        || !read_file(path, &data, &size)) {
        return false;
    }
    bool ok = map_read(data, size, map);
    free(data);
    if (!ok) {
        fprintf(stderr, "rom2: %s is not a map\n", path);
    }
    return ok;
}

/* Reads the sheet of ground tiles and puts it on the card. */
static bool add_tiles(MapScreen *screen, const char *content)
{
    char path[1024];
    int written = snprintf(path, sizeof path, "%s/terrain/tiles.png",
        content);
    return written >= 0 && (size_t)written < sizeof path
        && sheets_load_picture(screen->renderer, path, &screen->tiles,
            &screen->tiles_width, &screen->tiles_height);
}

/* Reads the sheets of the kinds of object the map has on it. A kind's
 * folder is named by its ID, three digits, and then its name.
 *
 * A kind that has no folder, or whose sheet cannot be read, is left
 * unknown and is not drawn: the original has a few objects without a
 * picture. Returns false only if the folder of objects cannot be
 * listed. */
static bool add_objects(MapScreen *screen, const char *content)
{
    bool used[OBJECT_KINDS] = {false};
    size_t squares = (size_t)screen->map.info.width
        * screen->map.info.height;
    for (size_t i = 0; i < squares; i++) {
        if (screen->map.objects[i] != 0) {
            used[screen->map.objects[i] - 1] = true;
        }
    }
    char folder[1024];
    char *names = NULL;
    size_t count = 0;
    int written = snprintf(folder, sizeof folder, "%s/objects", content);
    if (written < 0 || (size_t)written >= sizeof folder
        || !file_list_folder(folder, &names, &count)) {
        fprintf(stderr, "rom2: cannot list %s\n", folder);
        return false;
    }
    const char *name = names;
    for (size_t i = 0; i < count; i++, name += strlen(name) + 1) {
        /* Three digits and an underscore. */
        if (strspn(name, "0123456789") != 3 || name[3] != '_') {
            continue;
        }
        int id = atoi(name);
        char path[1024];
        written = snprintf(path, sizeof path, "%s/%s/object.toml", folder,
            name);
        if (id >= OBJECT_KINDS || !used[id] || written < 0
            || (size_t)written >= sizeof path) {
            continue;
        }
        SpriteSheet layout;
        ObjectLook *look = &screen->objects[id];
        if (sheets_load_sprites(screen->renderer, path, &layout,
                &look->sheet)) {
            look->known = true;
            look->cell_width = layout.cell_width;
            look->cell_height = layout.cell_height;
            look->anchor_x = layout.anchor_x;
            look->anchor_y = layout.anchor_y;
        }
    }
    free(names);
    return true;
}

/* The camera as it is now: where the screen has been moved to, at the
 * size the window has, and inside the map. */
static Camera camera_now(MapScreen *screen)
{
    Camera camera;
    camera.left = (int32_t)SDL_floorf(screen->left);
    camera.top = (int32_t)SDL_floorf(screen->top);
    renderer_size(screen->renderer, &camera.width, &camera.height);
    camera_keep_inside(&camera, screen->map.info.width,
        screen->map.info.height);
    /* A camera that was moved back takes its place with it, or the
     * keys would have to undo what was pushed past the edge. */
    if ((float)camera.left != SDL_floorf(screen->left)) {
        screen->left = (float)camera.left;
    }
    if ((float)camera.top != SDL_floorf(screen->top)) {
        screen->top = (float)camera.top;
    }
    return camera;
}

/* Starts the camera on the first unit of the first player, or on the
 * middle of a map that has none. */
static void start_camera(MapScreen *screen)
{
    const Map *map = &screen->map;
    int32_t x = (int32_t)map->info.width * MAP_SQUARE / 2;
    int32_t y = (int32_t)map->info.height * MAP_SQUARE / 2;
    for (uint32_t i = 0; i < map->unit_count; i++) {
        if (map->units[i].player == 1) {
            /* A unit's place is in 256ths of a square. */
            x = map->units[i].x * MAP_SQUARE / 256;
            y = map->units[i].y * MAP_SQUARE / 256;
            break;
        }
    }
    Camera camera;
    renderer_size(screen->renderer, &camera.width, &camera.height);
    /* The unit stands on the ground, which is raised up the screen. */
    camera_look_at(&camera, x, y - ground_height_at(map, x, y));
    screen->left = (float)camera.left;
    screen->top = (float)camera.top;
}

MapScreen *map_screen_open(Renderer *renderer, const char *content,
                           const char *name)
{
    MapScreen *screen = calloc(1, sizeof *screen);
    if (screen == NULL) {
        fprintf(stderr, "rom2: out of memory\n");
        return NULL;
    }
    screen->renderer = renderer;
    if (!read_map(content, name, &screen->map)) {
        free(screen);
        return NULL;
    }
    screen->light = malloc((size_t)screen->map.info.width
        * screen->map.info.height);
    if (screen->light == NULL) {
        fprintf(stderr, "rom2: out of memory\n");
        map_screen_close(screen);
        return NULL;
    }
    light_ground(&screen->map, screen->light);
    if (!add_tiles(screen, content) || !add_objects(screen, content)) {
        map_screen_close(screen);
        return NULL;
    }
    start_camera(screen);
    return screen;
}

/* Moves the camera by the keys that are held down. */
static void move_camera(MapScreen *screen, float seconds)
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    float step = SDL_roundf(SCROLL_SPEED * seconds);
    step = step < 1.0f ? 1.0f : step;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
        screen->left -= step;
    }
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
        screen->left += step;
    }
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
        screen->top -= step;
    }
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
        screen->top += step;
    }
}

bool map_screen_frame(MapScreen *screen, float seconds)
{
    move_camera(screen, seconds);
    Camera camera = camera_now(screen);
    size_t most = ground_most(&camera);
    if (most > screen->ground_room) {
        GroundCorner *ground = realloc(screen->ground,
            most * sizeof *ground);
        if (ground == NULL) {
            fprintf(stderr, "rom2: out of memory\n");
            return false;
        }
        screen->ground = ground;
        screen->ground_room = most;
    }
    most = objects_most(&camera, screen->objects);
    if (most > screen->sprite_room) {
        Sprite *sprites = realloc(screen->sprites, most * sizeof *sprites);
        if (sprites == NULL) {
            fprintf(stderr, "rom2: out of memory\n");
            return false;
        }
        screen->sprites = sprites;
        screen->sprite_room = most;
    }
    Frame frame;
    frame.ground_sheet = screen->tiles;
    frame.ground = screen->ground;
    frame.ground_count = ground_corners(&screen->map, screen->light,
        &camera, screen->tiles_width, screen->tiles_height, screen->ground);
    frame.sprites = screen->sprites;
    frame.sprite_count = objects_sprites(&screen->map, screen->objects,
        &camera, screen->sprites);
    if (!renderer_draw(screen->renderer, &frame)) {
        fprintf(stderr, "rom2: cannot draw: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void map_screen_close(MapScreen *screen)
{
    if (screen == NULL) {
        return;
    }
    map_free(&screen->map);
    free(screen->light);
    free(screen->ground);
    free(screen->sprites);
    free(screen);
}
