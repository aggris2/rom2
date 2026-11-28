#include "game/map_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "base/file.h"
#include "data/map.h"
#include "data/structure_kind.h"
#include "data/unit_kind.h"
#include "game/sheets.h"
#include "mechanics/clock.h"
#include "mechanics/random.h"
#include "mechanics/units.h"
#include "view/camera.h"
#include "view/ground.h"
#include "view/light.h"
#include "view/objects.h"
#include "view/structures.h"
#include "view/units.h"

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
    bool grid;                  /* whether the grid is shown */
    Clock clock;                /* the ticks of the game */
    Units live;                 /* the units of the game */
    Random random;              /* its dice */
    uint8_t *light;             /* the light at each corner of the map */
    ObjectLook objects[OBJECT_KINDS];   /* how each kind of object looks */
    StructureLook structures[STRUCTURE_KINDS];  /* and of structure */
    UnitLook units[UNIT_TYPES];                 /* and type of unit */
    UnitKind kinds[UNIT_KIND_IDS];  /* each kind of unit, by its ID */
    GroundSquare *ground;       /* room for a frame's ground */
    size_t ground_room;
    Sprite *sprites;            /* room for a frame's sprites, twice: */
    size_t sprite_room;         /* the second half is to sort them with */
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

/* What a folder of the content holds: a folder for each kind, named
 * by the kind's ID, three digits, and then its name, with the kind's
 * definition in it. */
typedef struct Kinds {
    const char *folder;         /* in the content */
    const char *definition;     /* the file's name */
    /* Makes the look of kind `id` from its definition, whose top is
     * `top` and which is at `path`. A kind it cannot use it leaves
     * unknown, with a message. */
    void (*add)(MapScreen *screen, int id, const char *path,
                const TomlTable *top);
} Kinds;

/* Reads the definitions of the kinds the map has on it: those of
 * `used`, which has an entry for each of `most` IDs.
 *
 * A kind that has no folder, or whose sheet cannot be read, is left
 * unknown and is not drawn: the original has a few objects without a
 * picture. Returns false only if the folder cannot be listed. */
static bool add_kinds(MapScreen *screen, const char *content,
                      const Kinds *kinds, const bool *used, int most)
{
    char folder[1024];
    char *names = NULL;
    size_t count = 0;
    int written = snprintf(folder, sizeof folder, "%s/%s", content,
        kinds->folder);
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
        written = snprintf(path, sizeof path, "%s/%s/%s", folder, name,
            kinds->definition);
        TomlTable *top = NULL;
        if (id >= most || !used[id] || written < 0
            || (size_t)written >= sizeof path
            || !sheets_read_definition(path, &top)) {
            continue;
        }
        kinds->add(screen, id, path, top);
        toml_free(top);
    }
    free(names);
    return true;
}

/* Reads the sheet of a definition that is a sprite sheet and no more,
 * and puts it on the card. */
static bool add_sheet(MapScreen *screen, const char *path,
                      const TomlTable *top, SpriteSheet *layout,
                      uint16_t *sheet)
{
    if (!sprite_sheet_read(top, layout)) {
        fprintf(stderr, "rom2: %s has no usable [sprite] part\n", path);
        return false;
    }
    return sheets_load_sprites(screen->renderer, path, layout, sheet);
}

static void add_object(MapScreen *screen, int id, const char *path,
                       const TomlTable *top)
{
    SpriteSheet layout;
    ObjectLook *look = &screen->objects[id];
    if (add_sheet(screen, path, top, &layout, &look->sheet)) {
        look->known = true;
        look->cell_width = layout.cell_width;
        look->cell_height = layout.cell_height;
        look->anchor_x = layout.anchor_x;
        look->anchor_y = layout.anchor_y;
        look->round = layout.round;
    }
}

/* A structure is left unknown too if its sheet's cell is not its
 * picture: as wide as its base and as high as its definition says. */
static void add_structure(MapScreen *screen, int id, const char *path,
                          const TomlTable *top)
{
    StructureKind kind;
    StructureLook *look = &screen->structures[id];
    if (!structure_kind_read(top, &kind)
        || kind.sheet.cell_width != kind.width * MAP_SQUARE
        || kind.sheet.cell_height != kind.height * MAP_SQUARE) {
        fprintf(stderr, "rom2: %s is not a usable structure\n", path);
    } else if (sheets_load_sprites(screen->renderer, path, &kind.sheet,
            &look->sheet)) {
        look->known = true;
        look->width = kind.width;
        look->depth = kind.depth;
        look->height = kind.height;
        look->round = kind.sheet.round;
    }
}

static void add_unit(MapScreen *screen, int id, const char *path,
                     const TomlTable *top)
{
    SpriteSheet layout;
    UnitLook *look = &screen->units[id];
    if (add_sheet(screen, path, top, &layout, &look->sheet)) {
        look->known = true;
        look->cell_width = layout.cell_width;
        look->cell_height = layout.cell_height;
        look->anchor_x = layout.anchor_x;
        look->anchor_y = layout.anchor_y;
        /* The definition says what a unit's palettes go by. */
        const char *by = NULL;
        look->by_player = toml_get_text(toml_get_table(top, "sprite"),
            "palette_by", &by) && strcmp(by, "player") == 0;
        if (!unit_animation_read(top, "idle", &look->idle)) {
            fprintf(stderr, "rom2: %s: its idle animation is not usable\n",
                path);
        }
    }
    size_t count = toml_get_table_count(top, "kind");
    for (size_t i = 0; i < count; i++) {
        UnitKind kind;
        if (unit_kind_read(toml_get_table_at(top, "kind", i), &kind)) {
            screen->kinds[kind.id] = kind;
        } else {
            fprintf(stderr, "rom2: %s: kind %zu is not usable\n", path, i);
        }
    }
}

