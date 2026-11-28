#include "data/map.h"

#include <string.h>

#include "utest.h"

#include "base/buffer.h"
#include "expect.h"

/* A map built by hand: 3 squares wide and 2 high, with two structures,
 * one of them a bridge, two players and one unit. Each test changes
 * what it is about before the map is put together. */
typedef struct MakeMap {
    uint32_t signature;
    uint32_t width;
    uint32_t height;
    uint32_t players;
    uint32_t structures;
    uint32_t units;
    uint32_t bridge_type;   /* the type of the second structure */
    uint32_t info_stated;   /* the size section 0 says it has */
    int leave_out;          /* the id of a section to leave out, or -1 */
    int twice;              /* the id of a section to add twice, or -1 */
    int longer;             /* the id of one to add a byte to, or -1 */
    bool info_last;         /* put section 0 after the others */
    /* What section 0 says there is, where that is not what there is:
     * each is added to the true number. */
    uint32_t more_width;
    uint32_t more_players;
    uint32_t more_structures;
    uint32_t more_units;
    uint32_t more_actions;
    ByteBuffer file;
} MakeMap;

static MakeMap make_map(void)
{
    MakeMap m;
    memset(&m, 0, sizeof m);
    m.signature = 0x0052374du;
    m.width = 3;
    m.height = 2;
    m.players = 2;
    m.structures = 2;
    m.units = 1;
    m.bridge_type = 33;
    m.info_stated = 644;
    m.leave_out = -1;
    m.twice = -1;
    m.longer = -1;
    return m;
}

static void put_text(ByteBuffer *b, const char *text, size_t size)
{
    size_t length = strlen(text);
    for (size_t i = 0; i < size; i++) {
        buffer_push(b, i < length ? (uint8_t)text[i] : 0);
    }
}

static void put_section(MakeMap *m, int id, uint32_t stated,
                        const ByteBuffer *body)
{
    int times = id == m->twice ? 2 : 1;
    if (id == m->leave_out) {
        return;
    }
    for (int i = 0; i < times; i++) {
        buffer_u32_le(&m->file, 7);
        buffer_u32_le(&m->file, 20);
        buffer_u32_le(&m->file, stated + (id == m->longer ? 1u : 0u));
        buffer_u32_le(&m->file, (uint32_t)id);
        buffer_u32_le(&m->file, 0);
        buffer_append(&m->file, body->data, body->size);
        if (id == m->longer) {
            buffer_push(&m->file, 0);
        }
    }
}

static void put_info(MakeMap *m)
{
    ByteBuffer b = {0};
    buffer_u32_le(&b, m->width + m->more_width);
    buffer_u32_le(&b, m->height);
    buffer_u32_le(&b, 0x3f000000u);  /* 0.5 */
    buffer_u32_le(&b, 360);
    buffer_u32_le(&b, 21);
    buffer_u32_le(&b, 63);
    buffer_u32_le(&b, 8191);
    buffer_u32_le(&b, m->players + m->more_players);
    buffer_u32_le(&b, m->structures + m->more_structures);
    buffer_u32_le(&b, m->units + m->more_units);
    put_text(&b, "", 28);
    put_text(&b, "Ford", 64);
    buffer_u32_le(&b, 4);
    buffer_u32_le(&b, 2);
    put_text(&b, "", 8);
    put_text(&b, "Somebody", 512);
    put_section(m, 0, m->info_stated, &b);
    buffer_free(&b);
}

static void put_structure(ByteBuffer *b, uint32_t x, uint32_t y,
                          uint32_t type, uint32_t health,
                          uint32_t player, uint32_t number)
{
    buffer_u32_le(b, x * 256 + 128);
    buffer_u32_le(b, y * 256 + 128);
    buffer_u32_le(b, type);
    put_u16(b, health);
    buffer_u32_le(b, player);
    put_u16(b, number);
}

/* An action of the script: of `type`, with these first two values. */
static void put_action(ByteBuffer *b, const char *name, uint32_t type,
                       uint32_t first, uint32_t second)
{
    put_text(b, name, 64);
    buffer_u32_le(b, type);
    buffer_u32_le(b, 1);
    buffer_u32_le(b, 0);
    buffer_u32_le(b, first);
    buffer_u32_le(b, second);
    put_text(b, "", 796 - 84);
}

/* The script: four actions, no conditions and no triggers. Two of the
 * actions say where a player enters the map; one more does too, but
 * names a square the map does not have. */
