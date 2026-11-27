#include "import/item.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/toml.h"
#include "import/name.h"
#include "import/text.h"

uint32_t items_variants(const DataBinRow *row,
    ItemVariant out[ITEM_MAX_VARIANTS])
{
    if (row->number_count < ITEM_CLASSES) {
        return 0;
    }
    /* The masks are the last numbers of the row. */
    const int32_t *masks = row->numbers + row->number_count - ITEM_CLASSES;
    uint32_t count = 0;
    for (uint32_t c = 0; c < ITEM_CLASSES; c++) {
        for (uint32_t m = 0; m < ITEM_MATERIALS; m++) {
            if (((uint32_t)masks[c] >> m) & 1u) {
                out[count].material = (uint8_t)m;
                out[count].item_class = (uint8_t)c;
                count++;
            }
        }
    }
    return count;
}

bool items_slot(const DataBin *bin, DataBinTable table,
    const DataBinRow *row, int32_t *out)
{
    uint32_t index = 0;
    if (!databin_column(bin, table, "Slot", &index)
        || index >= row->number_count) {
        return false;
    }
    *out = row->numbers[index];
    return true;
}

bool items_file_name(int32_t material, int32_t slot, int32_t item_class,
    int32_t place, char out[ITEM_FILE_NAME_SIZE])
{
    if (material < 0 || material > 99 || slot < 0 || slot > 99
        || item_class < 0 || item_class > 9 || place < 0 || place > 99) {
        return false;
    }
    snprintf(out, ITEM_FILE_NAME_SIZE, "%02d%02d%d%02d", (int)material,
        (int)slot, (int)item_class, (int)place);
    return true;
}

/* Finds line number `index` of a text, counted from 0. A line ends at a
 * line feed, and a carriage return before that is not part of it. The
 * last line may end with the text. Returns false if there is no such
 * line. */
static bool line_at(const uint8_t *text, size_t size, uint32_t index,
    const uint8_t **out, size_t *out_length)
{
    size_t start = 0;
    for (uint32_t line = 0; start <= size; line++) {
        size_t end = start;
        while (end < size && text[end] != '\n') {
            end++;
        }
        if (line == index) {
            /* Past the last line feed there is a line only if there is
             * something in it. */
            if (end == size && start == size) {
                return false;
            }
            size_t length = end - start;
            if (length > 0 && text[start + length - 1] == '\r') {
                length--;
            }
            *out = text + start;
            *out_length = length;
            return true;
        }
        start = end + 1;
    }
    return false;
}

/* Counts the lines of a text, as line_at() sees them. */
static uint32_t line_count(const uint8_t *text, size_t size)
{
    uint32_t count = 0;
    for (size_t i = 0; i < size; i++) {
        if (text[i] == '\n') {
            count++;
        }
    }
    return size > 0 && text[size - 1] != '\n' ? count + 1 : count;
}

bool item_names_open(const uint8_t *numbers, size_t numbers_size,
    const uint8_t *server, size_t server_size, const uint8_t *shown,
    size_t shown_size, ItemNames *out)
{
    if (numbers_size % 2 != 0 || numbers_size / 2 > 0xffffu) {
        return false;
    }
    uint32_t count = (uint32_t)(numbers_size / 2);
    if (line_count(server, server_size) < count
        || line_count(shown, shown_size) < count) {
        return false;
    }
    out->numbers = numbers;
    out->count = count;
    out->server = server;
    out->server_size = server_size;
    out->shown = shown;
    out->shown_size = shown_size;
    return true;
}

/* Writes the name shown of item number `index` in the files to `out`. */
static bool shown_at(const ItemNames *names, uint32_t index,
    char out[ITEM_NAME_SIZE])
{
    const uint8_t *line = NULL;
    size_t length = 0;
    ByteBuffer utf8 = {0};
    bool ok = line_at(names->shown, names->shown_size, index, &line,
            &length)
        && text_cp1251_to_utf8(line, length, &utf8)
        && utf8.size < ITEM_NAME_SIZE;
    if (ok) {
        if (utf8.size > 0) {
            memcpy(out, utf8.data, utf8.size);
        }
        out[utf8.size] = '\0';
    }
    buffer_free(&utf8);
    return ok;
}

