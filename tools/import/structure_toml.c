#include "import/structure_toml.h"

#include "import/toml.h"

bool structure_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE])
{
    Things t = structures_things(reg);
    char section[THINGS_SECTION_SIZE];
    const char *text = "";
    if (!things_section(&t, id, section)) {
        return false;
    }
    things_text(&t, id, "DescText", &text);
    return name_folder(id, text, out);
}

/* Adds a whole number of the structure under another name, if it is
 * stated. */
static bool copy_int(ByteBuffer *b, const Things *t, int32_t id,
    const char *key, const char *name)
{
    int32_t value = 0;
    return !things_int(t, id, key, &value) || toml_int(b, name, value);
}

/* Adds a text of the structure under another name, if it is stated and
 * not empty. */
static bool copy_text(ByteBuffer *b, const Things *t, int32_t id,
    const char *key, const char *name)
{
    const char *text = NULL;
    return !things_text(t, id, key, &text) || text[0] == '\0'
        || toml_text(b, name, text);
}

/* Reads a list of the structure. The registry has an empty text in
 * place of a list that is not there, and that, like no entry at all,
 * gives `*count` 0. Returns false for a list that cannot be read. */
static bool read_list(const Things *t, int32_t id, const char *key,
    int32_t out[STRUCTURE_TOML_MAX_STEPS], size_t *count)
{
    const char *text = NULL;
    *count = 0;
    return things_int_list(t, id, key, out, STRUCTURE_TOML_MAX_STEPS, count)
        || !things_has(t, id, key)
        || things_text(t, id, key, &text);
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
        || !read_list(&t, id, "AnimFrame", frames, &frame_count)
        || !read_list(&t, id, "AnimTime", ticks, &tick_count)
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
        && copy_text(b, &t, id, "Picture", "picture")
        && copy_int(b, &t, id, "IconID", "icon_id")
        && toml_int_list(b, "size", size, 2)
        && copy_int(b, &t, id, "FullHeight", "height")
        && put_selection(b, &t, id)
        && copy_int(b, &t, id, "ShadowY", "shadow_y")
        && copy_int(b, &t, id, "Indestructible", "indestructible")
        && copy_int(b, &t, id, "Usable", "usable")
        && copy_int(b, &t, id, "Flat", "flat")
        && copy_int(b, &t, id, "LightRadius", "light_radius")
        && copy_int(b, &t, id, "LightPulse", "light_pulse")
        && copy_int(b, &t, id, "VariableSize", "variable_size")
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
