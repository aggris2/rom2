/* import_units: writes the units of the original game as ROM2 content.
 *
 *     import_units <graphics.res> <content folder> [unit id]
 *
 * Every unit gets a folder of its own in <content folder>/units, named
 * after its ID and its name, with three files in it:
 *
 *     unit.toml     the unit's definition
 *     sprites.png   standing, moving, attacking and idle
 *     dying.png     dying and bones
 *
 * With a unit ID only that unit is written.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/png.h"
#include "import/reg.h"
#include "import/res.h"
#include "import/sprite256.h"
#include "import/unit_layout.h"
#include "import/unit_sheet.h"
#include "import/unit_toml.h"
#include "import/units.h"

/* Builds the archive path of a unit's sprite from its "File" number:
 * the text "FileN" in the section "Files", with '\' turned into '/',
 * "units/" in front and ".256" behind. */
static bool sprite_path(const Reg *reg, int32_t id, char *out, size_t size)
{
    int32_t file = 0;
    char key[32];
    const char *text = NULL;
    if (!units_int(reg, id, "File", &file)) {
        return false;
    }
    snprintf(key, sizeof key, "File%d", (int)file);
    if (!reg_text(reg, "Files", key, &text)) {
        return false;
    }
    int written = snprintf(out, size, "units/%s.256", text);
    if (written < 0 || (size_t)written >= size) {
        return false;
    }
    for (char *p = out; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    return true;
}

/* A unit as the registry describes it, with its sprite file opened. */
typedef struct Unit {
    int32_t id;
    const char *name;
    UnitLayout layout;
    int32_t width;
    int32_t height;
    int32_t center_x;
    int32_t center_y;
    char path[128];
    Sprite256 sprite;
    Rgb palette[256];
} Unit;

/* Reads everything about one unit. Returns a message saying what went
 * wrong, or NULL. */
static const char *unit_read(const ResArchive *archive, const Reg *reg,
    int32_t id, Unit *out)
{
    ResEntry entry;
    memset(out, 0, sizeof *out);
    out->id = id;
    out->name = "?";
    if (!unit_layout_read(reg, id, &out->layout)) {
        return "no unit with that ID, or its numbers are not usable";
    }
    if (!units_int(reg, id, "Width", &out->width) || out->width <= 0
        || !units_int(reg, id, "Height", &out->height) || out->height <= 0
        || !units_int(reg, id, "CenterX", &out->center_x)
        || !units_int(reg, id, "CenterY", &out->center_y)) {
        return "the unit has no usable Width, Height, CenterX or CenterY";
    }
    units_text(reg, id, "DescText", &out->name);

    if (!sprite_path(reg, id, out->path, sizeof out->path)
        || !res_find(archive, out->path, &entry) || entry.is_folder
        || !sprite256_open(archive->data + entry.offset, entry.size,
            &out->sprite)) {
        return "the unit's sprite file is not in the archive";
    }
    if (!sprite256_palette(&out->sprite, out->palette)) {
        return "the unit's sprite file has no palette";
    }
    if (out->sprite.frame_count != out->layout.frame_count) {
        return "the sprite file does not have the number of frames the "
            "registry describes";
    }
    return NULL;
}

/* Writes a sheet as a PNG file in a folder. */
static bool sheet_write(const UnitSheet *sheet, const Rgb palette[256],
    const char *folder, const char *name)
{
    char path[1024];
    int written = snprintf(path, sizeof path, "%s/%s", folder, name);
    if (written < 0 || (size_t)written >= sizeof path) {
        return false;
    }
    ByteBuffer png = {0};
    bool ok = png_write_indexed(&png, sheet_width(&sheet->sheet),
            sheet_height(&sheet->sheet), sheet->sheet.pixels, palette)
        && file_write_all(path, png.data, png.size);
    buffer_free(&png);
    return ok;
}

/* Writes the folder of one unit. Returns a message saying what went
 * wrong, or NULL. */
static const char *unit_write(const ResArchive *archive, const Reg *reg,
    int32_t id, const char *units_folder)
{
    /* static: two of these are too large for the stack to be at ease. */
    static Unit unit;
    static Unit donor;
    const char *problem = unit_read(archive, reg, id, &unit);
    if (problem != NULL) {
        return problem;
    }

    /* The dying frames are the unit's own, or those of the unit its
     * "Dying" names. They keep the palette of the file they are in. */
    const Unit *source = &unit;
    if (!unit.layout.own_dying) {
        if (unit_read(archive, reg, unit.layout.dying_id, &donor) != NULL) {
            return "the unit its dying frames come from cannot be read";
        }
        source = &donor;
    }
    bool has_dying = source->layout.phases[UNIT_ANIM_DYING] > 0
        || source->layout.phases[UNIT_ANIM_BONES] > 0;

    char name[UNIT_TOML_FOLDER_SIZE];
    char folder[1024];
    if (!unit_toml_folder_name(reg, id, name)) {
        return "the unit's ID does not make a folder name";
    }
    int written = snprintf(folder, sizeof folder, "%s/%s", units_folder,
        name);
    if (written < 0 || (size_t)written >= sizeof folder - 16) {
        return "the name of the content folder is too long";
    }

    UnitSheet sheet;
    UnitSheet dying;
    if (!unit_sheet_build(&unit.layout, &unit.sprite, (uint32_t)unit.width,
            (uint32_t)unit.height, unit.center_x, unit.center_y, &sheet)) {
        return "the main sheet could not be built";
    }
    if (has_dying && !unit_sheet_build_dying(&source->layout,
            &source->sprite, (uint32_t)source->width,
            (uint32_t)source->height, source->center_x, source->center_y,
            &dying)) {
        unit_sheet_free(&sheet);
        return "the dying sheet could not be built";
    }

    ByteBuffer toml = {0};
    char path[1040];
    snprintf(path, sizeof path, "%s/unit.toml", folder);
    if (!unit_toml_write(&toml, reg, id, &unit.layout, &sheet,
            has_dying ? &dying : NULL)) {
        problem = "the unit's values could not be written out";
    } else if (!file_make_folder(folder)
        || !file_write_all(path, toml.data, toml.size)
        || !sheet_write(&sheet, unit.palette, folder, "sprites.png")
        || (has_dying
            && !sheet_write(&dying, source->palette, folder, "dying.png"))) {
        problem = "the unit's folder could not be written";
    }
    buffer_free(&toml);
    unit_sheet_free(&sheet);
    if (has_dying) {
        unit_sheet_free(&dying);
    }
    if (problem == NULL) {
        printf("  %s\n", name);
    }
    return problem;
}

/* Does the work once the archive is in memory. `only` is the one unit to
 * write, or -1 for all. Returns how many units could not be written, or
 * -1 if nothing could be done at all. */
static int import_units(const uint8_t *data, size_t size,
    const char *content_folder, int32_t only)
{
    ResArchive archive;
    ResEntry entry;
    Reg reg;
    int32_t count = 0;
    if (!res_open(data, size, &archive)) {
        fprintf(stderr, "import_units: not a .res archive\n");
        return -1;
    }
    if (!res_find(&archive, "units/units.reg", &entry) || entry.is_folder
        || !reg_open(archive.data + entry.offset, entry.size, &reg)
        || !reg_int(&reg, "Global", "UnitCount", &count)) {
        fprintf(stderr, "import_units: no units/units.reg in the archive\n");
        return -1;
    }

    char units_folder[1000];
    int written = snprintf(units_folder, sizeof units_folder, "%s/units",
        content_folder);
    if (written < 0 || (size_t)written >= sizeof units_folder
        || !file_make_folder(content_folder)
        || !file_make_folder(units_folder)) {
        fprintf(stderr, "import_units: cannot make the folder %s/units\n",
            content_folder);
        return -1;
    }

    /* The units are the sections "Unit0", "Unit1" and so on. */
    int done = 0;
    int failed = 0;
    printf("%s\n", units_folder);
    for (int32_t i = 0; i < count; i++) {
        char section[32];
        int32_t id = 0;
        snprintf(section, sizeof section, "Unit%d", (int)i);
        if (!reg_int(&reg, section, "ID", &id)
            || (only >= 0 && id != only)) {
            continue;
        }
        const char *problem = unit_write(&archive, &reg, id, units_folder);
        if (problem == NULL) {
            done++;
        } else {
            const char *name = "?";
            units_text(&reg, id, "DescText", &name);
            fprintf(stderr, "  unit %d \"%s\" left out: %s\n", (int)id,
                name, problem);
            failed++;
        }
    }
    if (only >= 0 && done + failed == 0) {
        fprintf(stderr, "import_units: no unit has the ID %d\n", (int)only);
        return -1;
    }
    printf("%d units written, %d left out\n", done, failed);
    return failed;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: import_units <graphics.res> "
            "<content folder> [unit id]\n");
        return 2;
    }

    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(argv[1], &data, &size)) {
        fprintf(stderr, "import_units: cannot read %s\n", argv[1]);
        return 1;
    }
    int32_t only = argc == 4 ? atoi(argv[3]) : -1;
    int failed = import_units(data, size, argv[2], only);
    free(data);
    /* Asking for one unit and not getting it is a failure. Importing
     * all of them and leaving some out is reported and is not. */
    return failed < 0 || (only >= 0 && failed > 0) ? 1 : 0;
}
