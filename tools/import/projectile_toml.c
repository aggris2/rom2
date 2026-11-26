#include "import/projectile_toml.h"

#include <stdio.h>

#include "import/projectiles.h"
#include "import/toml.h"

bool projectile_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE])
{
    Things t = projectiles_things(reg);
    char section[THINGS_SECTION_SIZE];
    const char *text = "";
    if (!things_section(&t, id, section)) {
        return false;
    }
    things_text(&t, id, "File", &text);
    return name_folder(id, text, out);
}

/* Adds a whole number of the projectile under another name, if it is
 * stated. */
static bool copy_int(ByteBuffer *b, const Things *t, int32_t id,
    const char *key, const char *name)
{
    int32_t value = 0;
    return !things_int(t, id, key, &value) || toml_int(b, name, value);
}

bool projectile_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const ProjectileSheet *sheet)
{
    Things t = projectiles_things(reg);
    char section[THINGS_SECTION_SIZE];
    char name[128];
    const char *text = "";
    if (!things_section(&t, id, section)) {
        return false;
    }
    things_text(&t, id, "File", &text);
    snprintf(name, sizeof name, "%s", text);
    for (char *p = name; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }

    const int32_t cell[2] = {
        (int32_t)sheet->cell_width, (int32_t)sheet->cell_height
    };
    const int32_t anchor[2] = {cell[0] / 2, cell[1] / 2};

    return toml_comment(b, name)
        && toml_int(b, "id", id)
        && toml_text(b, "name", name)
        && copy_int(b, &t, id, "Homing", "homing")
        && copy_int(b, &t, id, "SFX", "sfx")
        && toml_table(b, "sprite")
        && toml_text(b, "sheet", "sprites.png")
        && toml_text(b, "alpha", "alpha.png")
        && toml_text(b, "palettes", "palettes.png")
        && toml_int(b, "palette_rows", 1)
        && toml_int_list(b, "cell", cell, 2)
        && toml_int_list(b, "anchor", anchor, 2)
        && toml_int(b, "columns", (int32_t)sheet->columns)
        && toml_int(b, "rows", (int32_t)sheet->rows);
}
