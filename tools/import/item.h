/* Everything about items: reading them from the tables of data.bin,
 * finding the names the game shows for them, and writing item.toml and
 * the tables of materials, classes and modifiers. The parts follow in
 * that order.
 *
 * The original does not keep a row for every item. An item is a shape,
 * such as a dagger, made of a material, such as iron, in a class, such
 * as common, and its numbers are the shape's numbers multiplied by the
 * material's and the class's. The shapes are the rows of three tables:
 * weapons, shields and armour.
 *
 * Not every material goes with every class. A shape's row ends in 8
 * short numbers, one for each class, and in each of them bit `m` is set
 * if the shape exists in that class in material `m`. One such pair of
 * material and class is a variant of the shape.
 *
 * The pictures of an item are in files named after it: two digits for
 * the material, two for the slot it is worn in, one for the class, and
 * two for the shape's place in its table, counted from 1. An iron common
 * dagger, in slot 1 and second among the weapons, is "0001002".
 *
 * See docs/formats/items.md.
 */
#ifndef ROM2_IMPORT_ITEM_H
#define ROM2_IMPORT_ITEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/databin.h"

#define ITEM_CLASSES 8u
#define ITEM_MATERIALS 16u
/* The most variants a shape can have. */
#define ITEM_MAX_VARIANTS (ITEM_CLASSES * ITEM_MATERIALS)
/* The slot of the magic items, which are not in the three tables. */
#define ITEM_SLOT_MAGIC 14

/* A material and a class a shape exists in. */
typedef struct ItemVariant {
    uint8_t material;
    uint8_t item_class;
} ItemVariant;

/* The room an item's file name needs, with its closing zero. */
#define ITEM_FILE_NAME_SIZE 8

/* Lists the variants of a shape from its row, class by class and in each
 * class material by material. Returns how many there are, which is 0 for
 * a row without the 8 short numbers. */
uint32_t items_variants(const DataBinRow *row,
    ItemVariant out[ITEM_MAX_VARIANTS]);

/* Reads the slot a shape is worn in, the column "Slot" of its table.
 * Returns false, and leaves `*out` alone, if the table has no such
 * column or the row no such number. */
bool items_slot(const DataBin *bin, DataBinTable table,
    const DataBinRow *row, int32_t *out);

/* Writes the name of an item's files, such as "0001002". `place` is the
 * shape's place in its table, counted from 1.
 *
 * Returns false, and leaves `out` alone, if a number does not fit its
 * digits: a material or a slot over 99, a class over 9, a place over
 * 99, or any of them negative. */
bool items_file_name(int32_t material, int32_t slot, int32_t item_class,
    int32_t place, char out[ITEM_FILE_NAME_SIZE]);

/* The names the original game shows for its items.
 *
 * Three files give them. data/itemname.bin in world.res is a number of
 * 16 bits for each item, low byte first: the item's material in its top
 * 4 bits, then 4 bits of its slot, 3 of its class and 5 of its place in
 * its table. text/itemserv.txt and text/itemname.txt in main.res have a
 * line for each of those numbers, in the same order: the first the name
 * the server knows the item by, such as "Common Bronze Bracers" or
 * "Book Air", the second the name the player sees, "Bronze Bracers" or
 * "Book".
 */
/* The room a name needs, with its closing zero. */
#define ITEM_NAME_SIZE 192

/* The three files, open. Does not own their bytes. */
typedef struct ItemNames {
    const uint8_t *numbers;
    uint32_t count;
    const uint8_t *server;
    size_t server_size;
    const uint8_t *shown;
    size_t shown_size;
} ItemNames;

/* Opens the three files held in memory: the numbers, the names for the
 * server and the names shown.
 *
 * Returns false, and leaves `*out` alone, if the numbers are not a whole
 * count of 16 bits each, or a text has fewer lines than there are
 * numbers. */
bool item_names_open(const uint8_t *numbers, size_t numbers_size,
    const uint8_t *server, size_t server_size, const uint8_t *shown,
    size_t shown_size, ItemNames *out);

/* Writes the name shown for the item with this material, slot, class
 * and place to `out`, as UTF-8.
 *
 * Returns false, and leaves `out` alone, if a number does not fit its
 * bits, no item has that number, or its name does not fit. */
bool item_names_shown(const ItemNames *names, int32_t material,
    int32_t slot, int32_t item_class, int32_t place,
    char out[ITEM_NAME_SIZE]);

/* Writes the name shown for the item the server knows as `server_name`
 * to `out`, as UTF-8. The magic items are found so: their names in
 * data.bin are the server's.
 *
 * Returns false, and leaves `out` alone, if the server knows no item by
 * that name, or the name shown does not fit. */
bool item_names_shown_for(const ItemNames *names, const char *server_name,
    char out[ITEM_NAME_SIZE]);

