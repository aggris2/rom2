#include "mechanics/relations.h"

#include <string.h>

void relations_from_map(const Map *map, Relations *out)
{
    memset(out, 0, sizeof *out);
    for (uint32_t a = 0; a < map->player_count && a < RELATIONS_PLAYERS;
        a++) {
        for (uint32_t b = 0; b < map->player_count
            && b < RELATIONS_PLAYERS; b++) {
            /* The original keeps the low eight bits. */
            out->stands[a][b] = (uint8_t)(map->players[a].stance[b] & 0xFFu);
        }
        out->stands[a][a] = RELATION_OWN;
    }
}

bool relations_enemy(const Relations *relations, int32_t player,
                     int32_t other)
{
    return player >= 1 && other >= 1
        && (uint32_t)player <= RELATIONS_PLAYERS
        && (uint32_t)other <= RELATIONS_PLAYERS
        && (relations->stands[player - 1][other - 1] & RELATION_ENEMY) != 0;
}
