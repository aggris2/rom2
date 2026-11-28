#include "data/unit_animation.h"

#include <string.h>

#define MOST 4096

bool unit_animation_read(const TomlTable *top, const char *name,
                         UnitAnimation *out)
{
    UnitAnimation animation;
    memset(&animation, 0, sizeof animation);
    const TomlTable *all = toml_get_table(top, "anim");
    const TomlTable *table = all == NULL ? NULL : toml_get_table(all, name);
    if (table == NULL) {
        *out = animation;
        return true;
    }
    int32_t column = 0;
    int32_t columns = 0;
    if (!toml_get_int(table, "column", &column) || column < 0
        || column > MOST || !toml_get_int(table, "columns", &columns)
        || columns < 1 || columns > MOST
        || !sprite_round_read(table, columns, &animation.round)
        || animation.round.length == 0) {
        return false;
    }
    animation.column = (uint16_t)column;
    *out = animation;
    return true;
}
