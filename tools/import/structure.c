#include "import/structure.h"

#include <stdio.h>
#include <string.h>

#include "base/toml.h"
#include "import/frames.h"
#include "import/things.h"

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

/* Puts tile `tile` of the file at tile `index` of the picture in cell
 * `column`. */
static bool put_tile(const StructureLayout *layout, const Frames *frames,
    uint32_t tile, uint32_t index, Sheet *sheet, uint32_t column)
{
    if (tile >= frames->count) {
        return false;
    }
    const Frame256 *frame = &frames->items[tile];
    if (frame->width != STRUCTURE_TILE || frame->height != STRUCTURE_TILE) {
        return false;
    }
    return sheet_put(sheet, column, 0, frame->pixels, frame->width,
        frame->height, (index % layout->width) * STRUCTURE_TILE,
        (index / layout->width) * STRUCTURE_TILE, false);
}

/* Builds the sheet of the animation, or of the pieces, and that of the
 * ruin, from one set of frames. */
static bool build(const StructureLayout *layout, const Frames *frames,
    Sheet *sheet, Sheet *ruin)
{
    uint32_t tiles = layout->width * layout->height;
    uint32_t columns = layout->is_pieces ? layout->pieces : layout->phases;
    if (!sheet_create(sheet, layout->width * STRUCTURE_TILE,
            layout->height * STRUCTURE_TILE, columns, 1)) {
        return false;
    }
    for (uint32_t column = 0; column < columns; column++) {
        for (uint32_t index = 0; index < tiles; index++) {
            uint32_t tile = column;
            if (!layout->is_pieces
                && !structures_tile(layout, column, index, &tile)) {
                return false;
            }
            if (!put_tile(layout, frames, tile, index, sheet, column)) {
                return false;
            }
        }
    }
    if (!layout->has_ruin) {
        return true;
    }
    if (!sheet_create(ruin, layout->width * STRUCTURE_TILE,
            layout->height * STRUCTURE_TILE, 1, 1)) {
        return false;
    }
    for (uint32_t index = 0; index < tiles; index++) {
        uint32_t tile = 0;
        if (!structures_ruin_tile(layout, index, &tile)
            || !put_tile(layout, frames, tile, index, ruin, 0)) {
            return false;
        }
    }
    return true;
}

bool structure_sheet_build(const StructureLayout *layout,
    const Sprite256 *sprite, const Sprite256 *edges, StructureSheet *out)
{
    if (layout->width == 0 || layout->height == 0
        || (edges != NULL && edges->frame_count != sprite->frame_count)) {
        return false;
    }
    Frames frames;
    Frames edge_frames = {NULL, 0};
    if (!frames_read(sprite, &frames)) {
        return false;
    }
    if (edges != NULL && !frames_read(edges, &edge_frames)) {
        frames_free(&frames);
        return false;
    }

    StructureSheet result;
    memset(&result, 0, sizeof result);
    bool ok = build(layout, &frames, &result.sheet, &result.ruin)
        && (edges == NULL || build(layout, &edge_frames, &result.edges,
            &result.ruin_edges));
    frames_free(&frames);
    frames_free(&edge_frames);
    if (!ok) {
        structure_sheet_free(&result);
        return false;
    }
    *out = result;
    return true;
}

void structure_sheet_free(StructureSheet *sheet)
{
    sheet_free(&sheet->sheet);
    sheet_free(&sheet->edges);
    sheet_free(&sheet->ruin);
    sheet_free(&sheet->ruin_edges);
    memset(sheet, 0, sizeof *sheet);
}

bool structure_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE])
{
    Things t = structures_things(reg);
    return thing_toml_folder_name(&t, id, "DescText", out);
}

/* Adds the box to click on, if the structure states it. */
static bool put_selection(ByteBuffer *b, const Things *t, int32_t id)
{
    int32_t box[4];
    if (!things_int(t, id, "SelectionX1", &box[0])
        || !things_int(t, id, "SelectionY1", &box[1])
        || !things_int(t, id, "SelectionX2", &box[2])
        || !things_int(t, id, "SelectionY2", &box[3])) {
        return true;
    }
    return toml_int_list(b, "selection", box, 4);
}

/* Adds one of data.bin's numbers for the structure, if the buildings
 * have a column of that name and a row for this ID. */
