#include "data/terrain_costs.h"

#include <stddef.h>

bool terrain_costs_read(const TomlTable *top, TerrainCosts *out)
{
    /* In the order of TerrainKind, from TERRAIN_LAND. */
    static const char *const names[TERRAIN_KINDS - 1] = {
        "CostLand", "CostGrass", "CostFlowers", "CostSand", "CostCracked",
        "CostStones", "CostSavanna", "CostMountain", "CostWater",
        "CostRoad"
    };
    const TomlTable *terrain = toml_get_table(top, "Terrain");
    TerrainCosts costs;
    if (terrain == NULL) {
        return false;
    }
    costs.of[TERRAIN_NONE] = TERRAIN_COST_NONE;
    for (int kind = TERRAIN_LAND; kind < TERRAIN_KINDS; kind++) {
        int32_t cost = 0;
        if (!toml_get_int(terrain, names[kind - 1], &cost) || cost < 1
            || cost > TERRAIN_COST_MOST) {
            return false;
        }
        costs.of[kind] = (uint8_t)cost;
    }
    *out = costs;
    return true;
}
