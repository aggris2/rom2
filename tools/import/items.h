/* Reading items from the tables of data.bin.
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
#ifndef ROM2_IMPORT_ITEMS_H
#define ROM2_IMPORT_ITEMS_H

#include <stdbool.h>
#include <stdint.h>

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

#endif