static bool put_databin(ByteBuffer *b, const DataBin *bin,
    const DataBinRow *row, const char *column, const char *name)
{
    uint32_t index = 0;
    return !databin_column(bin, DATABIN_BUILDINGS, column, &index)
        || index >= row->number_count
        || toml_int(b, name, row->numbers[index]);
}

/* Adds data.bin's numbers for the structure with the given ID, which is
 * row `id` of the buildings, counted from 1. */
static bool put_numbers(ByteBuffer *b, const DataBin *bin, int32_t id)
{
    /* static: a row is too large for the stack to be at ease. */
    static DataBinRow row;
    if (bin == NULL || id < 1) {
        return true;
    }
    DataBinRows rows = databin_rows(bin, DATABIN_BUILDINGS);
    for (int32_t i = 0; i < id; i++) {
        if (!databin_next(&rows, &row)) {
            return true;
        }
    }
    return put_databin(b, bin, &row, "scanRange", "scan_range")
        && put_databin(b, bin, &row, "healthMax", "health")
        && put_databin(b, bin, &row, "Passability", "passability")
        && put_databin(b, bin, &row, "BuildingPresent", "building_present");
}

bool structure_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const StructureLayout *layout, const StructureSheet *sheet,
    const DataBin *bin)
{
    Things t = structures_things(reg);
    char section[THINGS_SECTION_SIZE];
    int32_t frames[STRUCTURE_TOML_MAX_STEPS];
    int32_t ticks[STRUCTURE_TOML_MAX_STEPS];
    size_t frame_count = 0;
    size_t tick_count = 0;
    const char *name = "";
    if (!things_section(&t, id, section)
        || !thing_toml_list(&t, id, "AnimFrame", frames,
            STRUCTURE_TOML_MAX_STEPS, &frame_count)
        || !thing_toml_list(&t, id, "AnimTime", ticks,
            STRUCTURE_TOML_MAX_STEPS, &tick_count)
        || frame_count != tick_count) {
        return false;
    }
    /* A structure that turned out to stand still, or is pieces, has no
     * use for the lists. */
    if (layout->is_pieces || layout->phases == 1) {
        frame_count = 0;
    }
    for (size_t i = 0; i < frame_count; i++) {
        if (frames[i] < 0 || (uint32_t)frames[i] >= sheet->sheet.columns) {
            return false;
        }
    }
    things_text(&t, id, "DescText", &name);

    int32_t tile_width = 0;
    int32_t tile_height = 0;
    things_int(&t, id, "TileWidth", &tile_width);
    things_int(&t, id, "TileHeight", &tile_height);
    const int32_t size[2] = {tile_width, tile_height};
    const int32_t cell[2] = {
        (int32_t)sheet->sheet.cell_width, (int32_t)sheet->sheet.cell_height
    };

    bool ok = toml_comment(b, name)
        && toml_int(b, "id", id)
        && toml_text(b, "name", name)
        && thing_toml_text(b, &t, id, "Picture", "picture", false)
        && thing_toml_int(b, &t, id, "IconID", "icon_id")
        && toml_int_list(b, "size", size, 2)
        && thing_toml_int(b, &t, id, "FullHeight", "height")
        && put_selection(b, &t, id)
        && thing_toml_int(b, &t, id, "ShadowY", "shadow_y")
        && thing_toml_int(b, &t, id, "Indestructible", "indestructible")
        && thing_toml_int(b, &t, id, "Usable", "usable")
        && thing_toml_int(b, &t, id, "Flat", "flat")
        && thing_toml_int(b, &t, id, "LightRadius", "light_radius")
        && thing_toml_int(b, &t, id, "LightPulse", "light_pulse")
        && thing_toml_int(b, &t, id, "VariableSize", "variable_size")
        && put_numbers(b, bin, id);

    ok = ok && toml_table(b, "sprite")
        && toml_text(b, "sheet", "sprites.png")
        && (sheet->edges.pixels == NULL
            || toml_text(b, "edges", "edges.png"))
        && toml_int_list(b, "cell", cell, 2)
        && toml_int(b, "columns", (int32_t)sheet->sheet.columns)
        && (frame_count == 0
            || (toml_int_list(b, "frames", frames, frame_count)
                && toml_int_list(b, "ticks", ticks, tick_count)))
        && (sheet->ruin.pixels == NULL
            || toml_text(b, "ruin", "ruin.png"))
        && (sheet->ruin_edges.pixels == NULL
            || toml_text(b, "ruin_edges", "ruin_edges.png"));
    return ok;
}
