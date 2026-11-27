/* The units step of rom2_import: writes the units of the original game as
 * ROM2 content.
 *
 * Every unit gets a folder of its own in <content folder>/units, named
 * after its ID and its name, with three files in it:
 *
 *     unit.toml      the unit's definition, and its kinds with their
 *                    numbers, which come from data.bin in world.res
 *     sprites.png    standing, moving, attacking and idle
 *     dying.png      dying and bones
 *     edges.png, dying_edges.png
 *                    the soft edges of those two, to draw under them
 *     palettes.png   the unit's colourings, one in each row
 *
 * A unit whose dying frames come from a unit with other colours also
 * gets dying_palettes.png.
 *
 * With a unit ID only that unit is written.
 */
#include "import/steps/steps.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/databin.h"
#include "import/pal.h"
#include "base/png.h"
#include "import/reg.h"
#include "base/toml.h"
#include "import/res.h"
#include "import/sprite256.h"
#include "import/unit.h"
#include "import/steps/shared.h"

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

/* The most colourings a unit may have. The game's most is 16, the
 * player colours. */
#define MAX_PALETTES 32

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
    /* The sprite file with the soft edges, the one whose name ends in
     * "b", if the unit has one. */
    Sprite256 edges;
    bool has_edges;
    /* The unit's colourings: `palette_rows` palettes of 256 colours, one
     * after the other. The first is the one the sheets are written with.
     * `by_player` says whether the player picks one, or the unit's face. */
    Rgb palettes[MAX_PALETTES * 256];
    uint32_t palette_rows;
    bool by_player;
    /* Whether the file has more dying phases than the registry gives,
     * and how many the registry gives. */
    bool dying_corrected;
    uint32_t dying_said;
} Unit;

/* Reads the .pal file with the given name next to the unit's sprite
 * file, or, with `levels_up` 1, one folder further up. Sets `*data` and
 * `*size` to its bytes. */
static bool pal_file(const ResArchive *archive, const Unit *unit,
    int levels_up, const char *name, const uint8_t **data, size_t *size)
{
    char path[160];
    ResEntry entry;
    /* The folder is the sprite's path up to its last '/', or to the one
     * before that. */
    size_t length = strlen(unit->path);
    for (int cut = 0; cut <= levels_up; cut++) {
        while (length > 0 && unit->path[length - 1] != '/') {
            length--;
        }
        if (length == 0) {
            return false;
        }
        if (cut < levels_up) {
            length--;
        }
    }
    int written = snprintf(path, sizeof path, "%.*s%s", (int)length,
        unit->path, name);
    if (written < 0 || (size_t)written >= sizeof path
        || !res_find(archive, path, &entry) || entry.is_folder) {
        return false;
    }
    *data = archive->data + entry.offset;
    *size = entry.size;
    return true;
}

/* Reads the unit's colourings. The registry's "Palette" says how many
 * the unit has of its own: palette.pal, palette2.pal and so on, next to
 * its sprite file. 0 stands for the humans' shared human.pal, one folder
 * up, with a palette for each player colour. A unit with neither has the
 * one palette inside its sprite file. */
