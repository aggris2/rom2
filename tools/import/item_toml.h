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
 *     variant_materials = [0, 1, 1, 2]
 *     variant_classes = [0, 0, 1, 1]
 *     variant_folders = ["iron_common", "bronze_common",
 *         "bronze_uncommon", "steel_uncommon"]
 *
 *     [icon]
 *     sheet = "icon.png"
 *     alpha = "icon_alpha.png"
 *     size = [80, 80]
 *
 * `id` is the shape's place in its table, counted from 1, and `kind`
 * says which table: "weapon", "shield" or "armour". The numbers are the
 * table's columns under ROM2's names.
 *
 * (The real file has each list on one line.)
 *
 * The three `variant` lists go together: entry `n` of each is the
 * material and the class of the shape's variant `n`, as numbers of the
 * rows of materials.toml and classes.toml, and the name of the folder
 * with that variant's pictures. The name is the material's and the
 * class's. Each variant has pictures of its own, because the original
 * draws each one anew, with its own colours.
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

/* Adds the text of item.toml for a shape to `b`. `table` is
 * DATABIN_WEAPONS, DATABIN_SHIELDS or DATABIN_ARMOUR, `row` the shape's
 * row and `place` its place in the table, counted from 1. With
 * `icon_width` and `icon_height` not 0 the file names the inventory
 * picture of the variants, which is of that size.
 *
 * Returns false if `table` is not one of the three, a variant's
 * material or class is not in the tables, or memory runs out. `b` may
 * then hold part of the text. */
bool item_toml_write_shape(ByteBuffer *b, const DataBin *bin,
    DataBinTable table, const DataBinRow *row, int32_t place,
    uint32_t icon_width, uint32_t icon_height);

/* Adds the text of item.toml for a magic item to `b`. With `icon_width`
 * and `icon_height` not 0 the file names its picture, of that size. */
bool item_toml_write_magic(ByteBuffer *b, const DataBinRow *row,
    int32_t place, uint32_t icon_width, uint32_t icon_height);

/* Adds the text of materials.toml, classes.toml or modifiers.toml to
 * `b`: an entry for every row of that table of data.bin.
 *
 * Returns false if memory runs out or a number is not finite. */
bool item_toml_write_materials(ByteBuffer *b, const DataBin *bin);
bool item_toml_write_classes(ByteBuffer *b, const DataBin *bin);
bool item_toml_write_modifiers(ByteBuffer *b, const DataBin *bin);

#endif
