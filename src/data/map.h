/* Reading a map: an .alm file, which ROM2 takes as the original has it.
 *
 * A map is its ground, the height of the ground, what stands on it and
 * who plays on it. The file is a header and sections, each with a
 * number that says what it holds. This reads the sections for the map
 * as a whole, the ground, the heights, the objects, the structures, the
 * players and the units, and of the script of the map where a player
 * enters it. The rest of the script and what goes with it are passed
 * over for now.
 *
 * The layout is described in docs/formats/alm.md.
 */
#ifndef ROM2_DATA_MAP_H
#define ROM2_DATA_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The side of a square of a map, in pixels. Heights are in the same
 * pixels. */
#define MAP_SQUARE 32

/* The most a height raises the ground, in pixels. */
#define MAP_HIGHEST 127

/* The squares along each edge of a map that are never shown and never
 * walked on. */
#define MAP_EDGE 8

/* The most squares a map may have along one side. */
#define MAP_MAX_SIDE 256u

/* The most players a map may have. A player has a number for each of
 * the others, and there is room for 16. */
#define MAP_MAX_PLAYERS 16u

/* The room a name needs, with its closing zero. */
#define MAP_NAME_SIZE 65
#define MAP_AUTHOR_SIZE 513
#define MAP_PLAYER_NAME_SIZE 33

/* What a map says of itself. */
typedef struct MapInfo {
    uint32_t width;        /* in squares */
    uint32_t height;
    float sun_angle;       /* in radians */
    uint32_t time_of_day;
    uint32_t darkness;
    uint32_t contrast;
    uint32_t ground_used;  /* a bit for each family of ground */
    char name[MAP_NAME_SIZE];
    uint32_t for_players;  /* how many players it is meant for */
    uint32_t level;
    char author[MAP_AUTHOR_SIZE];
} MapInfo;

/* A structure on the map. */
typedef struct MapStructure {
    int32_t x;             /* the top left square of its base */
    int32_t y;
    uint32_t type;         /* its ID in structures.reg */
    int32_t health;
    int32_t player;        /* counted from 1 */
    int32_t number;        /* its number in the map */
    /* For a bridge of variable size, its size in squares. 0 for the
     * others. */
    int32_t bridge_width;
    int32_t bridge_height;
} MapStructure;

/* A player of the map. */
typedef struct MapPlayer {
    int32_t colour;
    uint32_t flags;        /* not read yet */
    int32_t money;
    char name[MAP_PLAYER_NAME_SIZE];
    /* How it stands to each player, by bits. */
    uint16_t stance[MAP_MAX_PLAYERS];
} MapPlayer;

/* A unit on the map. */
typedef struct MapUnit {
    int32_t x;             /* in 256ths of a square */
    int32_t y;
    uint32_t type;         /* its ID in units.reg */
    uint32_t face;
    uint32_t flags;
    uint32_t quest_flags;
    int32_t kind;          /* the id of its kind */
    int32_t player;        /* counted from 1 */
    int32_t sack;          /* the number of its sack from 1, 0 for none */
    int32_t angle;         /* the standing step it faces, 0 to 15 */
    int32_t health;        /* -1 for what its kind has */
    int32_t health_max;
    uint32_t number;       /* its number in the map */
    int32_t group;
} MapUnit;

/* A square where a player enters the map: a hero is put on one. */
typedef struct MapDrop {
    int32_t x;             /* in squares */
    int32_t y;
} MapDrop;

/* A map, read. The lists are owned. */
typedef struct Map {
    uint32_t version;
    MapInfo info;
    /* A value for each square, row after row from the top: the ground's
     * number, the height of the square's top left corner, and the
     * object on it, which is 0 for none and an object's ID plus 1. */
    uint16_t *ground;
    uint8_t *heights;
    uint8_t *objects;
    MapStructure *structures;
    uint32_t structure_count;
    MapPlayer *players;
    uint32_t player_count;
    MapUnit *units;
    uint32_t unit_count;
    /* In the order of the script. One outside the map is left out. */
    MapDrop *drops;
    uint32_t drop_count;
} Map;

/* Reads a map held in memory.
 *
 * Returns false, and leaves `*out` alone, if the bytes are not a map,
 * are cut short, a section is not as large as the map's own numbers
 * say, the map has no size or is larger than MAP_MAX_SIDE, it has more
 * players than MAP_MAX_PLAYERS, it lacks its ground, heights or
 * objects, its script says it has more actions than it holds, or
 * memory runs out. */
bool map_read(const uint8_t *data, size_t size, Map *out);

/* Frees the lists of a map and sets it to zero. */
void map_free(Map *map);

#endif