bool item_names_shown(const ItemNames *names, int32_t material,
    int32_t slot, int32_t item_class, int32_t place,
    char out[ITEM_NAME_SIZE])
{
    if (material < 0 || material > 15 || slot < 0 || slot > 15
        || item_class < 0 || item_class > 7 || place < 0 || place > 31) {
        return false;
    }
    uint32_t wanted = ((uint32_t)material << 12) | ((uint32_t)slot << 8)
        | ((uint32_t)item_class << 5) | (uint32_t)place;
    for (uint32_t i = 0; i < names->count; i++) {
        uint32_t number = names->numbers[i * 2]
            | ((uint32_t)names->numbers[i * 2 + 1] << 8);
        if (number == wanted) {
            return shown_at(names, i, out);
        }
    }
    return false;
}

bool item_names_shown_for(const ItemNames *names, const char *server_name,
    char out[ITEM_NAME_SIZE])
{
    size_t wanted = strlen(server_name);
    for (uint32_t i = 0; i < names->count; i++) {
        const uint8_t *line = NULL;
        size_t length = 0;
        if (line_at(names->server, names->server_size, i, &line, &length)
            && length == wanted && memcmp(line, server_name, wanted) == 0) {
            return shown_at(names, i, out);
        }
    }
    return false;
}

/* A column of data.bin and the name ROM2 gives it. */
typedef struct Column {
    const char *column;
    const char *key;
} Column;

/* The columns of weapons, shields and armour. The file's own names have
 * marks in front, and one is misspelt. */
static const Column SHAPE_COLUMNS[] = {
    {"Shape", "shape"},
    {"Material", "material"},
    {"Price", "price"},
    {"weight", "weight"},
    {"Slot", "slot"},
    {"@.attackType", "attack_type"},
    {"@.physicalMin", "damage_min"},
    {"@.physicalMax", "damage_max"},
    {"@.toHit", "to_hit"},
    {"#.deIrnce", "defence"},
    {"#.absorbtion", "absorption"},
    {"@.range", "range"},
    {"@.charge", "charge"},
    {"@.relax", "relax"},
    {"2 handed", "two_handed"},
    {"sutableFor", "suitable_for"},
    {"Other Parameter", "other"},
};

/* Reads the name of row `number`, counted from 0, of a table. */
static bool row_name(const DataBin *bin, DataBinTable table,
    uint32_t number, char out[DATABIN_TEXT_SIZE])
{
    /* static: a row is too large for the stack to be at ease. */
    static DataBinRow row;
    DataBinRows rows = databin_rows(bin, table);
    for (uint32_t i = 0; i <= number; i++) {
        if (!databin_next(&rows, &row)) {
            return false;
        }
    }
    strcpy(out, row.name);
    return true;
}

bool item_toml_variant_name(const DataBin *bin, uint32_t material,
    uint32_t item_class, char out[ITEM_TOML_VARIANT_NAME_SIZE])
{
    char name[DATABIN_TEXT_SIZE];
    char label[ITEM_TOML_VARIANT_NAME_SIZE];
    /* The classes are the table the file calls its shapes. */
    if (!row_name(bin, DATABIN_MATERIALS, material, name)) {
        return false;
    }
    name_label(name, label, sizeof label / 2);
    size_t length = strlen(label);
    if (!row_name(bin, DATABIN_SHAPES, item_class, name)) {
        return false;
    }
    label[length++] = '_';
    name_label(name, label + length, sizeof label - length);
    strcpy(out, label);
    return true;
}

/* Adds the files and the size of a picture for the inventory. */
static bool put_icon(ByteBuffer *b, uint32_t width, uint32_t height)
{
    const int32_t size[2] = {(int32_t)width, (int32_t)height};
    return toml_table(b, "icon")
        && toml_text(b, "sheet", "icon.png")
        && toml_text(b, "alpha", "icon_alpha.png")
        && toml_int_list(b, "size", size, 2);
}