static const char *unit_read_palettes(const ResArchive *archive,
    const Reg *reg, Unit *unit)
{
    int32_t count = 1;
    const uint8_t *data = NULL;
    size_t size = 0;
    units_int(reg, unit->id, "Palette", &count);
    if (count < 0 || count > MAX_PALETTES) {
        return "the unit's number of palettes is not usable";
    }

    /* Whatever follows, the sprite's own palette is the fallback. */
    if (!sprite256_palette(&unit->sprite, unit->palettes)) {
        return "the unit's sprite file has no palette";
    }
    unit->palette_rows = 1;
    unit->by_player = false;

    if (count == 0) {
        if (!pal_file(archive, unit, 1, "human.pal", &data, &size)) {
            return NULL;
        }
        uint32_t rows = pal_table_count(size);
        if (rows == 0 || rows > MAX_PALETTES) {
            return "human.pal is not a table of palettes";
        }
        for (uint32_t i = 0; i < rows; i++) {
            pal_table_read(data, size, i, unit->palettes + i * 256);
        }
        unit->palette_rows = rows;
        unit->by_player = true;
        return NULL;
    }

    for (int32_t i = 0; i < count; i++) {
        char name[32];
        if (i == 0) {
            snprintf(name, sizeof name, "palette.pal");
        } else {
            snprintf(name, sizeof name, "palette%d.pal", (int)i + 1);
        }
        if (!pal_file(archive, unit, 0, name, &data, &size)) {
            /* The first may be missing: it is the one in the sprite. */
            if (i == 0) {
                continue;
            }
            return "one of the unit's .pal files is missing";
        }
        if (!pal_bmp_read(data, size, unit->palettes + (size_t)i * 256)) {
            return "one of the unit's .pal files cannot be read";
        }
    }
    unit->palette_rows = (uint32_t)count;
    return NULL;
}

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
    if (out->sprite.frame_count != out->layout.frame_count) {
        /* A file may have more dying frames than the registry says. */
        uint32_t said = out->layout.phases[UNIT_ANIM_DYING];
        if (!unit_layout_fit_dying(&out->layout,
                out->sprite.frame_count)) {
            return "the sprite file does not have the number of frames "
                "the registry describes";
        }
        out->dying_said = said;
        out->dying_corrected = true;
    }

    /* The soft edges are in a file of the same name with "b" before the
     * ending. A unit without one is drawn without them. */
    char edges_path[sizeof out->path + 1];
    size_t stem = strlen(out->path) - 4;
    snprintf(edges_path, sizeof edges_path, "%.*sb.256", (int)stem,
        out->path);
    out->has_edges = res_find(archive, edges_path, &entry)
        && !entry.is_folder
        && sprite256_open(archive->data + entry.offset, entry.size,
            &out->edges)
        && out->edges.frame_count == out->sprite.frame_count;
    return unit_read_palettes(archive, reg, out);
}

/* Writes a unit's palettes as a picture, 256 pixels wide with one row
 * for each palette. */
static bool palettes_write(const Unit *unit, const char *folder,
    const char *name)
{
    char path[1024];
    int written = snprintf(path, sizeof path, "%s/%s", folder, name);
    if (written < 0 || (size_t)written >= sizeof path) {
        return false;
    }
    ByteBuffer png = {0};
    bool ok = png_write_rgb(&png, 256, unit->palette_rows, unit->palettes)
        && file_write_all(path, png.data, png.size);
    buffer_free(&png);
    return ok;
}

/* Writes the folder of one unit. Returns a message saying what went
 * wrong, or NULL. */
