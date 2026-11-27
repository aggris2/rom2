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
#ifndef ROM2_IMPORT_ITEM_TOML_H
#define ROM2_IMPORT_ITEM_TOML_H

#include <stdbool.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/databin.h"
#include "import/item_names.h"

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