/* Adds which characters a shape is drawn on. */
static bool put_worn(ByteBuffer *b, const ItemTomlWorn *worn)
{
    const int32_t size[2] = {(int32_t)worn->width, (int32_t)worn->height};
    return toml_table(b, "worn")
        && toml_text_list(b, "characters", worn->characters,
            worn->character_count)
        && (!worn->has_second || toml_bool(b, "second", true))
        && toml_int_list(b, "size", size, 2);
}

bool item_toml_write_shape(ByteBuffer *b, const DataBin *bin,
    DataBinTable table, const DataBinRow *row, int32_t place,
    uint32_t icon_width, uint32_t icon_height, const ItemTomlWorn *worn)
{
    return item_toml_write_shape_named(b, bin, table, row, place,
        icon_width, icon_height, worn, NULL);
}

bool item_toml_write_shape_named(ByteBuffer *b, const DataBin *bin,
    DataBinTable table, const DataBinRow *row, int32_t place,
    uint32_t icon_width, uint32_t icon_height, const ItemTomlWorn *worn,
    const ItemNames *names)
{
    const char *kind;
    if (table == DATABIN_WEAPONS) {
        kind = "weapon";
    } else if (table == DATABIN_SHIELDS) {
        kind = "shield";
    } else if (table == DATABIN_ARMOUR) {
        kind = "armour";
    } else {
        return false;
    }
    if (!toml_comment(b, row->name) || !toml_int(b, "id", place)
        || !toml_text(b, "name", row->name)
        || !toml_text(b, "kind", kind)) {
        return false;
    }
    for (size_t i = 0; i < sizeof SHAPE_COLUMNS / sizeof SHAPE_COLUMNS[0];
        i++) {
        uint32_t index = 0;
        if (databin_column(bin, table, SHAPE_COLUMNS[i].column, &index)
            && index < row->number_count
            && !toml_int(b, SHAPE_COLUMNS[i].key, row->numbers[index])) {
            return false;
        }
    }

    ItemVariant variants[ITEM_MAX_VARIANTS];
    uint32_t count = items_variants(row, variants);
    if (count == 0) {
        return true;
    }
    if ((icon_width != 0 && icon_height != 0
            && !put_icon(b, icon_width, icon_height))
        || (worn != NULL && worn->character_count > 0 && !put_worn(b, worn))) {
        return false;
    }

    /* An entry for each variant: the folder with its pictures, and its
     * material and class as numbers of the rows of their tables. */
    int32_t slot = -1;
    items_slot(bin, table, row, &slot);
    for (uint32_t i = 0; i < count; i++) {
        char folder[ITEM_TOML_VARIANT_NAME_SIZE];
        char shown[ITEM_NAME_SIZE];
        /* The name the game shows, where the original gives one. */
        bool named = names != NULL && item_names_shown(names,
            variants[i].material, slot, variants[i].item_class, place,
            shown);
        if (!item_toml_variant_name(bin, variants[i].material,
                variants[i].item_class, folder)
            || !toml_table_list(b, "variant")
            || !toml_text(b, "folder", folder)
            || (named && !toml_text(b, "display_name", shown))
            || !toml_int(b, "material", variants[i].material)
            || !toml_int(b, "class", variants[i].item_class)) {
            return false;
        }
    }
    return true;
}

bool item_toml_write_magic(ByteBuffer *b, const DataBinRow *row,
    int32_t place, uint32_t icon_width, uint32_t icon_height)
{
    return item_toml_write_magic_named(b, row, place, icon_width,
        icon_height, NULL);
}

