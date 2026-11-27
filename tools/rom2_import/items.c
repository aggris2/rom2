/* The items step of rom2_import: writes the items of the original game as
 * ROM2 content.
 *
 * In <content folder>/items this makes:
 *
 *     weapons/, shields/, armour/
 *         a folder for every shape, named after its place in its table
 *         and its name, with item.toml and a folder for each of its
 *         variants, such as iron_common, with the variant's picture for
 *         the inventory: icon.png and icon_alpha.png
 *         and a folder `worn` with its pictures as worn, such as
 *         fighter_man.png
 *     magic/
 *         a folder for every magic item, with item.toml, icon.png and
 *         icon_alpha.png
 *     materials.toml, classes.toml, modifiers.toml
 *         the tables a shape's numbers are multiplied by, and the
 *         changes magic can make
 *
 * and in <content folder>/characters a folder for each kind of character,
 * with the pictures of the characters the items are drawn on.
 *
 * The numbers come from data.bin in world.res, the pictures from
 * graphics.res.
 */
#include "rom2_import/steps.h"

#include <stdio.h>
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
    char path[1400];
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

/* The kinds of character an item is drawn on: the names of their folders
 * in the original, and the names ROM2 gives them, in the same order. And
 * the size of a character. */
static const char *const CHARACTER_FOLDERS[] = {
    "mfighter", "ffighter", "mmage", "fmage"
};
static const char *const CHARACTER_NAMES[] = {
    "fighter_man", "fighter_woman", "mage_man", "mage_woman"
};
#define CHARACTER_COUNT (sizeof CHARACTER_FOLDERS / sizeof CHARACTER_FOLDERS[0])
#define CHARACTER_WIDTH 160
#define CHARACTER_HEIGHT 240

/* Says whether the archive has a file at `path`. */
static bool has_file(const ResArchive *archive, const char *path)
{
    ResEntry entry;
    return res_find(archive, path, &entry) && !entry.is_folder;
}

/* Writes the first frame of the .256 file at `path` as the 8-bit indexed
 * PNG `name` in a folder, with the file's own palette. A file that is
 * not there, or is empty, writes nothing: `*written` says whether a
 * picture was written. Returns false only if a file that is there
 * cannot be read or written. */
static bool picture_write(const ResArchive *archive, const char *path,
    const char *folder, const char *name, bool *written)
{
    ResEntry entry;
    Sprite256 sprite;
    Rgb palette[256];
    Frame256 frame;
    *written = false;
    if (!res_find(archive, path, &entry) || entry.is_folder
        || entry.size == 0) {
        return true;
    }
    if (!sprite256_open(archive->data + entry.offset, entry.size, &sprite)
        || sprite.frame_count == 0
        || !sprite256_palette(&sprite, palette)) {
        return false;
    }
    Reader r = sprite256_frames(&sprite);
    if (!sprite256_frame_read(&r, &frame)) {
        return false;
    }
    ByteBuffer png = {0};
    bool ok = png_write_indexed(&png, frame.width, frame.height,
            frame.pixels, palette)
        && write_in(folder, name, &png);
    buffer_free(&png);
    frame256_free(&frame);
    *written = ok;
    return ok;
}

/* Writes the pictures of one item as worn, for every kind of character
 * that has one, into the folder `worn` of the variant's folder: a
 * picture named after the character, and one with "_second" for the item's
 * second part. `*count` counts them. */
static bool worn_write(const ResArchive *archive, const char *item,
    const char *variant_folder, int *count)
{
    char folder[1300];
    bool made = false;
    for (size_t i = 0; i < CHARACTER_COUNT; i++) {
        char first[96];
        char second[96];
        char name[48];
        bool written = false;
        snprintf(first, sizeof first, "equipment/%s/primary/%s.256",
            CHARACTER_FOLDERS[i], item);
        snprintf(second, sizeof second, "equipment/%s/secondary/%s.256",
            CHARACTER_FOLDERS[i], item);
        /* The folder is made when the first picture turns up. */
        if (!made && (has_file(archive, first)
                || has_file(archive, second))) {
            if (!make_folder(variant_folder, "worn", folder,
                    sizeof folder)) {
                return false;
            }
            made = true;
        }
        snprintf(name, sizeof name, "%s.png", CHARACTER_NAMES[i]);
        if (!picture_write(archive, first, folder, name, &written)) {
            return false;
        }
        *count += written ? 1 : 0;
        snprintf(name, sizeof name, "%s_second.png", CHARACTER_NAMES[i]);
        if (!picture_write(archive, second, folder, name, &written)) {
            return false;
        }
        *count += written ? 1 : 0;
    }
    return true;
}

/* Finds which kinds of character an item is drawn on, and whether it
 * has a second picture, by the files the archive has for it. */
static void worn_find(const ResArchive *archive, const char *item,
    const char *characters[CHARACTER_COUNT], ItemTomlWorn *out)
{
    out->characters = characters;
    out->character_count = 0;
    out->has_second = false;
    out->width = CHARACTER_WIDTH;
    out->height = CHARACTER_HEIGHT;
    for (size_t i = 0; i < CHARACTER_COUNT; i++) {
        char path[96];
        snprintf(path, sizeof path, "equipment/%s/primary/%s.256",
            CHARACTER_FOLDERS[i], item);
        if (has_file(archive, path)) {
            characters[out->character_count++] = CHARACTER_NAMES[i];
        }
        snprintf(path, sizeof path, "equipment/%s/secondary/%s.256",
            CHARACTER_FOLDERS[i], item);
        if (has_file(archive, path)) {
            out->has_second = true;
        }
    }
}

