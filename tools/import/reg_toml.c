#include "import/reg_toml.h"

#include <stdio.h>
#include <string.h>

#include "base/toml.h"

/* Writes `name` as a TOML key to `out`, which has room for 40
 * characters: as it is if it may stand bare, in quotes if not. A name
 * is at most 16 characters, and the original has no '"' or '\' in one;
 * such a name is refused. */
static bool key_name(const char *name, char out[40])
{
    bool bare = name[0] != '\0';
    for (const char *c = name; *c != '\0'; c++) {
        bool plain = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z')
            || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-';
        if (*c == '"' || *c == '\\' || (unsigned char)*c < 0x20) {
            return false;
        }
        bare = bare && plain;
    }
    snprintf(out, 40, bare ? "%.16s" : "\"%.16s\"", name);
    return true;
}

/* Writes one value of a section. */
static bool put_value(ByteBuffer *b, const Reg *reg, const char *section,
    const RegEntry *entry)
{
    char key[40];
    if (!key_name(entry->name, key)) {
        return false;
    }
    switch (entry->type) {
    case REG_INT: {
        int32_t number = 0;
        return reg_int(reg, section, entry->name, &number)
            && toml_int(b, key, number);
    }
    case REG_FLOAT: {
        double number = 0;
        return reg_float(reg, section, entry->name, &number)
            && toml_float(b, key, number);
    }
    case REG_TEXT: {
        const char *text = NULL;
        return reg_text(reg, section, entry->name, &text)
            && toml_text(b, key, text);
    }
    case REG_INT_LIST: {
        int32_t numbers[REG_TOML_MAX_LIST];
        size_t count = 0;
        return reg_int_list(reg, section, entry->name, numbers,
                REG_TOML_MAX_LIST, &count)
            && toml_int_list(b, key, numbers, count);
    }
    default:
        /* A section inside a section: the original has none. */
        return false;
    }
}

bool reg_toml_write(ByteBuffer *b, const Reg *reg)
{
    for (uint32_t i = 0; i < reg->root_count; i++) {
        RegEntry section;
        char name[40];
        if (!reg_entry_at(reg, reg->root_first + i, &section)
            || section.type != REG_SECTION
            || !key_name(section.name, name)
            || !toml_table(b, name)) {
            return false;
        }
        /* A section's `a` is its first entry and `b` how many it has. */
        for (uint32_t k = 0; k < section.b; k++) {
            RegEntry entry;
            if (!reg_entry_at(reg, section.a + k, &entry)
                || !put_value(b, reg, section.name, &entry)) {
                return false;
            }
        }
    }
    return true;
}
