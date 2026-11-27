/* A small builder for registries, for tests that need a registry with
 * particular sections and values in it.
 *
 *     MakeReg m;
 *     make_reg_start(&m);
 *     make_reg_section(&m, "Global");
 *     make_reg_int(&m, "UnitCount", 2);
 *     make_reg_section(&m, "Unit0");
 *     make_reg_text(&m, "DescText", "Swordsman");
 *     size_t size = make_reg_finish(&m);
 *     ... use m.bytes and size ...
 *
 * It builds well-formed registries only. Tests of damaged ones write their
 * bytes by hand.
 */
#ifndef ROM2_TESTS_IMPORT_MAKE_REG_H
#define ROM2_TESTS_IMPORT_MAKE_REG_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    MAKE_REG_MAX_SECTIONS = 32,
    MAKE_REG_MAX_KEYS = 128,
    MAKE_REG_MAX_VALUES = 2048,
    MAKE_REG_MAX_BYTES = 24
        + (MAKE_REG_MAX_SECTIONS + MAKE_REG_MAX_KEYS) * 32
        + 4 + MAKE_REG_MAX_VALUES
};

typedef struct MakeRegKey {
    int section;
    uint32_t type;
    uint32_t a;
    uint32_t b;
    char name[17];
} MakeRegKey;

typedef struct MakeReg {
    char sections[MAKE_REG_MAX_SECTIONS][17];
    int section_count;
    MakeRegKey keys[MAKE_REG_MAX_KEYS];
    int key_count;
    uint8_t values[MAKE_REG_MAX_VALUES];
    uint32_t values_size;
    uint8_t bytes[MAKE_REG_MAX_BYTES];
} MakeReg;

static inline void make_reg_put_u32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)(value & 0xff);
    at[1] = (uint8_t)((value >> 8) & 0xff);
    at[2] = (uint8_t)((value >> 16) & 0xff);
    at[3] = (uint8_t)((value >> 24) & 0xff);
}

static inline void make_reg_start(MakeReg *m)
{
    memset(m, 0, sizeof *m);
}

/* Starts a section. The values added next go into it. */
static inline void make_reg_section(MakeReg *m, const char *name)
{
    strncpy(m->sections[m->section_count], name, 16);
    m->section_count++;
}

static inline MakeRegKey *make_reg_key(MakeReg *m, const char *name,
    uint32_t type)
{
    MakeRegKey *key = &m->keys[m->key_count];
    m->key_count++;
    key->section = m->section_count - 1;
    key->type = type;
    strncpy(key->name, name, 16);
    return key;
}

static inline void make_reg_int(MakeReg *m, const char *name, int32_t value)
{
    make_reg_key(m, name, 2)->a = (uint32_t)value;
}

/* Adds a decimal number, given as the two halves of its 64 bits: the
 * lower and the upper. 1.0 is 0 and 0x3ff00000. */
static inline void make_reg_float(MakeReg *m, const char *name,
    uint32_t low, uint32_t high)
{
    MakeRegKey *key = make_reg_key(m, name, 4);
    key->a = low;
    key->b = high;
}

static inline void make_reg_text(MakeReg *m, const char *name,
    const char *text)
{
    MakeRegKey *key = make_reg_key(m, name, 0);
    uint32_t length = (uint32_t)strlen(text) + 1;
    key->a = m->values_size;
    key->b = length;
    memcpy(m->values + m->values_size, text, length);
    m->values_size += length;
}

static inline void make_reg_int_list(MakeReg *m, const char *name,
    const int32_t *numbers, uint32_t count)
{
    MakeRegKey *key = make_reg_key(m, name, 6);
    key->a = m->values_size;
    key->b = count * 4;
    for (uint32_t i = 0; i < count; i++) {
        make_reg_put_u32(m->values + m->values_size, (uint32_t)numbers[i]);
        m->values_size += 4;
    }
}

static inline void make_reg_put_entry(uint8_t *at, uint32_t type,
    uint32_t a, uint32_t b, const char *name)
{
    memset(at, 0, 32);
    make_reg_put_u32(at + 4, a);
    make_reg_put_u32(at + 8, b);
    make_reg_put_u32(at + 12, type);
    memcpy(at + 16, name, strlen(name));
}

/* Lays the registry out in m->bytes and returns its size. The section
 * entries come first, then each section's values in the order they were
 * added. */
static inline size_t make_reg_finish(MakeReg *m)
{
    uint32_t sections = (uint32_t)m->section_count;
    uint32_t entries = sections + (uint32_t)m->key_count;
    uint8_t *table = m->bytes + 24;

    memset(m->bytes, 0, sizeof m->bytes);
    make_reg_put_u32(m->bytes + 0, 0x31415926u);
    make_reg_put_u32(m->bytes + 4, 0);
    make_reg_put_u32(m->bytes + 8, sections);
    make_reg_put_u32(m->bytes + 12, 17);
    make_reg_put_u32(m->bytes + 16, entries);

    uint32_t next = sections;
    for (int s = 0; s < m->section_count; s++) {
        uint32_t first = next;
        for (int k = 0; k < m->key_count; k++) {
            const MakeRegKey *key = &m->keys[k];
            if (key->section == s) {
                make_reg_put_entry(table + next * 32, key->type, key->a,
                    key->b, key->name);
                next++;
            }
        }
        make_reg_put_entry(table + (uint32_t)s * 32, 1, first, next - first,
            m->sections[s]);
    }

    uint8_t *after_table = table + entries * 32;
    make_reg_put_u32(after_table, m->values_size);
    memcpy(after_table + 4, m->values, m->values_size);
    return 24 + (size_t)entries * 32 + 4 + m->values_size;
}

#endif
