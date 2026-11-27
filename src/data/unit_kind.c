#include "data/unit_kind.h"

bool unit_kind_read(const TomlTable *kind, UnitKind *out)
{
    int32_t id = 0;
    int32_t face = 0;
    if (!toml_get_int(kind, "id", &id) || id < 0 || id >= UNIT_KIND_IDS
        || !toml_get_int(kind, "face", &face) || face < 0
        || face > UINT16_MAX) {
        return false;
    }
    out->id = (uint16_t)id;
    out->face = (uint16_t)face;
    return true;
}