static void put_script(MakeMap *m)
{
    ByteBuffer b = {0};
    buffer_u32_le(&b, 4 + m->more_actions);
    put_action(&b, "Say", 3, 1, 1);
    put_action(&b, "Drop", 65538, 2, 1);
    put_action(&b, "Far", 65538, m->width, 0);
    put_action(&b, "Drop 2", 65538, 0, 0);
    buffer_u32_le(&b, 0);
    buffer_u32_le(&b, 0);
    put_section(m, 7, (uint32_t)b.size, &b);
    buffer_free(&b);
}

static void put_others(MakeMap *m)
{
    ByteBuffer b = {0};
    size_t squares = (size_t)m->width * m->height;
    for (size_t i = 0; i < squares; i++) {
        put_u16(&b, (uint32_t)(0x0100 + i));
    }
    put_section(m, 1, (uint32_t)b.size, &b);
    b.size = 0;
    for (size_t i = 0; i < squares; i++) {
        buffer_push(&b, (uint8_t)(10 + i));
    }
    put_section(m, 2, (uint32_t)b.size, &b);
    b.size = 0;
    for (size_t i = 0; i < squares; i++) {
        buffer_push(&b, i == 4 ? 7 : 0);
    }
    put_section(m, 3, (uint32_t)b.size, &b);

    b.size = 0;
    for (uint32_t i = 0; i < m->players; i++) {
        buffer_u32_le(&b, 5 + i);
        buffer_u32_le(&b, 2);
        buffer_u32_le(&b, 1000);
        put_text(&b, i == 0 ? "self" : "Enemy", 32);
        for (uint32_t k = 0; k < 16; k++) {
            put_u16(&b, k == i ? 18 : 1);
        }
    }
    put_section(m, 5, (uint32_t)b.size, &b);

    /* A section the reader passes over. */
    b.size = 0;
    put_text(&b, "", 12);
    put_section(m, 11, (uint32_t)b.size, &b);

    b.size = 0;
    put_structure(&b, 1, 0, 12, 100, 1, 3);
    put_structure(&b, 2, 1, m->bridge_type, 80, 2, 4);
    if ((m->bridge_type & 0xffffu) == 33
        || (m->bridge_type & 0x01000000u) != 0) {
        buffer_u32_le(&b, 3);
        buffer_u32_le(&b, 5);
    }
    put_section(m, 4, (uint32_t)b.size, &b);

    b.size = 0;
    buffer_u32_le(&b, 2 * 256 + 128);
    buffer_u32_le(&b, 1 * 256 + 128);
    put_u16(&b, 17);
    put_u16(&b, 3);
    buffer_u32_le(&b, 16);
    buffer_u32_le(&b, 4);
    buffer_u32_le(&b, 1234);
    buffer_u32_le(&b, 2);
    buffer_u32_le(&b, 1);
    buffer_u32_le(&b, 9);
    put_u16(&b, 0xffff);
    put_u16(&b, 0xffff);
    put_u16(&b, 301);
    put_u16(&b, 0xabcd);
    buffer_u32_le(&b, 6);
    put_section(m, 6, (uint32_t)b.size, &b);
    buffer_free(&b);
    put_script(m);
}

static void finish(MakeMap *m)
{
    buffer_u32_le(&m->file, m->signature);
    buffer_u32_le(&m->file, 20);
    buffer_u32_le(&m->file, 0);
    buffer_u32_le(&m->file, 0);  /* the count of sections, set below */
    buffer_u32_le(&m->file, 1300);
    if (m->info_last) {
        put_others(m);
        put_info(m);
    } else {
        put_info(m);
        put_others(m);
    }
    /* Count the sections that were put. */
    uint32_t count = 9;
    count -= m->leave_out >= 0 ? 1u : 0u;
    count += m->twice >= 0 ? 1u : 0u;
    m->file.data[12] = (uint8_t)count;
}

/* Puts the map together and reads it. */
static bool reads(MakeMap *m, Map *out)
{
    finish(m);
    bool ok = map_read(m->file.data, m->file.size, out);
    buffer_free(&m->file);
    return ok;
}

UTEST(map, reads_what_the_map_says_of_itself)
{
    MakeMap m = make_map();
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    EXPECT_EQ(1300u, map.version);
    EXPECT_EQ(3u, map.info.width);
    EXPECT_EQ(2u, map.info.height);
    EXPECT_EQ(0.5f, map.info.sun_angle);
    EXPECT_EQ(360u, map.info.time_of_day);
    EXPECT_EQ(21u, map.info.darkness);
    EXPECT_EQ(63u, map.info.contrast);
    EXPECT_EQ(8191u, map.info.ground_used);
    EXPECT_STREQ("Ford", map.info.name);
    EXPECT_EQ(4u, map.info.for_players);
    EXPECT_EQ(2u, map.info.level);
    EXPECT_STREQ("Somebody", map.info.author);
    map_free(&map);
}

