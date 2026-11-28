#include "data/map.h"

#include <stdlib.h>
#include <string.h>

#include "base/reader.h"

#define SIGNATURE 0x0052374du
#define HEADER_SIZE 20u
/* The data of the first section is 16 bytes longer than its header
 * says: that is so in every map of the original. */
#define INFO_SIZE 660u
#define INFO_SIZE_STATED 644u

enum {
    SECTION_INFO = 0,
    SECTION_GROUND = 1,
    SECTION_HEIGHTS = 2,
    SECTION_OBJECTS = 3,
    SECTION_STRUCTURES = 4,
    SECTION_PLAYERS = 5,
    SECTION_UNITS = 6,
    SECTION_SCRIPT = 7
};

/* The one structure whose record is 8 bytes longer, with its size: the
 * bridge of variable size. Newer maps also mark it with a bit. */
#define TYPE_BRIDGE 33u

/* An action of the script, and the type of one that says where a
 * player enters the map. */
#define ACTION_SIZE 796u
#define ACTION_DROP 65538u
#define TYPE_HAS_SIZE 0x01000000u

/* Copies a text of `size` bytes, which may or may not have a closing
 * zero, into `out`, which has room for `size + 1`. */
static void read_text(Reader *r, size_t size, char *out)
{
    const uint8_t *bytes = reader_bytes(r, size);
    size_t length = 0;
    while (bytes != NULL && length < size && bytes[length] != 0) {
        out[length] = (char)bytes[length];
        length++;
    }
    out[length] = '\0';
}

