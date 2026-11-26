/* import_items: writes the items of the original game as ROM2 content.
 *
 *     import_items <graphics.res> <world.res> <content folder>
 *
 * In <content folder>/items this makes:
 *
 *     weapons/, shields/, armour/
 *         a folder for every shape, named after its place in its table
 *         and its name, with item.toml and a folder for each of its
 *         variants, such as iron_common, with the variant's picture for
 *         the inventory: icon.png and icon_alpha.png
 *     magic/
 *         a folder for every magic item, with item.toml, icon.png and
 *         icon_alpha.png
 *     materials.toml, classes.toml, modifiers.toml
 *         the tables a shape's numbers are multiplied by, and the
 *         changes magic can make
 *
 * The numbers come from data.bin in world.res, the pictures from
 * graphics.res.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/databin.h"
#include "import/icon_strip.h"
#include "import/item_toml.h"
#include "import/items.h"
#include "import/name.h"
#include "import/png.h"
#include "import/res.h"
#include "import/sprite256.h"

/* The size of a picture for the inventory. */
#define ICON_SIDE 80

/* Writes the bytes of a buffer as the file `name` in a folder. */
static bool write_in(const char *folder, const char *name,
    const ByteBuffer *bytes)
{
    char path[1300];
    int written = snprintf(path, sizeof path, "%s/%s", folder, name);
    return written > 0 && (size_t)written < sizeof path
        && file_write_all(path, bytes->data, bytes->size);
}

/* Makes the folder `parent`/`name` and writes its path to `out`. */
static bool make_folder(const char *parent, const char *name, char *out,
    size_t size)
{
    int written = snprintf(out, size, "%s/%s", parent, name);
    return written > 0 && (size_t)written < size && file_make_folder(out);
}

/* Writes the inventory picture of one item as two files in a folder:
 * icon.png, with the picture's colours as numbers and its own palette,
 * and icon_alpha.png, with how solid each pixel is. */
static bool icon_write(const ResArchive *archive, int32_t material,
    int32_t slot, int32_t item_class, int32_t place, const char *folder)
{
    char item[ITEM_FILE_NAME_SIZE];
    char path[64];
    ResEntry entry;
    Sprite256 icon;
    IconStrip strip;
    if (!items_file_name(material, slot, item_class, place, item)) {
        return false;
    }
    snprintf(path, sizeof path, "inventory/%s.16a", item);
    if (!res_find(archive, path, &entry) || entry.is_folder
        || !sprite256_open(archive->data + entry.offset, entry.size, &icon)
        || !icon_strip_create(&strip, ICON_SIDE, ICON_SIDE, 1)) {
        return false;
    }
    ByteBuffer png = {0};
    bool ok = icon_strip_put(&strip, 0, &icon);
    ok = ok && png_write_indexed(&png, ICON_SIDE, ICON_SIDE, strip.pixels,
            strip.palettes)
        && write_in(folder, "icon.png", &png);
    png.size = 0;
    ok = ok && png_write_grey(&png, ICON_SIDE, ICON_SIDE, strip.solid)
        && write_in(folder, "icon_alpha.png", &png);
    buffer_free(&png);
    icon_strip_free(&strip);
    return ok;
}

/* Writes the folders of the shapes of one table. Returns how many could
 * not be written. `*done` and `*icons` count shapes and pictures. */
static int shapes_write(const ResArchive *archive, const DataBin *bin,
    DataBinTable table, const char *items_folder, const char *kind_folder,
    int *done, int *icons)
{
    static DataBinRow row;
    char parent[1024];
    int failed = 0;
    if (!make_folder(items_folder, kind_folder, parent, sizeof parent)) {
        return 1;
    }
    DataBinRows rows = databin_rows(bin, table);
    int32_t place = 0;
    while (databin_next(&rows, &row)) {
        place++;
        int32_t slot = -1;
        /* A row without a slot is a gap in the table. */
        if (!items_slot(bin, table, &row, &slot) || slot < 0) {
            continue;
        }

        ItemVariant variants[ITEM_MAX_VARIANTS];
        uint32_t count = items_variants(&row, variants);
        char name[NAME_FOLDER_SIZE];
        char folder[1100];
        ByteBuffer toml = {0};
        bool ok = name_folder(place, row.name, name)
            && make_folder(parent, name, folder, sizeof folder)
            && item_toml_write_shape(&toml, bin, table, &row, place,
                ICON_SIDE, ICON_SIDE)
            && write_in(folder, "item.toml", &toml);
        buffer_free(&toml);

        /* A folder for each variant, named after its material and its
         * class, with its picture in it. */
        for (uint32_t i = 0; ok && i < count; i++) {
            char variant[ITEM_TOML_VARIANT_NAME_SIZE];
            char variant_folder[1200];
            ok = item_toml_variant_name(bin, variants[i].material,
                    variants[i].item_class, variant)
                && make_folder(folder, variant, variant_folder,
                    sizeof variant_folder)
                && icon_write(archive, variants[i].material, slot,
                    variants[i].item_class, place, variant_folder);
        }
        if (ok) {
            printf("  %s/%s, %u variants\n", kind_folder, name, count);
            (*done)++;
            *icons += (int)count;
        } else {
            fprintf(stderr, "  %s %d \"%s\" left out: its pictures or its "
                "folder could not be made\n", kind_folder, (int)place,
                row.name);
            failed++;
        }
    }
    return failed;
}