/* Writes the characters themselves, without anything on, into
 * <content folder>/characters/<kind>: 001.png, 002.png and so on, for
 * as long as the archive has the next number. Returns how many. */
static int characters_write(const ResArchive *archive,
    const char *content_folder)
{
    char parent[1024];
    int count = 0;
    if (!make_folder(content_folder, "characters", parent,
            sizeof parent)) {
        return 0;
    }
    for (size_t i = 0; i < CHARACTER_COUNT; i++) {
        char folder[1100];
        if (!make_folder(parent, CHARACTER_NAMES[i], folder,
                sizeof folder)) {
            continue;
        }
        for (int number = 1; number <= 999; number++) {
            char path[96];
            char name[16];
            bool written = false;
            snprintf(path, sizeof path, "equipment/%s/%d.256",
                CHARACTER_FOLDERS[i], number);
            snprintf(name, sizeof name, "%03d.png", number);
            if (!has_file(archive, path)
                || !picture_write(archive, path, folder, name, &written)
                || !written) {
                break;
            }
            count++;
        }
    }
    return count;
}

/* Writes the folders of the shapes of one table. Returns how many could
 * not be written. `*done` and `*icons` count shapes and pictures. */
static int shapes_write(const ResArchive *archive, const DataBin *bin,
    DataBinTable table, const char *items_folder, const char *kind_folder,
    const ItemNames *names, int *done, int *icons, int *worn_count)
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
        /* Which characters the shape is drawn on is the same for all its
         * variants, so the first one tells. */
        const char *characters[CHARACTER_COUNT];
        ItemTomlWorn worn;
        char item[ITEM_FILE_NAME_SIZE] = "";
        if (count > 0) {
            items_file_name(variants[0].material, slot,
                variants[0].item_class, place, item);
        }
        worn_find(archive, item, characters, &worn);

        char name[NAME_FOLDER_SIZE];
        char folder[1100];
        ByteBuffer toml = {0};
        bool ok = name_folder(place, row.name, name)
            && make_folder(parent, name, folder, sizeof folder)
            && item_toml_write_shape_named(&toml, bin, table, &row, place,
                ICON_SIDE, ICON_SIDE, &worn, names)
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
                    variants[i].item_class, place, variant_folder)
                && items_file_name(variants[i].material, slot,
                    variants[i].item_class, place, item)
                && worn_write(archive, item, variant_folder, worn_count);
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
    const char *items_folder, const ItemNames *names, int *done, int *icons)
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
            && item_toml_write_magic_named(&toml, &row, place, side, side,
                names)
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

int import_items(const ResArchive *graphics, const ResArchive *world,
    const ResArchive *main_archive, const char *content_folder)
{
    ResEntry entry;
    DataBin bin;
    if (!res_find(world, "data/data.bin", &entry) || entry.is_folder
        || !databin_open(world->data + entry.offset, entry.size, &bin)) {
        fprintf(stderr, "items: no usable data/data.bin in "
            "world.res\n");
        return -1;
    }

    char items_folder[1000];
    if (!file_make_folder(content_folder)
        || !make_folder(content_folder, "items", items_folder,
            sizeof items_folder)) {
        fprintf(stderr, "items: cannot make the folder %s/items\n",
            content_folder);
        return -1;
    }

    int shapes = 0;
    int magic = 0;
    int icons = 0;
    int worn = 0;
    int failed = 0;
    printf("%s\n", items_folder);
    /* The names the game shows, from three files in two archives. With
     * one of them missing the items are written without. */
    ItemNames names_files;
    const ItemNames *names = NULL;
    ResEntry numbers;
    ResEntry server;
    ResEntry shown;
    if (main_archive != NULL
        && res_find(world, "data/itemname.bin", &numbers)
        && !numbers.is_folder
        && res_find(main_archive, "text/itemserv.txt", &server)
        && !server.is_folder
        && res_find(main_archive, "text/itemname.txt", &shown)
        && !shown.is_folder
        && item_names_open(world->data + numbers.offset, numbers.size,
            main_archive->data + server.offset, server.size,
            main_archive->data + shown.offset, shown.size,
            &names_files)) {
        names = &names_files;
    } else {
        fprintf(stderr, "  the names of the items could not be read: "
            "they are written without\n");
    }

    failed += shapes_write(graphics, &bin, DATABIN_WEAPONS, items_folder,
        "weapons", names, &shapes, &icons, &worn);
    failed += shapes_write(graphics, &bin, DATABIN_SHIELDS, items_folder,
        "shields", names, &shapes, &icons, &worn);
    failed += shapes_write(graphics, &bin, DATABIN_ARMOUR, items_folder,
        "armour", names, &shapes, &icons, &worn);
    failed += magic_write(graphics, &bin, items_folder, names, &magic,
        &icons);
    if (!table_write(items_folder, "materials.toml", &bin,
            item_toml_write_materials)
        || !table_write(items_folder, "classes.toml", &bin,
            item_toml_write_classes)
        || !table_write(items_folder, "modifiers.toml", &bin,
            item_toml_write_modifiers)) {
        fprintf(stderr, "  the three tables could not be written\n");
        failed++;
    }
    int characters = characters_write(graphics, content_folder);
    printf("%d shapes and %d magic items written with %d pictures for the "
        "inventory and %d of items worn, %d left out\n", shapes, magic,
        icons, worn, failed);
    printf("%d characters written to %s/characters\n", characters,
        content_folder);
    return failed;
}