/* A decimal number of 32 bits, as the file has it. */
static float read_float(Reader *r)
{
    uint32_t bits = reader_u32(r);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

/* The counts the first section gives for the others. */
typedef struct Counts {
    uint32_t players;
    uint32_t structures;
    uint32_t units;
} Counts;

static bool read_info(Reader *r, MapInfo *info, Counts *counts)
{
    info->width = reader_u32(r);
    info->height = reader_u32(r);
    info->sun_angle = read_float(r);
    info->time_of_day = reader_u32(r);
    info->darkness = reader_u32(r);
    info->contrast = reader_u32(r);
    info->ground_used = reader_u32(r);
    counts->players = reader_u32(r);
    counts->structures = reader_u32(r);
    counts->units = reader_u32(r);
    /* Seven counts for the sections this does not read. */
    reader_skip(r, 28);
    read_text(r, MAP_NAME_SIZE - 1, info->name);
    info->for_players = reader_u32(r);
    info->level = reader_u32(r);
    reader_skip(r, 8);
    read_text(r, MAP_AUTHOR_SIZE - 1, info->author);
    return reader_ok(r) && info->width > 0 && info->height > 0
        && info->width <= MAP_MAX_SIDE && info->height <= MAP_MAX_SIDE
        && counts->players <= MAP_MAX_PLAYERS;
}

static bool read_ground(Reader *r, size_t squares, uint16_t **out)
{
    if (reader_remaining(r) != squares * 2 || *out != NULL) {
        return false;
    }
    uint16_t *ground = malloc(squares * sizeof *ground);
    if (ground == NULL) {
        return false;
    }
    for (size_t i = 0; i < squares; i++) {
        ground[i] = reader_u16(r);
    }
    *out = ground;
    return true;
}

static bool read_bytes(Reader *r, size_t squares, uint8_t **out)
{
    if (reader_remaining(r) != squares || *out != NULL) {
        return false;
    }
    uint8_t *bytes = malloc(squares);
    if (bytes == NULL) {
        return false;
    }
    memcpy(bytes, reader_bytes(r, squares), squares);
    *out = bytes;
    return true;
}

static bool read_structures(Reader *r, uint32_t count, Map *map)
{
    if (map->structures != NULL) {
        return false;
    }
    /* A record is at least 20 bytes, so a count the section cannot
     * hold is refused before memory is asked for. */
    if (count > reader_remaining(r) / 20) {
        return false;
    }
    MapStructure *list = calloc(count > 0 ? count : 1, sizeof *list);
    if (list == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        MapStructure *s = &list[i];
        /* A place is the square times 256, plus 128 for its middle. */
        s->x = reader_i32(r) >> 8;
        s->y = reader_i32(r) >> 8;
        uint32_t type = reader_u32(r);
        s->type = type & 0xffffu;
        s->health = reader_i16(r);
        s->player = reader_i32(r);
        s->number = reader_i16(r);
        if ((type & TYPE_HAS_SIZE) != 0 || s->type == TYPE_BRIDGE) {
            s->bridge_width = reader_i32(r);
            s->bridge_height = reader_i32(r);
        }
    }
    if (!reader_ok(r) || reader_remaining(r) != 0) {
        free(list);
        return false;
    }
    map->structures = list;
    map->structure_count = count;
    return true;
}

static bool read_players(Reader *r, uint32_t count, Map *map)
{
    if (map->players != NULL || reader_remaining(r) != (size_t)count * 76) {
        return false;
    }
    MapPlayer *list = calloc(count > 0 ? count : 1, sizeof *list);
    if (list == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        MapPlayer *p = &list[i];
        p->colour = reader_i32(r);
        p->flags = reader_u32(r);
        p->money = reader_i32(r);
        read_text(r, MAP_PLAYER_NAME_SIZE - 1, p->name);
        for (uint32_t k = 0; k < MAP_MAX_PLAYERS; k++) {
            p->stance[k] = reader_u16(r);
        }
    }
    map->players = list;
    map->player_count = count;
    return true;
}

static bool read_units(Reader *r, uint32_t count, Map *map)
{
    if (map->units != NULL || reader_remaining(r) != (size_t)count * 48) {
        return false;
    }
    MapUnit *list = calloc(count > 0 ? count : 1, sizeof *list);
    if (list == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        MapUnit *u = &list[i];
        u->x = reader_i32(r);
        u->y = reader_i32(r);
        u->type = reader_u16(r);
        u->face = reader_u16(r);
        u->flags = reader_u32(r);
        u->quest_flags = reader_u32(r);
        u->kind = reader_i32(r);
        u->player = reader_i32(r);
        u->sack = reader_i32(r);
        u->angle = reader_i32(r);
        u->health = reader_i16(r);
        u->health_max = reader_i16(r);
        /* The number is 16 bits, and 16 more that mean nothing. */
        u->number = reader_u16(r);
        reader_skip(r, 2);
        u->group = reader_i32(r);
    }
    map->units = list;
    map->unit_count = count;
    return true;
}

/* The script begins with its actions. Of them this keeps the squares
 * where a player enters the map, and reads no further. */
static bool read_script(Reader *r, Map *map)
{
    if (map->drops != NULL) {
        return false;
    }
    uint32_t count = reader_u32(r);
    if (!reader_ok(r) || count > reader_remaining(r) / ACTION_SIZE) {
        return false;
    }
    MapDrop *list = calloc(count > 0 ? count : 1, sizeof *list);
    if (list == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        /* Its name, its type, its index and whether it runs once, and
         * then its values: the first two of this one are the square. */
        reader_skip(r, 64);
        uint32_t type = reader_u32(r);
        reader_skip(r, 8);
        int32_t x = reader_i32(r);
        int32_t y = reader_i32(r);
        reader_skip(r, ACTION_SIZE - 84);
        if (type == ACTION_DROP && x >= 0 && y >= 0
            && (uint32_t)x < map->info.width
            && (uint32_t)y < map->info.height) {
            list[map->drop_count].x = x;
            list[map->drop_count].y = y;
            map->drop_count++;
        }
    }
    map->drops = list;
    return true;
}

bool map_read(const uint8_t *data, size_t size, Map *out)
{
    Reader r = reader_init(data, size);
    uint32_t signature = reader_u32(&r);
    uint32_t header_size = reader_u32(&r);
    reader_skip(&r, 4);
    uint32_t sections = reader_u32(&r);
    uint32_t version = reader_u32(&r);
    if (!reader_ok(&r) || signature != SIGNATURE
        || header_size != HEADER_SIZE) {
        return false;
    }

    Map map;
    Counts counts = {0, 0, 0};
    bool has_info = false;
    bool ok = true;
    memset(&map, 0, sizeof map);
    map.version = version;
    for (uint32_t i = 0; ok && i < sections; i++) {
        reader_skip(&r, 4);
        uint32_t section_header = reader_u32(&r);
        uint32_t stated = reader_u32(&r);
        uint32_t id = reader_u32(&r);
        reader_skip(&r, 4);
        if (!reader_ok(&r) || section_header != HEADER_SIZE) {
            ok = false;
            break;
        }
        size_t body_size = stated;
        if (id == SECTION_INFO) {
            if (stated != INFO_SIZE_STATED || has_info) {
                ok = false;
                break;
            }
            body_size = INFO_SIZE;
        }
        Reader body = reader_sub(&r, body_size);
        if (!reader_ok(&r)) {
            ok = false;
            break;
        }
        size_t squares = (size_t)map.info.width * map.info.height;
        if (id == SECTION_INFO) {
            ok = read_info(&body, &map.info, &counts);
            has_info = ok;
        } else if (id > SECTION_SCRIPT) {
            /* A section this does not read. */
        } else if (!has_info) {
            /* The others cannot be read before the map's size and
             * counts are known. */
            ok = false;
        } else if (id == SECTION_GROUND) {
            ok = read_ground(&body, squares, &map.ground);
        } else if (id == SECTION_HEIGHTS) {
            ok = read_bytes(&body, squares, &map.heights);
        } else if (id == SECTION_OBJECTS) {
            ok = read_bytes(&body, squares, &map.objects);
        } else if (id == SECTION_STRUCTURES) {
            ok = read_structures(&body, counts.structures, &map);
        } else if (id == SECTION_PLAYERS) {
            ok = read_players(&body, counts.players, &map);
        } else if (id == SECTION_UNITS) {
            ok = read_units(&body, counts.units, &map);
        } else if (id == SECTION_SCRIPT) {
            ok = read_script(&body, &map);
        }
    }
    /* A map without structures, players or units may leave out their
     * sections. The ground, the heights and the objects it must have. */
    ok = ok && map.ground != NULL && map.heights != NULL
        && map.objects != NULL
        && map.structure_count == counts.structures
        && map.player_count == counts.players
        && map.unit_count == counts.units;
    if (!ok) {
        map_free(&map);
        return false;
    }
    *out = map;
    return true;
}

void map_free(Map *map)
{
    free(map->ground);
    free(map->heights);
    free(map->objects);
    free(map->structures);
    free(map->players);
    free(map->units);
    free(map->drops);
    memset(map, 0, sizeof *map);
}