/* Reads the sheets of what the map has on it: its objects, its
 * structures and its units. */
static bool add_looks(MapScreen *screen, const char *content)
{
    const Map *map = &screen->map;
    bool objects[OBJECT_KINDS] = {false};
    bool structures[STRUCTURE_KINDS] = {false};
    bool units[UNIT_TYPES] = {false};
    size_t squares = (size_t)map->info.width * map->info.height;
    for (size_t i = 0; i < squares; i++) {
        if (map->objects[i] != 0) {
            objects[map->objects[i] - 1] = true;
        }
    }
    for (uint32_t i = 0; i < map->structure_count; i++) {
        if (map->structures[i].type < STRUCTURE_KINDS) {
            structures[map->structures[i].type] = true;
        }
    }
    for (uint32_t i = 0; i < map->unit_count; i++) {
        if (map->units[i].type < UNIT_TYPES) {
            units[map->units[i].type] = true;
        }
    }
    const Kinds of_objects = {"objects", "object.toml", add_object};
    const Kinds of_structures
        = {"structures", "structure.toml", add_structure};
    const Kinds of_units = {"units", "unit.toml", add_unit};
    return add_kinds(screen, content, &of_objects, objects, OBJECT_KINDS)
        && add_kinds(screen, content, &of_structures, structures,
            STRUCTURE_KINDS)
        && add_kinds(screen, content, &of_units, units, UNIT_TYPES);
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
    if (!add_tiles(screen, content) || !add_looks(screen, content)) {
        map_screen_close(screen);
        return NULL;
    }
    /* Every game starts its dice the same until there is a reason not
     * to, such as a game the player starts anew. */
    random_start(&screen->random, 1);
    if (!units_from_map(&screen->map, screen->kinds, &screen->live)) {
        fprintf(stderr, "rom2: out of memory\n");
        map_screen_close(screen);
        return NULL;
    }
    start_camera(screen);
    clock_start(&screen->clock);
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
    /* The ticks that pass with this frame. The camera is the player's
     * and not the game's, and moves while the game is paused too. */
    for (uint32_t ticks = clock_advance(&screen->clock, seconds);
        ticks > 0; ticks--) {
        units_tick(&screen->live, &screen->random);
    }
    move_camera(screen, seconds);
    Camera camera = camera_now(screen);
    size_t most = ground_most(&camera);
    if (most > screen->ground_room) {
        GroundSquare *ground = realloc(screen->ground,
            most * sizeof *ground);
        if (ground == NULL) {
            fprintf(stderr, "rom2: out of memory\n");
            return false;
        }
        screen->ground = ground;
        screen->ground_room = most;
    }
    most = objects_most(&camera, screen->objects)
        + structures_most(&screen->map, screen->structures)
        + units_most(&screen->live);
    if (most > screen->sprite_room) {
        Sprite *sprites = realloc(screen->sprites,
            2 * most * sizeof *sprites);
        if (sprites == NULL) {
            fprintf(stderr, "rom2: out of memory\n");
            return false;
        }
        screen->sprites = sprites;
        screen->sprite_room = most;
    }
    Frame frame;
    frame.ground_grid = screen->grid;
    frame.ground_sheet = screen->tiles;
    frame.ground = screen->ground;
    uint64_t tick = screen->clock.tick;
    frame.ground_count = ground_squares(&screen->map, screen->light, tick,
        &camera, screen->tiles_width, screen->tiles_height, screen->ground);
    frame.sprites = screen->sprites;
    frame.sprite_count = objects_sprites(&screen->map, screen->objects,
        tick, &camera, screen->sprites);
    frame.sprite_count += structures_sprites(&screen->map,
        screen->structures, tick, &camera,
        screen->sprites + frame.sprite_count);
    frame.sprite_count += units_sprites(&screen->map, &screen->live,
        screen->units, screen->kinds, tick, &camera,
        screen->sprites + frame.sprite_count);
    sprites_sort(screen->sprites, frame.sprite_count,
        screen->sprites + screen->sprite_room);
    if (!renderer_draw(screen->renderer, &frame)) {
        fprintf(stderr, "rom2: cannot draw: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void map_screen_key(MapScreen *screen, SDL_Keycode key)
{
    if (key == SDLK_G) {
        screen->grid = !screen->grid;
    } else if (key == SDLK_P) {
        screen->clock.paused = !screen->clock.paused;
    }
}

void map_screen_close(MapScreen *screen)
{
    if (screen == NULL) {
        return;
    }
    units_free(&screen->live);
    map_free(&screen->map);
    free(screen->light);
    free(screen->ground);
    free(screen->sprites);
    free(screen);
}