static const char *unit_write(const ResArchive *archive, const Reg *reg,
    const DataBin *bin, int32_t id, const char *units_folder,
    uint32_t *kind_count)
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
    if (!unit_sheet_build(&unit.layout, &unit.sprite,
            unit.has_edges ? &unit.edges : NULL, (uint32_t)unit.width,
            (uint32_t)unit.height, unit.center_x, unit.center_y, &sheet)) {
        return "the main sheet could not be built";
    }
    if (has_dying && !unit_sheet_build_dying(&source->layout,
            &source->sprite, source->has_edges ? &source->edges : NULL,
            (uint32_t)source->width,
            (uint32_t)source->height, source->center_x, source->center_y,
            &dying)) {
        unit_sheet_free(&sheet);
        return "the dying sheet could not be built";
    }

    /* The dying sheet has the unit's own palettes, unless its frames
     * come from a unit whose palettes are not the same. */
    bool other_colours = has_dying
        && (source->palette_rows != unit.palette_rows
            || memcmp(source->palettes, unit.palettes,
                unit.palette_rows * 256 * sizeof(Rgb)) != 0);
    UnitTomlPalettes palettes;
    palettes.file = "palettes.png";
    palettes.rows = unit.palette_rows;
    palettes.by_player = unit.by_player;
    palettes.dying_file = other_colours ? "dying_palettes.png"
        : "palettes.png";
    palettes.dying_rows = source->palette_rows;

    ByteBuffer toml = {0};
    char path[1040];
    snprintf(path, sizeof path, "%s/unit.toml", folder);
    if (!unit_toml_write(&toml, reg, id, &unit.layout, &sheet,
            has_dying ? &dying : NULL, &palettes)
        || !unit_toml_write_kinds(&toml, bin, id, kind_count)) {
        problem = "the unit's values could not be written out";
    } else if (!file_make_folder(folder)
        || !file_write_all(path, toml.data, toml.size)
        || !step_sheet_write(&sheet.sheet, unit.palettes, folder, "sprites.png")
        || !step_sheet_write(&sheet.edges, unit.palettes, folder, "edges.png")
        || !palettes_write(&unit, folder, "palettes.png")
        || (has_dying
            && (!step_sheet_write(&dying.sheet, source->palettes, folder,
                    "dying.png")
                || !step_sheet_write(&dying.edges, source->palettes, folder,
                    "dying_edges.png")))
        || (other_colours
            && !palettes_write(source, folder, "dying_palettes.png"))) {
        problem = "the unit's folder could not be written";
    }
    buffer_free(&toml);
    unit_sheet_free(&sheet);
    if (has_dying) {
        unit_sheet_free(&dying);
    }
    if (problem == NULL) {
        printf("  %s, %u kinds\n", name, *kind_count);
        if (unit.dying_corrected) {
            printf("    its file has %u dying phases where the registry "
                "says %u: written with %u\n",
                unit.layout.phases[UNIT_ANIM_DYING], unit.dying_said,
                unit.layout.phases[UNIT_ANIM_DYING]);
        }
    }
    return problem;
}

/* Counts the kinds in data.bin: the rows of monsters and humans that
 * are not empty. */
static uint32_t count_kinds(const DataBin *bin)
{
    static DataBinRow row;
    uint32_t count = 0;
    DataBinRows rows = databin_rows(bin, DATABIN_MONSTERS);
    while (databin_next(&rows, &row)) {
        count += row.number_count > 0 ? 1u : 0u;
    }
    rows = databin_rows(bin, DATABIN_HUMANS);
    while (databin_next(&rows, &row)) {
        count += row.number_count > 0 ? 1u : 0u;
    }
    return count;
}

int import_units(const ResArchive *graphics, const ResArchive *world,
    const char *content_folder, int32_t only)
{
    Reg reg;
    DataBin bin;
    char units_folder[1000];
    int32_t count = 0;
    if (!step_reg_open("units", graphics, "units/units.reg", &reg)
        || !step_databin_open("units", world, &bin)
        || !step_folder("units", content_folder, "units", units_folder,
            sizeof units_folder)) {
        return -1;
    }
    if (!reg_int(&reg, "Global", "UnitCount", &count)) {
        fprintf(stderr, "units: the registry does not say how many units "
            "it has\n");
        return -1;
    }

    /* The units are the sections "Unit0", "Unit1" and so on. */
    int done = 0;
    int failed = 0;
    uint32_t kinds = 0;
    printf("%s\n", units_folder);
    for (int32_t i = 0; i < count; i++) {
        char section[32];
        int32_t id = 0;
        snprintf(section, sizeof section, "Unit%d", (int)i);
        if (!reg_int(&reg, section, "ID", &id)
            || (only >= 0 && id != only)) {
            continue;
        }
        uint32_t kind_count = 0;
        const char *problem = unit_write(graphics, &reg, &bin, id,
            units_folder, &kind_count);
        if (problem == NULL) {
            done++;
            kinds += kind_count;
        } else {
            const char *name = "?";
            units_text(&reg, id, "DescText", &name);
            fprintf(stderr, "  unit %d \"%s\" left out: %s\n", (int)id,
                name, problem);
            failed++;
        }
    }
    if (only >= 0 && done + failed == 0) {
        fprintf(stderr, "units: no unit has the ID %d\n", (int)only);
        return -1;
    }
    printf("%d units written with %u kinds, %d left out\n", done, kinds,
        failed);
    if (only < 0) {
        /* A kind belongs to no folder if its unit was left out, or if
         * the registry has no unit with its number. */
        printf("%u kinds in data.bin belong to no unit that was written\n",
            count_kinds(&bin) - kinds);
    }
    return failed;
}

