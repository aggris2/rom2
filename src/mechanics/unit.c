#include "mechanics/unit.h"

void unit_turn(Unit *unit)
{
    /* The steps to go one way round and the other: a byte counts
     * round by itself. */
    uint8_t forward = (uint8_t)(unit->facing_wanted - unit->facing);
    uint8_t backward = (uint8_t)(unit->facing - unit->facing_wanted);
    uint8_t shorter = forward < backward ? forward : backward;
    if ((!unit->turning && shorter < UNIT_TURN_AT_ONCE)
        || shorter < unit->turn_speed) {
        unit->facing = unit->facing_wanted;
    } else if (forward <= backward) {
        unit->facing = (uint8_t)(unit->facing + unit->turn_speed);
    } else {
        unit->facing = (uint8_t)(unit->facing - unit->turn_speed);
    }
    unit->turning = unit->facing != unit->facing_wanted;
}
