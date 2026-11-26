#include "import/units.h"

#include <string.h>

#include "import/things.h"

/* The units are the things of units.reg: "UnitCount" of them, in the
 * sections "Unit0", "Unit1" and so on. */
static Things units(const Reg *r)
{
    Things t = {r, "UnitCount", "Unit"};
    return t;
}

bool units_section(const Reg *r, int32_t id, char out[UNITS_SECTION_SIZE])
{
    Things t = units(r);
    char name[THINGS_SECTION_SIZE];
    if (!things_section(&t, id, name)
        || strlen(name) >= UNITS_SECTION_SIZE) {
        return false;
    }
    strcpy(out, name);
    return true;
}

bool units_has(const Reg *r, int32_t id, const char *key)
{
    Things t = units(r);
    return things_has(&t, id, key);
}

bool units_int(const Reg *r, int32_t id, const char *key, int32_t *out)
{
    Things t = units(r);
    return things_int(&t, id, key, out);
}

bool units_text(const Reg *r, int32_t id, const char *key,
    const char **out)
{
    Things t = units(r);
    return things_text(&t, id, key, out);
}

bool units_int_list(const Reg *r, int32_t id, const char *key,
    int32_t *out, size_t capacity, size_t *out_count)
{
    Things t = units(r);
    return things_int_list(&t, id, key, out, capacity, out_count);
}