/* ---- The heroes ---- */

/* A hero has no unit of its own in the registry. It has a look for each
 * way of being armed, and a look is drawn as one unit is: it has that
 * unit's frames in that unit's order, in a sprite file of its own. */
typedef struct HeroLook {
    const char *folder;   /* in units/heroes and units/heroes_l */
    const char *name;     /* its folder in content/heroes */
    int32_t unit;         /* the unit it is laid out as */
} HeroLook;

static const HeroLook HERO_LOOKS[] = {
    {"unarmed", "unarmed", 1},
    {"unarmed_", "unarmed_with_shield", 2},
    {"swordsman", "swordsman", 3},
    {"swordsman_", "swordsman_with_shield", 4},
    {"swordsman2h", "two_handed_swordsman", 5},
    {"axeman", "axeman", 7},
    {"axeman_", "axeman_with_shield", 8},
    {"axeman2h", "two_handed_axeman", 9},
    {"clubman", "clubman", 10},
    {"clubman_", "clubman_with_shield", 11},
    {"pikeman", "pikeman", 12},
    {"pikeman_", "pikeman_with_shield", 13},
    {"archer", "archer", 14},
    {"xbowman", "crossbowman", 15},
    {"mage", "mage", 23},
    {"mage_st", "mage_with_staff", 24}
};
#define HERO_LOOK_COUNT (sizeof HERO_LOOKS / sizeof HERO_LOOKS[0])

/* Reads one look of the hero: in armour from units/heroes, or without
 * from units/heroes_l. Returns a message saying what went wrong, NULL
 * if all is well, and "" if the original has no such sprite, which is
 * so for the mages without armour. */
static const char *hero_read(const ResArchive *archive, const Reg *reg,
    const HeroLook *look, bool armour, Unit *out)
{
    ResEntry entry;
    memset(out, 0, sizeof *out);
    out->id = look->unit;
    out->name = look->name;
    if (!unit_layout_read(reg, look->unit, &out->layout)
        || !units_int(reg, look->unit, "Width", &out->width)
        || out->width <= 0
        || !units_int(reg, look->unit, "Height", &out->height)
        || out->height <= 0
        || !units_int(reg, look->unit, "CenterX", &out->center_x)
        || !units_int(reg, look->unit, "CenterY", &out->center_y)) {
        return "the unit it is laid out as cannot be read";
    }

    const char *set = armour ? "heroes" : "heroes_l";
    snprintf(out->path, sizeof out->path, "units/%s/%s/sprites.256", set,
        look->folder);
    if (!res_find(archive, out->path, &entry) || entry.is_folder) {
        return "";
    }
    if (!sprite256_open(archive->data + entry.offset, entry.size,
            &out->sprite)) {
        return "its sprite file cannot be read";
    }

    /* The unit may keep dying frames after its other frames, and the
     * hero's file stop before them. The layout is then the unit's
     * without its dying. */
    UnitLayout *layout = &out->layout;
    if (out->sprite.frame_count != layout->frame_count
        && layout->own_dying
        && out->sprite.frame_count == layout->first[UNIT_ANIM_DYING]) {
        layout->phases[UNIT_ANIM_DYING] = 0;
        layout->phases[UNIT_ANIM_BONES] = 0;
        layout->first[UNIT_ANIM_BONES] = layout->first[UNIT_ANIM_DYING];
        layout->first[UNIT_ANIM_IDLE] = layout->first[UNIT_ANIM_DYING];
        layout->own_dying = false;
        layout->frame_count = out->sprite.frame_count;
    }
    if (out->sprite.frame_count != layout->frame_count) {
        return "its sprite file does not have the unit's number of frames";
    }

    char edges_path[sizeof out->path + 1];
    snprintf(edges_path, sizeof edges_path, "units/%s/%s/spritesb.256",
        set, look->folder);
    out->has_edges = res_find(archive, edges_path, &entry)
        && !entry.is_folder
        && sprite256_open(archive->data + entry.offset, entry.size,
            &out->edges)
        && out->edges.frame_count == out->sprite.frame_count;

    /* The colours are the 16 a player may have. Without armour they
     * are in units/heroes_l/human.pal. units/heroes has no such file,
     * and the one the human units have is taken. */
    if (!sprite256_palette(&out->sprite, out->palettes)) {
        return "its sprite file has no palette";
    }
    out->palette_rows = 1;
    const char *table = armour ? "units/humans/human.pal"
        : "units/heroes_l/human.pal";
    if (res_find(archive, table, &entry) && !entry.is_folder) {
        const uint8_t *data = archive->data + entry.offset;
        uint32_t rows = pal_table_count(entry.size);
        if (rows == 0 || rows > MAX_PALETTES) {
            return "human.pal is not a table of palettes";
        }
        for (uint32_t i = 0; i < rows; i++) {
            pal_table_read(data, entry.size, i, out->palettes + i * 256);
        }
        out->palette_rows = rows;
        out->by_player = true;
    }
    return NULL;
}

