#include "data/structure_kind.h"

#include <stddef.h>

static bool is_side(int32_t value)
{
    return value >= 1 && value <= STRUCTURE_MAX_SIDE;
}

bool structure_kind_read(const TomlTable *top, StructureKind *out)
{
    const int32_t *size = NULL;
    size_t count = 0;
    int32_t height = 0;
    StructureKind kind;
    if (!toml_get_int_list(top, "size", &size, &count) || count != 2
        || !is_side(size[0]) || !is_side(size[1])
        || !toml_get_int(top, "height", &height) || !is_side(height)
        || height < size[1] || !sprite_sheet_read(top, &kind.sheet)) {
        return false;
    }
    kind.width = (uint16_t)size[0];
    kind.depth = (uint16_t)size[1];
    kind.height = (uint16_t)height;
    *out = kind;
    return true;
}