UTEST(map, reads_the_ground_the_heights_and_the_objects)
{
    MakeMap m = make_map();
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    EXPECT_EQ(0x0100u, map.ground[0]);
    EXPECT_EQ(0x0105u, map.ground[5]);
    EXPECT_EQ(10u, map.heights[0]);
    EXPECT_EQ(15u, map.heights[5]);
    EXPECT_EQ(0u, map.objects[3]);
    EXPECT_EQ(7u, map.objects[4]);
    map_free(&map);
}

UTEST(map, reads_a_structure_and_the_size_of_a_bridge)
{
    MakeMap m = make_map();
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    ASSERT_EQ(2u, map.structure_count);
    EXPECT_EQ(1, map.structures[0].x);
    EXPECT_EQ(0, map.structures[0].y);
    EXPECT_EQ(12u, map.structures[0].type);
    EXPECT_EQ(100, map.structures[0].health);
    EXPECT_EQ(1, map.structures[0].player);
    EXPECT_EQ(3, map.structures[0].number);
    EXPECT_EQ(0, map.structures[0].bridge_width);
    EXPECT_EQ(2, map.structures[1].x);
    EXPECT_EQ(1, map.structures[1].y);
    EXPECT_EQ(33u, map.structures[1].type);
    EXPECT_EQ(3, map.structures[1].bridge_width);
    EXPECT_EQ(5, map.structures[1].bridge_height);
    map_free(&map);
}

UTEST(map, a_type_with_the_size_bit_has_a_size_and_loses_the_bit)
{
    MakeMap m = make_map();
    m.bridge_type = 0x01000000u | 40u;
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    EXPECT_EQ(40u, map.structures[1].type);
    EXPECT_EQ(3, map.structures[1].bridge_width);
    EXPECT_EQ(5, map.structures[1].bridge_height);
    map_free(&map);
}

UTEST(map, reads_a_player)
{
    MakeMap m = make_map();
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    ASSERT_EQ(2u, map.player_count);
    EXPECT_EQ(5, map.players[0].colour);
    EXPECT_EQ(2u, map.players[0].flags);
    EXPECT_EQ(1000, map.players[0].money);
    EXPECT_STREQ("self", map.players[0].name);
    EXPECT_EQ(18u, map.players[0].stance[0]);
    EXPECT_EQ(1u, map.players[0].stance[1]);
    EXPECT_EQ(6, map.players[1].colour);
    EXPECT_STREQ("Enemy", map.players[1].name);
    EXPECT_EQ(1u, map.players[1].stance[0]);
    EXPECT_EQ(18u, map.players[1].stance[1]);
    EXPECT_EQ(1u, map.players[1].stance[15]);
    map_free(&map);
}

UTEST(map, reads_a_unit)
{
    MakeMap m = make_map();
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    ASSERT_EQ(1u, map.unit_count);
    const MapUnit *u = &map.units[0];
    EXPECT_EQ(2 * 256 + 128, u->x);
    EXPECT_EQ(1 * 256 + 128, u->y);
    EXPECT_EQ(17u, u->type);
    EXPECT_EQ(3u, u->face);
    EXPECT_EQ(16u, u->flags);
    EXPECT_EQ(4u, u->quest_flags);
    EXPECT_EQ(1234, u->kind);
    EXPECT_EQ(2, u->player);
    EXPECT_EQ(1, u->sack);
    EXPECT_EQ(9, u->angle);
    EXPECT_EQ(-1, u->health);
    EXPECT_EQ(-1, u->health_max);
    EXPECT_EQ(301u, u->number);
    EXPECT_EQ(6, u->group);
    map_free(&map);
}

UTEST(map, reads_where_a_player_enters_from_the_script)
{
    MakeMap m = make_map();
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    /* In the script's order, and without the one outside the map. */
    ASSERT_EQ(2u, map.drop_count);
    EXPECT_EQ(2, map.drops[0].x);
    EXPECT_EQ(1, map.drops[0].y);
    EXPECT_EQ(0, map.drops[1].x);
    EXPECT_EQ(0, map.drops[1].y);
    map_free(&map);
}

UTEST(map, a_map_without_a_script_has_no_such_square)
{
    MakeMap m = make_map();
    m.leave_out = 7;
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    EXPECT_EQ(0u, map.drop_count);
    map_free(&map);
}

UTEST(map, a_map_with_nothing_on_it_may_leave_those_sections_out)
{
    MakeMap m = make_map();
    m.units = 0;
    m.leave_out = 6;
    Map map;
    ASSERT_TRUE(reads(&m, &map));
    EXPECT_EQ(0u, map.unit_count);
    map_free(&map);
}

