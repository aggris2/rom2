#include "import/structures.h"

#include <stdio.h>
#include <string.h>

Things structures_things(const Reg *reg)
{
    Things t = {reg, "Count", "Structure"};
    return t;
}

bool structures_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size)
{
    Things t = structures_things(reg);
    char path[256];
    const char *text = NULL;
    if (!things_text(&t, id, "File", &text)) {
        return false;
    }
    int written = snprintf(path, sizeof path, "structures/%s.256", text);
    if (written < 0 || (size_t)written >= sizeof path
        || (size_t)written >= size) {
        return false;
    }
    /* The registry writes some folders with a capital. The archive has
     * every name in lower case. */
    for (char *p = path; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        } else if (*p >= 'A' && *p <= 'Z') {
            *p = (char)(*p - 'A' + 'a');
        }
    }
    memcpy(out, path, (size_t)written + 1);
    return true;
}

bool structures_layout(const Reg *reg, int32_t id, uint32_t tile_count,
    StructureLayout *out)
{
    Things t = structures_things(reg);
    StructureLayout layout;
    int32_t width = 0;
    int32_t height = 0;
    int32_t phases = 1;
    int32_t variable = 0;
    const char *mask = "";
    memset(&layout, 0, sizeof layout);
    if (!things_int(&t, id, "TileWidth", &width) || width <= 0
        || !things_int(&t, id, "FullHeight", &height) || height <= 0
        || (uint32_t)width > STRUCTURE_MAX_TILES / (uint32_t)height) {
        return false;
    }
    things_int(&t, id, "Phases", &phases);
    things_int(&t, id, "VariableSize", &variable);
    things_text(&t, id, "AnimMask", &mask);
    if (phases <= 0 || phases > 4096) {
        return false;
    }

    if (variable != 0) {
        if (tile_count == 0) {
            return false;
        }
        layout.width = 1;
        layout.height = 1;
        layout.phases = 1;
        layout.is_pieces = true;
        layout.pieces = tile_count;
        *out = layout;
        return true;
    }

    uint32_t tiles = (uint32_t)width * (uint32_t)height;
    layout.width = (uint32_t)width;
    layout.height = (uint32_t)height;

    /* The mask has a mark for each tile: '+' for one that changes. A
     * structure without a mask changes all its tiles. */
    size_t mask_length = strlen(mask);
    if (mask_length != 0 && mask_length != tiles) {
        return false;
    }
    for (uint32_t i = 0; i < tiles; i++) {
        layout.animated[i] = mask_length == 0 || mask[i] == '+';
        layout.animated_count += layout.animated[i] ? 1u : 0u;
    }

    uint32_t moving = layout.animated_count * ((uint32_t)phases - 1);
    if (tile_count >= tiles && tile_count - tiles == moving) {
        layout.phases = (uint32_t)phases;
    } else if (tile_count >= 2 * tiles
        && tile_count - 2 * tiles == moving) {
        layout.phases = (uint32_t)phases;
        layout.has_ruin = true;
    } else if (tile_count == tiles) {
        layout.phases = 1;
    } else {
        return false;
    }
    if (layout.phases == 1) {
        memset(layout.animated, 0, sizeof layout.animated);
        layout.animated_count = 0;
    }
    *out = layout;
    return true;
}

bool structures_tile(const StructureLayout *layout, uint32_t phase,
    uint32_t index, uint32_t *out_tile)
{
    uint32_t tiles = layout->width * layout->height;
    if (layout->is_pieces || phase >= layout->phases || index >= tiles) {
        return false;
    }
    if (phase == 0 || !layout->animated[index]) {
        *out_tile = index;
        return true;
    }
    /* The changing tiles of each later picture follow the first picture,
     * in the order they have in it. */
    uint32_t before = 0;
    for (uint32_t i = 0; i < index; i++) {
        before += layout->animated[i] ? 1u : 0u;
    }
    *out_tile = tiles + layout->animated_count * (phase - 1) + before;
    return true;
}

bool structures_ruin_tile(const StructureLayout *layout, uint32_t index,
    uint32_t *out_tile)
{
    uint32_t tiles = layout->width * layout->height;
    if (!layout->has_ruin || index >= tiles) {
        return false;
    }
    *out_tile = tiles + layout->animated_count * (layout->phases - 1)
        + index;
    return true;
}
