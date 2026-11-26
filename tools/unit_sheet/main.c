/* unit_sheet: writes the sprite sheets of one unit as PNG pictures.
 *
 *     unit_sheet <graphics.res> <unit id> <out.png>
 *
 * The main sheet goes to <out.png>. The dying sheet goes next to it, with
 * "-dying" added to the name: out-dying.png.
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
#include "import/units.h"

static const char *const ANIM_NAMES[UNIT_ANIM_COUNT] = {
    "move", "attack", "dying", "bones", "idle"
};

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
        printf("unit %d \"%s\": %s has %u frames, and the registry's "
            "numbers describe %u\n", (int)id, out->name, out->path,
            out->sprite.frame_count, out->layout.frame_count);
        return "the sprite file does not match the registry";
    }
    return NULL;
}

/* Writes a sheet as a PNG file and says what is on it. */
static bool sheet_write(const UnitSheet *sheet, const Rgb palette[256],
    const char *path)
{
    ByteBuffer png = {0};
    bool ok = png_write_indexed(&png, sheet_width(&sheet->sheet),
            sheet_height(&sheet->sheet), sheet->sheet.pixels, palette)
        && file_write_all(path, png.data, png.size);
    if (ok) {
        uint32_t first = sheet->first_direction_row;
        printf("  %s\n", path);
        printf("    %u columns x %u rows of %u x %u, %u x %u pixels, "
            "%zu bytes\n", sheet->sheet.columns, sheet->sheet.rows,
            sheet->sheet.cell_width, sheet->sheet.cell_height,
            sheet_width(&sheet->sheet), sheet_height(&sheet->sheet),
            png.size);
        printf("    anchor %d, %d\n", (int)sheet->anchor_x,
            (int)sheet->anchor_y);
        if (first > 0) {
            printf("    row 0     standing, 16 steps\n");
        }
        for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
            if (sheet->columns[anim] > 0) {
                printf("    rows %u-%u  %-7s columns %u to %u\n", first,
                    first + UNIT_DIRECTIONS - 1, ANIM_NAMES[anim],
                    sheet->column[anim],
                    sheet->column[anim] + sheet->columns[anim] - 1);
            }
        }
    }
    buffer_free(&png);
    return ok;
}

/* Makes the name of the dying sheet from the name of the main one:
 * "out.png" gives "out-dying.png". */
static bool dying_path(const char *path, char *out, size_t size)
{
    size_t length = strlen(path);
    size_t stem = length;
    if (length >= 4 && strcmp(path + length - 4, ".png") == 0) {
        stem = length - 4;
    }
    int written = snprintf(out, size, "%.*s-dying.png", (int)stem, path);
    return written >= 0 && (size_t)written < size;
}

/* Does the work once the archive is in memory. Returns a message saying
 * what went wrong, or NULL if the pictures were written. */
static const char *write_unit_sheets(const uint8_t *data, size_t size,
    int32_t id, const char *out_path)
{
    /* static: two of these are too large for the stack to be at ease. */
    static Unit unit;
    static Unit donor;
    ResArchive archive;
    ResEntry entry;
    Reg reg;
    if (!res_open(data, size, &archive)) {
        return "not a .res archive";
    }
    if (!res_find(&archive, "units/units.reg", &entry) || entry.is_folder
        || !reg_open(archive.data + entry.offset, entry.size, &reg)) {
        return "no units/units.reg in the archive";
    }

    const char *problem = unit_read(&archive, &reg, id, &unit);
    if (problem != NULL) {
        return problem;
    }
    printf("unit %d \"%s\"\n", (int)id, unit.name);
    printf("  sprite %s, %u frames%s\n", unit.path, unit.sprite.frame_count,
        unit.layout.flip ? ", half of the directions mirrored" : "");
    printf("  box %d x %d, centre %d, %d\n", (int)unit.width,
        (int)unit.height, (int)unit.center_x, (int)unit.center_y);

    UnitSheet sheet;
    if (!unit_sheet_build(&unit.layout, &unit.sprite, (uint32_t)unit.width,
            (uint32_t)unit.height, unit.center_x, unit.center_y, &sheet)) {
        return "the main sheet could not be built";
    }
    bool ok = sheet_write(&sheet, unit.palette, out_path);
    unit_sheet_free(&sheet);
    if (!ok) {
        return "the main sheet could not be written";
    }

    /* The dying frames are the unit's own, or those of the unit its
     * "Dying" names. They keep the palette of the file they are in. */
    const Unit *source = &unit;
    if (!unit.layout.own_dying) {
        problem = unit_read(&archive, &reg, unit.layout.dying_id, &donor);
        if (problem != NULL) {
            printf("  the dying frames should come from unit %d\n",
                (int)unit.layout.dying_id);
            return problem;
        }
        source = &donor;
        printf("  dying frames from unit %d \"%s\", %s\n", (int)donor.id,
            donor.name, donor.path);
    }
    if (source->layout.phases[UNIT_ANIM_DYING] == 0
        && source->layout.phases[UNIT_ANIM_BONES] == 0) {
        printf("  no dying frames\n");
        return NULL;
    }

    char path[512];
    if (!dying_path(out_path, path, sizeof path)) {
        return "the name of the output file is too long";
    }
    if (!unit_sheet_build_dying(&source->layout, &source->sprite,
            (uint32_t)source->width, (uint32_t)source->height,
            source->center_x, source->center_y, &sheet)) {
        return "the dying sheet could not be built";
    }
    ok = sheet_write(&sheet, source->palette, path);
    unit_sheet_free(&sheet);
    return ok ? NULL : "the dying sheet could not be written";
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: unit_sheet <graphics.res> <unit id> "
            "<out.png>\n");
        return 2;
    }

    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(argv[1], &data, &size)) {
        fprintf(stderr, "unit_sheet: cannot read %s\n", argv[1]);
        return 1;
    }
    const char *problem = write_unit_sheets(data, size, atoi(argv[2]),
        argv[3]);
    free(data);
    if (problem != NULL) {
        fprintf(stderr, "unit_sheet: %s\n", problem);
        return 1;
    }
    return 0;
}
