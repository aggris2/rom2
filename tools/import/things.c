#include "import/things.h"

#include <stdio.h>
#include <string.h>

bool things_count(const Things *t, uint32_t *out)
{
    int32_t count = 0;
    if (!reg_int(t->reg, "Global", t->count_key, &count) || count < 0) {
        return false;
    }
    *out = (uint32_t)count;
    return true;
}

bool things_id_at(const Things *t, uint32_t position, int32_t *out)
{
    char name[THINGS_SECTION_SIZE];
    int written = snprintf(name, sizeof name, "%s%u", t->prefix,
        (unsigned)position);
    return written > 0 && (size_t)written < sizeof name
        && reg_int(t->reg, name, "ID", out);
}

bool things_section(const Things *t, int32_t id,
    char out[THINGS_SECTION_SIZE])
{
    uint32_t count = 0;
    if (!things_count(t, &count)) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        /* A count larger than the number of sections there really are
         * ends the search at the first one that is missing. */
        int32_t section_id = 0;
        if (!things_id_at(t, i, &section_id)) {
            return false;
        }
        if (section_id == id) {
            snprintf(out, THINGS_SECTION_SIZE, "%s%u", t->prefix,
                (unsigned)i);
            return true;
        }
    }
    return false;
}

/* The one search behind the readers below. It walks from the thing up
 * through its parents and stops at the first one that has an entry
 * called `key`, whatever kind of value that is. On success `section`
 * holds the name of the section the entry was found in. */
static bool find_stated(const Things *t, int32_t id, const char *key,
    char section[THINGS_SECTION_SIZE])
{
    for (int step = 0; step <= THINGS_MAX_PARENTS; step++) {
        char name[THINGS_SECTION_SIZE];
        if (!things_section(t, id, name)) {
            return false;
        }
        RegEntry entry;
        if (reg_find(t->reg, name, key, &entry)) {
            strcpy(section, name);
            return true;
        }
        if (!reg_int(t->reg, name, "Parent", &id)) {
            return false;
        }
    }
    return false;
}

bool things_has(const Things *t, int32_t id, const char *key)
{
    char section[THINGS_SECTION_SIZE];
    return find_stated(t, id, key, section);
}

bool things_int(const Things *t, int32_t id, const char *key, int32_t *out)
{
    char section[THINGS_SECTION_SIZE];
    return find_stated(t, id, key, section)
        && reg_int(t->reg, section, key, out);
}

bool things_text(const Things *t, int32_t id, const char *key,
    const char **out)
{
    char section[THINGS_SECTION_SIZE];
    return find_stated(t, id, key, section)
        && reg_text(t->reg, section, key, out);
}

bool things_int_list(const Things *t, int32_t id, const char *key,
    int32_t *out, size_t capacity, size_t *out_count)
{
    char section[THINGS_SECTION_SIZE];
    return find_stated(t, id, key, section)
        && reg_int_list(t->reg, section, key, out, capacity, out_count);
}
