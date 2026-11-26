#include "import/spell_toml.h"

#include "import/toml.h"

/* A column of data.bin's spells and the name ROM2 gives it. */
typedef struct SpellColumn {
    const char *column;
    const char *key;
} SpellColumn;

static const SpellColumn COLUMNS[] = {
    {"Complication Level", "level"},
    {"Mana Cost", "mana_cost"},
    {"Sphere", "sphere"},
    {"Item", "item"},
    {"Spell Target", "target"},
    {"Delivery System", "delivery"},
    {"Max Range", "max_range"},
    {"Spell Effect Speed", "effect_speed"},
    {"Distribution system", "distribution"},
    /* The file has this name with its quotes. */
    {"\"Radius, Length/2\"", "radius"},
    {"Area Effect Affect", "area_affect"},
    {"Area Effect Duaration", "area_duration"},
    {"Area Effect Frequency", "area_frequency"},
    {"Apply on Unit Method", "apply_method"},
    {"Spell Duration", "duration"},
    {"Spell Frequency", "frequency"},
    {"damageMin", "damage_min"},
    {"damageMax", "damage_max"},
    {"Defensive", "defensive"},
    {"SkillOffset", "skill_offset"},
    {"scrollCost", "scroll_cost"},
    {"bookCost", "book_cost"},
};

bool spell_toml_folder_name(int32_t id, const DataBinRow *row,
    char out[NAME_FOLDER_SIZE])
{
    return name_folder(id, row->name, out);
}

bool spell_toml_write(ByteBuffer *b, const DataBin *bin, int32_t id,
    const DataBinRow *row)
{
    if (!toml_comment(b, row->name) || !toml_int(b, "id", id)
        || !toml_text(b, "name", row->name)) {
        return false;
    }
    for (size_t i = 0; i < sizeof COLUMNS / sizeof COLUMNS[0]; i++) {
        uint32_t index = 0;
        if (databin_column(bin, DATABIN_SPELLS, COLUMNS[i].column, &index)
            && index < row->number_count
            && !toml_int(b, COLUMNS[i].key, row->numbers[index])) {
            return false;
        }
    }
    /* The one text of a spell's row is its effects. Most have none. */
    return row->text_count == 0 || row->texts[0][0] == '\0'
        || toml_text(b, "effects", row->texts[0]);
}