/* Writes one look of the hero, in armour or without, into its folder.
 * The messages are as hero_read's. */
static const char *hero_write(const ResArchive *archive, const Reg *reg,
    const HeroLook *look, bool armour, const char *look_folder)
{
    /* static: too large for the stack to be at ease. */
    static Unit hero;
    const char *problem = hero_read(archive, reg, look, armour, &hero);
    if (problem != NULL) {
        return problem;
    }
    char folder[1100];
    char path[1140];
    int written = snprintf(folder, sizeof folder, "%s/%s", look_folder,
        armour ? "armour" : "light");
    if (written < 0 || (size_t)written >= sizeof folder) {
        return "the name of the content folder is too long";
    }

    bool has_dying = hero.layout.own_dying
        && (hero.layout.phases[UNIT_ANIM_DYING] > 0
            || hero.layout.phases[UNIT_ANIM_BONES] > 0);
    UnitSheet sheet;
    UnitSheet dying;
    const Sprite256 *edges = hero.has_edges ? &hero.edges : NULL;
    if (!unit_sheet_build(&hero.layout, &hero.sprite, edges,
            (uint32_t)hero.width, (uint32_t)hero.height, hero.center_x,
            hero.center_y, &sheet)) {
        return "the main sheet could not be built";
    }
    if (has_dying && !unit_sheet_build_dying(&hero.layout, &hero.sprite,
            edges, (uint32_t)hero.width, (uint32_t)hero.height,
            hero.center_x, hero.center_y, &dying)) {
        unit_sheet_free(&sheet);
        return "the dying sheet could not be built";
    }

    UnitTomlPalettes palettes;
    palettes.file = "palettes.png";
    palettes.rows = hero.palette_rows;
    palettes.by_player = hero.by_player;
    palettes.dying_file = "palettes.png";
    palettes.dying_rows = hero.palette_rows;

    ByteBuffer toml = {0};
    char comment[160];
    snprintf(comment, sizeof comment, "The hero as %s, %s. Laid out as "
        "the unit named here.", look->name,
        armour ? "in armour" : "without armour");
    bool ok = toml_comment(&toml, comment)
        && toml_text(&toml, "look", look->name)
        && toml_bool(&toml, "armour", armour)
        && toml_int(&toml, "unit", look->unit);
    if (ok && !has_dying) {
        /* The hero's file has no dying frames. This is the unit whose
         * dying frames the registry names for the unit above. */
        ok = toml_int(&toml, "dying_unit", hero.layout.dying_id);
    }
    ok = ok && unit_toml_write_sprite(&toml, reg, look->unit, &hero.layout,
        &sheet, has_dying ? &dying : NULL, &palettes);
    snprintf(path, sizeof path, "%s/hero.toml", folder);
    if (!ok) {
        problem = "its values could not be written out";
    } else if (!file_make_folder(look_folder) || !file_make_folder(folder)
        || !file_write_all(path, toml.data, toml.size)
        || !step_sheet_write(&sheet.sheet, hero.palettes, folder, "sprites.png")
        || !step_sheet_write(&sheet.edges, hero.palettes, folder, "edges.png")
        || !palettes_write(&hero, folder, "palettes.png")
        || (has_dying
            && (!step_sheet_write(&dying.sheet, hero.palettes, folder,
                    "dying.png")
                || !step_sheet_write(&dying.edges, hero.palettes, folder,
                    "dying_edges.png")))) {
        problem = "its folder could not be written";
    }
    buffer_free(&toml);
    unit_sheet_free(&sheet);
    if (has_dying) {
        unit_sheet_free(&dying);
    }
    return problem;
}

