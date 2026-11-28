#include "data/unit_kind.h"

bool unit_kind_read(const TomlTable *kind, UnitKind *out)
{
    int32_t id = 0;
    int32_t face = 0;
    int32_t rotation_speed = 0;
    int32_t health = 0;
    int32_t mana = 0;
    int32_t size = -1;
    int32_t mover = -1;
    if (!toml_get_int(kind, "id", &id) || id < 0 || id >= UNIT_KIND_IDS
        || !toml_get_int(kind, "face", &face) || face < 0
        || face > UINT16_MAX
        || !toml_get_int(kind, "rotation_speed", &rotation_speed)
        || rotation_speed < 1 || rotation_speed > UINT8_MAX
        || !toml_get_int(kind, "health", &health) || health < -1
        || !toml_get_int(kind, "mana", &mana) || mana < -1
        || (toml_has(kind, "token_size")
            && (!toml_get_int(kind, "token_size", &size) || size == 0
                || size < -1 || size > UNIT_KIND_MAX_SIZE))
        || (toml_has(kind, "movement_type")
            && (!toml_get_int(kind, "movement_type", &mover) || mover == 0
                || mover < -1 || mover > UNIT_FLIES))) {
        return false;
    }
    out->id = (uint16_t)id;
    out->face = (uint16_t)face;
    out->rotation_speed = (uint8_t)rotation_speed;
    out->health = health;
    out->mana = mana;
    out->size = (uint8_t)(size < 0 ? 1 : size);
    out->mover = (uint8_t)(mover < 0 ? UNIT_WALKS : mover);
    return true;
}