/* Says whether the archive has a picture for a magic item. */
static bool magic_has_icon(const ResArchive *archive, int32_t place)
{
    char item[ITEM_FILE_NAME_SIZE];
    char path[64];
    ResEntry entry;
    if (!items_file_name(0, ITEM_SLOT_MAGIC, 0, place, item)) {
        return false;
    }
    snprintf(path, sizeof path, "inventory/%s.16a", item);
    return res_find(archive, path, &entry) && !entry.is_folder;
}

/* Writes the folders of the magic items. */
static int magic_write(const ResArchive *archive, const DataBin *bin,
    const char *items_folder, int *done, int *icons)
{
    static DataBinRow row;
    char parent[1024];
    int failed = 0;
    if (!make_folder(items_folder, "magic", parent, sizeof parent)) {
        return 1;
    }
    DataBinRows rows = databin_rows(bin, DATABIN_MAGIC_ITEMS);
    int32_t place = 0;
    while (databin_next(&rows, &row)) {
        place++;
        /* Its picture is named as an item of no material and no class
         * in the slot of the magic items. One without is still an
         * item. */
        bool has_icon = magic_has_icon(archive, place);
        uint32_t side = has_icon ? ICON_SIDE : 0;

        char name[NAME_FOLDER_SIZE];
        char folder[1100];
        ByteBuffer toml = {0};
        bool ok = name_folder(place, row.name, name)
            && make_folder(parent, name, folder, sizeof folder)
            && item_toml_write_magic(&toml, &row, place, side, side)
            && write_in(folder, "item.toml", &toml)
            && (!has_icon || icon_write(archive, 0, ITEM_SLOT_MAGIC, 0,
                place, folder));
        buffer_free(&toml);
        if (ok) {
            (*done)++;
            *icons += has_icon ? 1 : 0;
        } else {
            fprintf(stderr, "  magic item %d \"%s\" left out\n", (int)place,
                row.name);
            failed++;
        }
    }
    return failed;
}

/* Writes one of the three tables as a file in the items folder. */
static bool table_write(const char *items_folder, const char *name,
    const DataBin *bin, bool (*write)(ByteBuffer *, const DataBin *))
{
    ByteBuffer toml = {0};
    bool ok = write(&toml, bin) && write_in(items_folder, name, &toml);
    buffer_free(&toml);
    return ok;
}

/* Does the work once the archives are in memory. Returns how many
 * things could not be written, or -1 if nothing could be done. */
static int import_items(const uint8_t *data, size_t size,
    const uint8_t *world_data, size_t world_size,
    const char *content_folder)
{
    ResArchive archive;
    ResArchive world;
    ResEntry entry;
    DataBin bin;
    if (!res_open(data, size, &archive)) {
        fprintf(stderr, "import_items: the first file is not a .res "
            "archive\n");
        return -1;
    }
    if (!res_open(world_data, world_size, &world)
        || !res_find(&world, "data/data.bin", &entry) || entry.is_folder
        || !databin_open(world.data + entry.offset, entry.size, &bin)) {
        fprintf(stderr, "import_items: no usable data/data.bin in the "
            "second archive\n");
        return -1;
    }

    char items_folder[1000];
    if (!file_make_folder(content_folder)
        || !make_folder(content_folder, "items", items_folder,
            sizeof items_folder)) {
        fprintf(stderr, "import_items: cannot make the folder %s/items\n",
            content_folder);
        return -1;
    }

    int shapes = 0;
    int magic = 0;
    int icons = 0;
    int failed = 0;
    printf("%s\n", items_folder);
    failed += shapes_write(&archive, &bin, DATABIN_WEAPONS, items_folder,
        "weapons", &shapes, &icons);
    failed += shapes_write(&archive, &bin, DATABIN_SHIELDS, items_folder,
        "shields", &shapes, &icons);
    failed += shapes_write(&archive, &bin, DATABIN_ARMOUR, items_folder,
        "armour", &shapes, &icons);
    failed += magic_write(&archive, &bin, items_folder, &magic, &icons);
    if (!table_write(items_folder, "materials.toml", &bin,
            item_toml_write_materials)
        || !table_write(items_folder, "classes.toml", &bin,
            item_toml_write_classes)
        || !table_write(items_folder, "modifiers.toml", &bin,
            item_toml_write_modifiers)) {
        fprintf(stderr, "  the three tables could not be written\n");
        failed++;
    }
    printf("%d shapes and %d magic items written with %d pictures, %d "
        "left out\n", shapes, magic, icons, failed);
    return failed;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: import_items <graphics.res> <world.res> "
            "<content folder>\n");
        return 2;
    }

    uint8_t *data = NULL;
    uint8_t *world = NULL;
    size_t size = 0;
    size_t world_size = 0;
    if (!file_read_all(argv[1], &data, &size)) {
        fprintf(stderr, "import_items: cannot read %s\n", argv[1]);
        return 1;
    }
    if (!file_read_all(argv[2], &world, &world_size)) {
        fprintf(stderr, "import_items: cannot read %s\n", argv[2]);
        free(data);
        return 1;
    }
    int failed = import_items(data, size, world, world_size, argv[3]);
    free(world);
    free(data);
    return failed < 0 ? 1 : 0;
}