bool item_toml_write_magic_named(ByteBuffer *b, const DataBinRow *row,
    int32_t place, uint32_t icon_width, uint32_t icon_height,
    const ItemNames *names)
{
    char shown[ITEM_NAME_SIZE];
    /* The server knows a magic item by the name its row has. */
    bool named = names != NULL
        && item_names_shown_for(names, row->name, shown);
    /* A magic item's row is its price, its weight, and a text of what
     * it does, if it does anything. */
    return toml_comment(b, row->name)
        && toml_int(b, "id", place)
        && toml_text(b, "name", row->name)
        && (!named || toml_text(b, "display_name", shown))
        && toml_text(b, "kind", "magic")
        && (row->number_count < 1 || toml_int(b, "price", row->numbers[0]))
        && (row->number_count < 2 || toml_int(b, "weight", row->numbers[1]))
        && (row->text_count == 0 || row->texts[0][0] == '\0'
            || toml_text(b, "effects", row->texts[0]))
        && (icon_width == 0 || icon_height == 0
            || put_icon(b, icon_width, icon_height));
}

/* The fractions of a material or a class, after the two that are 0 in
 * every row. */
static const char *const FACTOR_KEYS[] = {
    "price", "weight", "damage", "to_hit", "defence", "absorption",
    "magic"
};
#define FACTOR_FIRST 2u

/* Adds an entry for every row of the materials or the classes. */
static bool put_factors(ByteBuffer *b, const DataBin *bin,
    DataBinTable table, const char *entry, const char *comment)
{
    /* static: a row is too large for the stack to be at ease. */
    static DataBinRow row;
    DataBinRows rows = databin_rows(bin, table);
    int32_t id = 0;
    if (!toml_comment(b, comment)) {
        return false;
    }
    while (databin_next(&rows, &row)) {
        if (!toml_table_list(b, entry) || !toml_int(b, "id", id)
            || !toml_text(b, "name", row.name)) {
            return false;
        }
        size_t keys = sizeof FACTOR_KEYS / sizeof FACTOR_KEYS[0];
        for (size_t i = 0; i < keys; i++) {
            if (FACTOR_FIRST + i < row.fraction_count
                && !toml_float(b, FACTOR_KEYS[i],
                    row.fractions[FACTOR_FIRST + i])) {
                return false;
            }
        }
        id++;
    }
    return true;
}

bool item_toml_write_materials(ByteBuffer *b, const DataBin *bin)
{
    return put_factors(b, bin, DATABIN_MATERIALS, "material",
        "The materials an item can be made of. A shape's numbers are "
        "multiplied by these.");
}

bool item_toml_write_classes(ByteBuffer *b, const DataBin *bin)
{
    return put_factors(b, bin, DATABIN_SHAPES, "class",
        "The classes an item can be of. A shape's numbers are multiplied "
        "by these.");
}

/* A modifier's row: four numbers, then how much of it each of the 12
 * places of a fighter can hold, then the same for a mage. */
#define MODIFIER_PLACES 12u

bool item_toml_write_modifiers(ByteBuffer *b, const DataBin *bin)
{
    static DataBinRow row;
    DataBinRows rows = databin_rows(bin, DATABIN_MODIFIERS);
    int32_t id = 0;
    if (!toml_comment(b, "What magic can change about whoever carries an "
            "item, and how much of it each place on the body holds.")) {
        return false;
    }
    while (databin_next(&rows, &row)) {
        bool whole = row.number_count >= 4 + 2 * MODIFIER_PLACES;
        if (!toml_table_list(b, "modifier") || !toml_int(b, "id", id)
            || !toml_text(b, "name", row.name)
            || (row.number_count >= 4
                && (!toml_int(b, "mana_cost", row.numbers[0])
                    || !toml_int(b, "affect_min", row.numbers[1])
                    || !toml_int(b, "affect_max", row.numbers[2])
                    || !toml_int(b, "usable_by", row.numbers[3])))
            || (whole
                && (!toml_int_list(b, "in_fighter", row.numbers + 4,
                        MODIFIER_PLACES)
                    || !toml_int_list(b, "in_mage",
                        row.numbers + 4 + MODIFIER_PLACES,
                        MODIFIER_PLACES)))) {
            return false;
        }
        id++;
    }
    return true;
}