/* True if the map is refused and `out` is left as it was. */
static bool refuses(MakeMap *m)
{
    Map map;
    memset(&map, 0x5a, sizeof map);
    Map before = map;
    bool ok = reads(m, &map);
    return !ok && memcmp(&map, &before, sizeof map) == 0;
}

UTEST(map, refuses_a_script_that_holds_fewer_actions_than_it_says)
{
    MakeMap m = make_map();
    m.more_actions = 1;
    EXPECT_TRUE(refuses(&m));
}

UTEST(map, refuses_what_is_not_a_map)
{
    MakeMap m = make_map();
    m.signature = 0x0052374eu;
    EXPECT_TRUE(refuses(&m));
}

UTEST(map, refuses_a_map_with_no_size_or_too_large)
{
    MakeMap none = make_map();
    none.width = 0;
    EXPECT_TRUE(refuses(&none));
    MakeMap flat = make_map();
    flat.height = 0;
    EXPECT_TRUE(refuses(&flat));
    MakeMap wide = make_map();
    wide.width = 257;
    EXPECT_TRUE(refuses(&wide));
    MakeMap tall = make_map();
    tall.height = 257;
    EXPECT_TRUE(refuses(&tall));
}

UTEST(map, reads_a_map_of_the_largest_size_and_the_most_players)
{
    MakeMap wide = make_map();
    wide.width = 256;
    wide.height = 1;
    Map map;
    ASSERT_TRUE(reads(&wide, &map));
    EXPECT_EQ(0x01ffu, map.ground[255]);
    map_free(&map);
    MakeMap tall = make_map();
    tall.width = 1;
    tall.height = 256;
    ASSERT_TRUE(reads(&tall, &map));
    EXPECT_EQ(0x01ffu, map.ground[255]);
    map_free(&map);
    MakeMap full = make_map();
    full.players = 16;
    ASSERT_TRUE(reads(&full, &map));
    EXPECT_EQ(16u, map.player_count);
    EXPECT_EQ(18u, map.players[15].stance[15]);
    map_free(&map);
}

UTEST(map, refuses_more_players_than_there_is_room_for)
{
    MakeMap m = make_map();
    m.players = 17;
    EXPECT_TRUE(refuses(&m));
}

UTEST(map, refuses_a_first_section_of_another_size)
{
    MakeMap m = make_map();
    m.info_stated = 640;
    EXPECT_TRUE(refuses(&m));
}

UTEST(map, refuses_a_map_that_lacks_its_ground_heights_or_objects)
{
    for (int id = 0; id <= 3; id++) {
        MakeMap m = make_map();
        m.leave_out = id;
        EXPECT_TRUE(refuses(&m));
    }
}

UTEST(map, refuses_a_section_given_twice)
{
    for (int id = 0; id <= 7; id++) {
        MakeMap m = make_map();
        m.twice = id;
        EXPECT_TRUE(refuses(&m));
    }
}

UTEST(map, refuses_a_section_before_the_one_with_the_size)
{
    MakeMap m = make_map();
    m.info_last = true;
    EXPECT_TRUE(refuses(&m));
}

UTEST(map, refuses_a_number_the_sections_do_not_bear_out)
{
    MakeMap width = make_map();
    width.more_width = 1;
    EXPECT_TRUE(refuses(&width));
    MakeMap players = make_map();
    players.more_players = 1;
    EXPECT_TRUE(refuses(&players));
    MakeMap structures = make_map();
    structures.more_structures = 1;
    EXPECT_TRUE(refuses(&structures));
    MakeMap fewer = make_map();
    fewer.more_structures = (uint32_t)-1;
    EXPECT_TRUE(refuses(&fewer));
    MakeMap units = make_map();
    units.more_units = 1;
    EXPECT_TRUE(refuses(&units));
    /* A count with no section at all. */
    for (int id = 4; id <= 6; id++) {
        MakeMap missing = make_map();
        missing.leave_out = id;
        EXPECT_TRUE(refuses(&missing));
    }
}

UTEST(map, refuses_a_section_a_byte_longer_than_it_should_be)
{
    for (int id = 1; id <= 6; id++) {
        MakeMap m = make_map();
        m.longer = id;
        EXPECT_TRUE(refuses(&m));
    }
}

UTEST(map, refuses_a_map_cut_short_anywhere)
{
    MakeMap m = make_map();
    finish(&m);
    for (size_t size = 0; size < m.file.size; size++) {
        Map map;
        EXPECT_FALSE(map_read(m.file.data, size, &map));
    }
    buffer_free(&m.file);
}
