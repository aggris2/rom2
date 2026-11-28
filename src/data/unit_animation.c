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
    int32_t begin = 0;
    if (!toml_get_int(table, "column", &column) || column < 0
        || column > MOST || !toml_get_int(table, "columns", &columns)
        || columns < 1 || columns > MOST
        || (toml_has(table, "begin")
            && (!toml_get_int(table, "begin", &begin) || begin < 0
                || begin >= columns))
        || !sprite_round_read(table, columns - begin, &animation.round)
        || animation.round.length == 0) {
        return false;
    }
    animation.column = (uint16_t)column;
    animation.begin = (uint16_t)begin;
    *out = animation;
    return true;
}
