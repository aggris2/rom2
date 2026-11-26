/* Reading data.bin, the tables of the original game's rules.
 *
 * The file is a spreadsheet saved in binary: eleven tables one after the
 * other, each with the names of its columns and then its rows. A row has
 * a name, and after it numbers, and in some tables texts. The layout is
 * described in docs/formats/databin.md.
 *
 * To read a table, ask for its rows and take them one at a time:
 *
 *     DataBin bin;
 *     DataBinRow row;
 *     if (databin_open(data, size, &bin)) {
 *         DataBinRows rows = databin_rows(&bin, DATABIN_MONSTERS);
 *         while (databin_next(&rows, &row)) {
 *             ...
 *         }
 *     }
 *
 * Which number of a row is which is found by the name the file itself
 * gives the column, with databin_column().
 */
#ifndef ROM2_IMPORT_DATABIN_H
#define ROM2_IMPORT_DATABIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/reader.h"

/* The tables, in the order they have in the file. */
typedef enum DataBinTable {
    DATABIN_SHAPES,
    DATABIN_MATERIALS,
    DATABIN_MODIFIERS,
    DATABIN_ARMOUR,
    DATABIN_SHIELDS,
    DATABIN_WEAPONS,
    DATABIN_MAGIC_ITEMS,
    DATABIN_MONSTERS,
    DATABIN_HUMANS,
    DATABIN_BUILDINGS,
    DATABIN_SPELLS,
    DATABIN_TABLE_COUNT
} DataBinTable;

/* A text in the file is at most 255 characters. */
#define DATABIN_TEXT_SIZE 256
/* The most numbers, fractions and texts a row may have. The game's most
 * are 62 numbers, 9 fractions and 10 texts. */
#define DATABIN_MAX_NUMBERS 64
#define DATABIN_MAX_FRACTIONS 9
#define DATABIN_MAX_TEXTS 10

/* Where one table is in the file. */
typedef struct DataBinPlace {
    const uint8_t *header;  /* its column names, to the end of the file */
    size_t header_size;
    const uint8_t *rows;    /* its first row, to the end of the file */
    size_t rows_size;
    uint32_t row_count;
} DataBinPlace;

/* An opened data.bin. It points into the bytes it was opened on, which
 * must stay alive as long as it is used. */
typedef struct DataBin {
    DataBinPlace tables[DATABIN_TABLE_COUNT];
} DataBin;

/* One row of a table. */
typedef struct DataBinRow {
    char name[DATABIN_TEXT_SIZE];
    /* Whole numbers. The rows of armour, shields and weapons end in 8
     * short numbers, which are here too, after the others. An empty row
     * has none. */
    uint32_t number_count;
    int32_t numbers[DATABIN_MAX_NUMBERS];
    /* Only the rows of shapes and materials have fractions. */
    uint32_t fraction_count;
    double fractions[DATABIN_MAX_FRACTIONS];
    /* The texts after the numbers: the items a monster or a human has,
     * the effects of a spell or a magic item. */
    uint32_t text_count;
    char texts[DATABIN_MAX_TEXTS][DATABIN_TEXT_SIZE];
} DataBinRow;

/* The rows of one table that have not been read yet. */
typedef struct DataBinRows {
    DataBinTable table;
    Reader reader;
    uint32_t left;
} DataBinRows;

/* Opens a data.bin held in memory: finds where each table is, and checks
 * that every row of every table can be read.
 *
 * Returns false, and leaves `*out` alone, if a table is cut short or a
 * row has more numbers than DATABIN_MAX_NUMBERS. */
bool databin_open(const uint8_t *data, size_t size, DataBin *out);

/* Gives the rows of a table, to be read with databin_next(). */
DataBinRows databin_rows(const DataBin *bin, DataBinTable table);

/* Reads the next row. Returns false, and leaves `*out` alone, when there
 * are no more rows. */
bool databin_next(DataBinRows *rows, DataBinRow *out);

/* Finds the column called `name` in a table and says which of a row's
 * numbers it is: 0 for the first number. Capital and small letters must
 * match. Shapes and materials share their column names, and so do
 * armour, shields and weapons.
 *
 * Returns false, and leaves `*out_index` alone, if the table has no
 * column of that name, or it is the first column, the row's name. */
bool databin_column(const DataBin *bin, DataBinTable table,
    const char *name, uint32_t *out_index);

#endif
