#include "mechanics/idle.h"

void idle_tick(Unit *unit, Random *random)
{
    if (unit->facing == unit->facing_wanted) {
        unit->turning = false;
        if (random_below(random, IDLE_CHANCES) >= IDLE_LOOKS) {
            return;
        }
        unit->facing_wanted = (uint8_t)(unit->facing + IDLE_TURN_LEAST
            + random_below(random, IDLE_TURN_SPREAD));
    }
    unit_turn(unit);
}
