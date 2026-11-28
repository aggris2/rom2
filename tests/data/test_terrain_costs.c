#include "data/terrain_costs.h"

#include <string.h>

#include "utest.h"

#define COSTS "[Terrain]\nCostLand = 8\nPassLand = 0\nCostGrass = 9\n" \
    "CostFlowers = 10\nCostSand = 14\nCostCracked = 6\nCostStones = 12\n" \
    "CostSavanna = 11\nCostMountain = 16\nCostWater = 7\n"

/* Reads the rules written as text. */
static bool reads(const char *text, TerrainCosts *out)
{
    TomlTable *top = NULL;
    if (!toml_read((const uint8_t *)text, strlen(text), &top, NULL)) {
        return false;
    }
    bool ok = terrain_costs_read(top, out);
    toml_free(top);
    return ok;
}

/* True if the rules are refused and `out` is left as it was. */
static bool refuses(const char *text)
{
    TerrainCosts costs;
    memset(&costs, 0x5a, sizeof costs);
    TerrainCosts before = costs;
    return !reads(text, &costs)
        && memcmp(&costs, &before, sizeof costs) == 0;
}

UTEST(terrain_costs, reads_the_cost_of_every_kind)
{
    TerrainCosts costs;
    ASSERT_TRUE(reads(COSTS "CostRoad = 5\n", &costs));
    EXPECT_EQ(8u, costs.of[TERRAIN_LAND]);
    EXPECT_EQ(9u, costs.of[TERRAIN_GRASS]);
    EXPECT_EQ(10u, costs.of[TERRAIN_FLOWERS]);
    EXPECT_EQ(14u, costs.of[TERRAIN_SAND]);
    EXPECT_EQ(6u, costs.of[TERRAIN_CRACKED]);
    EXPECT_EQ(12u, costs.of[TERRAIN_STONES]);
    EXPECT_EQ(11u, costs.of[TERRAIN_SAVANNA]);
    EXPECT_EQ(16u, costs.of[TERRAIN_MOUNTAIN]);
    EXPECT_EQ(7u, costs.of[TERRAIN_WATER]);
    EXPECT_EQ(5u, costs.of[TERRAIN_ROAD]);
    /* What is no ground cannot be crossed at any cost. */
    EXPECT_EQ((unsigned)TERRAIN_COST_NONE, costs.of[TERRAIN_NONE]);
}

UTEST(terrain_costs, refuses_rules_that_lack_a_cost_or_the_table)
{
    EXPECT_TRUE(refuses(COSTS));
    EXPECT_TRUE(refuses("CostLand = 8\n"));
    EXPECT_TRUE(refuses("[Other]\nCostLand = 8\n"));
}

UTEST(terrain_costs, refuses_a_cost_that_cannot_be)
{
    EXPECT_TRUE(refuses(COSTS "CostRoad = 0\n"));
    EXPECT_TRUE(refuses(COSTS "CostRoad = -3\n"));
    EXPECT_TRUE(refuses(COSTS "CostRoad = 255\n"));
    EXPECT_TRUE(refuses(COSTS "CostRoad = \"low\"\n"));
    TerrainCosts costs;
    EXPECT_TRUE(reads(COSTS "CostRoad = 254\n", &costs));
    EXPECT_TRUE(reads(COSTS "CostRoad = 1\n", &costs));
}
