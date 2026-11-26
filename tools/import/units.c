#include "import/units.h"

#include <stdio.h>
#include <string.h>

/* A parent's parent's parent, and so on: no further than this. The
 * original game's units go one step. A longer chain means a damaged
 * registry, or units that name each other in a circle. */
#define UNITS_MAX_PARENTS 8

bool units_section(const Reg *r, int32_t id, char out[UNITS_SECTION_SIZE])
{
    int32_t count = 0;
    if (!reg_int(r, "Global", "UnitCount", &count)) {
        return false;
    }
    for (int32_t i = 0; i < count; i++) {
        char name[UNITS_SECTION_SIZE];
        snprintf(name, sizeof name, "Unit%d", (int)i);

        /* A count larger than the number of sections there really are
         * ends the search at the first one that is missing. */
        int32_t section_id = 0;
        if (!reg_int(r, name, "ID", &section_id)) {
            return false;
        }
        if (section_id == id) {
            strcpy(out, name);
            return true;
        }
    }
    return false;
}

/* The one search behind the three readers below. It walks from the unit
 * up through its parents and stops at the first one that has an entry
 * called `key`, whatever kind of value that is. On success `section`
 * holds the name of the section the entry was found in. */
static bool find_stated(const Reg *r, int32_t id, const char *key,
    char section[UNITS_SECTION_SIZE])
{
    for (int step = 0; step <= UNITS_MAX_PARENTS; step++) {
        char name[UNITS_SECTION_SIZE];
        if (!units_section(r, id, name)) {
            return false;
        }
        RegEntry entry;
        if (reg_find(r, name, key, &entry)) {
            strcpy(section, name);
            return true;
        }
        if (!reg_int(r, name, "Parent", &id)) {
            return false;
        }
    }
    return false;
}

bool units_int(const Reg *r, int32_t id, const char *key, int32_t *out)
{
    char section[UNITS_SECTION_SIZE];
    return find_stated(r, id, key, section)
        && reg_int(r, section, key, out);
}

bool units_text(const Reg *r, int32_t id, const char *key,
    const char **out)
{
    char section[UNITS_SECTION_SIZE];
    return find_stated(r, id, key, section)
        && reg_text(r, section, key, out);
}

bool units_int_list(const Reg *r, int32_t id, const char *key,
    int32_t *out, size_t capacity, size_t *out_count)
{
    char section[UNITS_SECTION_SIZE];
    return find_stated(r, id, key, section)
        && reg_int_list(r, section, key, out, capacity, out_count);
}
