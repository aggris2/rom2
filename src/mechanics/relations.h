/* How the players of a game stand to each other: who is whose enemy.
 *
 * It is one way: a player may be at war with another who is not at
 * war with it. A map says how its players stand when a game starts,
 * and the game may change it as it is played.
 */
#ifndef ROM2_MECHANICS_RELATIONS_H
#define ROM2_MECHANICS_RELATIONS_H

#include <stdbool.h>
#include <stdint.h>

#include "data/map.h"

/* The players there can be: those a map can have. They are counted
 * from 1, as a map counts them. */
#define RELATIONS_PLAYERS MAP_MAX_PLAYERS

/* What a player can be to another, as bits: its enemy, its ally. One
 * that is neither is neutral. The original has more bits, for who
 * sees what; they are kept as the map has them and not read yet. */
#define RELATION_ENEMY 1u
#define RELATION_ALLY 2u

/* What every player is to itself, as the original has it: an ally who
 * shares what it sees. */
#define RELATION_OWN 18u

/* `stands[a - 1][b - 1]` is what player `b` is to player `a`. */
typedef struct Relations {
    uint8_t stands[RELATIONS_PLAYERS][RELATIONS_PLAYERS];
} Relations;

/* Fills the table as a map has it: each of its players stands to the
 * others as its record says, to itself as RELATION_OWN whatever the
 * record says, and a player the map does not have is neutral to all
 * and all to it. */
void relations_from_map(const Map *map, Relations *out);

/* Whether `other` is an enemy to `player`: whether `player` is at war
 * with it. False for a player there cannot be. */
bool relations_enemy(const Relations *relations, int32_t player,
                     int32_t other);

#endif
