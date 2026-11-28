#include "mechanics/relations.h"

#include <string.h>

#include "utest.h"

/* A map of three players: the first is at war with the second, who
 * is not at war with it, and neutral to the third. */
static void make_map(Map *map, MapPlayer *players)
{
    memset(map, 0, sizeof *map);
    memset(players, 0, 3 * sizeof *players);
    map->players = players;
    map->player_count = 3;
    players[0].stance[1] = RELATION_ENEMY;
    players[2].stance[1] = RELATION_ALLY;
}

UTEST(relations, a_player_is_at_war_with_whom_its_record_says)
{
    Map map;
    MapPlayer players[3];
    Relations relations;
    make_map(&map, players);
    relations_from_map(&map, &relations);
    EXPECT_TRUE(relations_enemy(&relations, 1, 2));
    EXPECT_FALSE(relations_enemy(&relations, 1, 3));
    EXPECT_FALSE(relations_enemy(&relations, 3, 2));
}

UTEST(relations, war_is_one_way)
{
    Map map;
    MapPlayer players[3];
    Relations relations;
    make_map(&map, players);
    relations_from_map(&map, &relations);
    EXPECT_FALSE(relations_enemy(&relations, 2, 1));
}

UTEST(relations, a_player_is_its_own_ally_whatever_its_record_says)
{
    Map map;
    MapPlayer players[3];
    Relations relations;
    make_map(&map, players);
    players[1].stance[1] = RELATION_ENEMY;
    relations_from_map(&map, &relations);
    EXPECT_FALSE(relations_enemy(&relations, 2, 2));
    EXPECT_EQ((unsigned)RELATION_OWN, relations.stands[1][1]);
}

UTEST(relations, the_bits_that_are_not_read_are_kept)
{
    Map map;
    MapPlayer players[3];
    Relations relations;
    make_map(&map, players);
    players[0].stance[2] = 0x11C;
    relations_from_map(&map, &relations);
    EXPECT_EQ(0x1Cu, relations.stands[0][2]);
    EXPECT_FALSE(relations_enemy(&relations, 1, 3));
}

UTEST(relations, a_player_the_map_has_not_is_nobodys_enemy)
{
    Map map;
    MapPlayer players[3];
    Relations relations;
    make_map(&map, players);
    relations_from_map(&map, &relations);
    EXPECT_FALSE(relations_enemy(&relations, 1, 4));
    EXPECT_FALSE(relations_enemy(&relations, 4, 1));
    EXPECT_FALSE(relations_enemy(&relations, 0, 2));
    EXPECT_FALSE(relations_enemy(&relations, 1, 0));
    EXPECT_FALSE(relations_enemy(&relations, 1, -3));
    EXPECT_FALSE(relations_enemy(&relations, 17, 2));
    EXPECT_FALSE(relations_enemy(&relations, 1, 17));
}