/* Writing the definitions of items.
 *
 * A shape, a row of data.bin's weapons, shields or armour, gets a file
 * item.toml in a folder of its own:
 *
 *     # Dagger
 *     id = 2
 *     name = "Dagger"
 *     kind = "weapon"
 *     price = 100
 *     weight = 5
 *     slot = 1
 *     attack_type = 1
 *     damage_min = 18
 *     damage_max = 27
 *
 *     [icon]
 *     sheet = "icon.png"
 *     alpha = "icon_alpha.png"
 *     size = [80, 80]
 *
 *     [[variant]]
 *     folder = "iron_common"
 *     material = 0
 *     class = 0
 *
 *     [[variant]]
 *     folder = "bronze_common"
 *     material = 1
 *     class = 0
 *
 * `id` is the shape's place in its table, counted from 1, and `kind`
 * says which table: "weapon", "shield" or "armour". The numbers are the
 * table's columns under ROM2's names.
 *
 * Each `[[variant]]` is one material and class the shape exists in, as
 * numbers of the rows of materials.toml and classes.toml, and the
 * folder with that variant's pictures, named after the two. Each
 * variant has pictures of its own, because the original draws each one
 * anew, with its own colours. A shape without variants, which only
 * monsters use, has none of this.
 *
 * `[icon]` names the files that every variant's folder has for its
 * picture in the inventory:
 *
 *     iron_common/icon.png         the picture, an 8-bit indexed PNG
 *                                  with its own palette
 *     iron_common/icon_alpha.png   how solid each pixel is, in shades
 *                                  of grey, as for a projectile
 *
 * A pixel is there if the second picture says so, whatever its colour
 * number is.
 *
 * A shape that can be worn also has `[worn]`:
 *
 *     [worn]
 *     characters = ["fighter_man", "fighter_woman"]
 *     second = true
 *     size = [160, 240]
 *
 * `characters` are the kinds of character the shape is drawn on in the
 * inventory: a man or a woman, a fighter or a mage. For each of them
 * a variant's folder has a folder called `worn`, with a picture of the
 * item as worn, named after the character:
 *
 *     iron_common/worn/fighter_man.png          an 8-bit indexed PNG with
 *                                            its own palette
 *     iron_common/worn/fighter_man_second.png   the item's second part
 *
 * A picture is of the size of the whole character, with the item at its
 * place and nothing else. A shape with `second = true` has the second
 * picture for each of its characters: the other hand's bracer or glove, the
 * other hand's ring, the shoulders of a cloak. A variant whose picture
 * is empty in the original has no file.
 *
 * A magic item, a row of data.bin's magic items, is a thing of its own
 * with one picture, in the same two files, in its own folder:
 *
 *     # Scroll Fire Arrow
 *     id = 6
 *     name = "Scroll Fire Arrow"
 *     kind = "magic"
 *     price = 100
 *     weight = 1
 *     effects = "castSpell=Fire_Arrow:50"
 *
 *     [icon]
 *     sheet = "icon.png"
 *     alpha = "icon_alpha.png"
 *     size = [80, 80]
 *
 * The materials, the classes and the modifiers are tables without
 * pictures, each written as one file of entries:
 *
 *     [[material]]
 *     id = 1
 *     name = "Bronze"
 *     price = 0.25
 *     weight = 0.5
 *     damage = 0.694
 *
 * A material's and a class's numbers are what a shape's are multiplied
 * by. Their `id` counts from 0, as the variant lists and the names of
 * the original's picture files do.
 */
/* The room the name of a variant's folder needs, with its closing
 * zero. */
#define ITEM_TOML_VARIANT_NAME_SIZE 64

/* Makes the name of a variant's folder from the names of its material
 * and its class, as name_label() writes them, with '_' between:
 * "Magic Wood" and "Very Rare" give "magic_wood_very_rare". The names
 * are those of the rows of data.bin's materials and classes.
 *
 * Returns false, and leaves `out` alone, if the tables have no such
 * rows. */
bool item_toml_variant_name(const DataBin *bin, uint32_t material,
    uint32_t item_class, char out[ITEM_TOML_VARIANT_NAME_SIZE]);

/* The kinds of character a shape is drawn on. */
typedef struct ItemTomlWorn {
    const char *const *characters;
    uint32_t character_count;
    /* True if the shape has a second picture for each of them. */
    bool has_second;
    /* The size of a picture, which is that of the whole character. */
    uint32_t width;
    uint32_t height;
} ItemTomlWorn;

/* Adds the text of item.toml for a shape to `b`. `table` is
 * DATABIN_WEAPONS, DATABIN_SHIELDS or DATABIN_ARMOUR, `row` the shape's
 * row and `place` its place in the table, counted from 1. With
 * `icon_width` and `icon_height` not 0 the file names the inventory
 * picture of the variants, which is of that size. `worn` says which
 * characters the shape is drawn on, or is NULL for a shape that is not
 * drawn on any.
 *
 * Returns false if `table` is not one of the three, a variant's
 * material or class is not in the tables, or memory runs out. `b` may
 * then hold part of the text. */
bool item_toml_write_shape(ByteBuffer *b, const DataBin *bin,
    DataBinTable table, const DataBinRow *row, int32_t place,
    uint32_t icon_width, uint32_t icon_height, const ItemTomlWorn *worn);

/* The same, with the name the game shows for each variant as its
 * "display_name", where `names` has one. `names` may be NULL. */
bool item_toml_write_shape_named(ByteBuffer *b, const DataBin *bin,
    DataBinTable table, const DataBinRow *row, int32_t place,
    uint32_t icon_width, uint32_t icon_height, const ItemTomlWorn *worn,
    const ItemNames *names);

/* Adds the text of item.toml for a magic item to `b`. With `icon_width`
 * and `icon_height` not 0 the file names its picture, of that size. */
bool item_toml_write_magic(ByteBuffer *b, const DataBinRow *row,
    int32_t place, uint32_t icon_width, uint32_t icon_height);

/* The same, with the name the game shows for the item as its
 * "display_name", where `names` has one. `names` may be NULL. */
bool item_toml_write_magic_named(ByteBuffer *b, const DataBinRow *row,
    int32_t place, uint32_t icon_width, uint32_t icon_height,
    const ItemNames *names);

/* Adds the text of materials.toml, classes.toml or modifiers.toml to
 * `b`: an entry for every row of that table of data.bin.
 *
 * Returns false if memory runs out or a number is not finite. */
bool item_toml_write_materials(ByteBuffer *b, const DataBin *bin);
bool item_toml_write_classes(ByteBuffer *b, const DataBin *bin);
bool item_toml_write_modifiers(ByteBuffer *b, const DataBin *bin);

#endif