/* Writes heroes.toml: which materials of armour show the hero in
 * armour and which without. The original says it in units/material.reg,
 * as a folder for each material: "Material0" = "heroes", or
 * "heroes_l". */
static bool hero_materials_write(const ResArchive *archive,
    const char *heroes_folder)
{
    ResEntry entry;
    Reg reg;
    char path[1040];
    int32_t armour[64];
    int32_t light[64];
    size_t armour_count = 0;
    size_t light_count = 0;
    if (!res_find(archive, "units/material.reg", &entry) || entry.is_folder
        || !reg_open(archive->data + entry.offset, entry.size, &reg)) {
        return false;
    }
    /* The materials are numbered from 0 with no gaps. */
    for (int32_t material = 0; material < 64; material++) {
        char section[32];
        const char *folder = NULL;
        snprintf(section, sizeof section, "Material%d", (int)material);
        if (!reg_text(&reg, section, "Path", &folder)) {
            break;
        }
        if (strcmp(folder, "heroes") == 0) {
            armour[armour_count++] = material;
        } else if (strcmp(folder, "heroes_l") == 0) {
            light[light_count++] = material;
        } else {
            return false;
        }
    }
    if (armour_count + light_count == 0) {
        return false;
    }
    ByteBuffer toml = {0};
    snprintf(path, sizeof path, "%s/heroes.toml", heroes_folder);
    bool ok = toml_comment(&toml, "Which look of the hero the material of "
            "the armour it wears picks. The numbers are those of "
            "items/materials.toml.")
        && toml_int_list(&toml, "armour_materials", armour, armour_count)
        && toml_int_list(&toml, "light_materials", light, light_count)
        && file_write_all(path, toml.data, toml.size);
    buffer_free(&toml);
    return ok;
}

int import_heroes(const ResArchive *graphics, const char *content_folder)
{
    Reg reg;
    char heroes_folder[1000];
    if (!step_reg_open("heroes", graphics, "units/units.reg", &reg)
        || !step_folder("heroes", content_folder, "heroes", heroes_folder,
            sizeof heroes_folder)) {
        return -1;
    }

    int done = 0;
    int failed = 0;
    printf("%s\n", heroes_folder);
    for (size_t i = 0; i < HERO_LOOK_COUNT; i++) {
        const HeroLook *look = &HERO_LOOKS[i];
        char folder[1060];
        snprintf(folder, sizeof folder, "%s/%s", heroes_folder, look->name);
        int here = 0;
        for (int armour = 1; armour >= 0; armour--) {
            const char *problem = hero_write(graphics, &reg, look,
                armour != 0, folder);
            if (problem == NULL) {
                here++;
            } else if (problem[0] != '\0') {
                fprintf(stderr, "  hero %s %s left out: %s\n", look->name,
                    armour != 0 ? "in armour" : "without armour", problem);
                failed++;
            }
        }
        printf("  %s, %d\n", look->name, here);
        done += here;
    }
    if (!hero_materials_write(graphics, heroes_folder)) {
        fprintf(stderr, "  heroes.toml left out: units/material.reg could "
            "not be read or written\n");
        failed++;
    }
    printf("%d looks of the hero written, %d left out\n", done, failed);
    return failed;
}
