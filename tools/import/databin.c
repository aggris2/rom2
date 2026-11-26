#include "import/databin.h"

#include <string.h>

/* How the rows of a table are stored. */
typedef struct Format {
    /* True if the table has column names of its own in front of it.
     * Otherwise it uses those of the table before. */
    bool has_header;
    /* How much larger the stored count is than the number of rows. */
    uint32_t count_extra;
    /* True if a short number after the name says how many numbers the
     * row has. */
    bool counted;
    uint32_t fractions;    /* fractions after the name */
    uint32_t shorts;       /* short numbers after the numbers */
    uint32_t texts;        /* texts at the end */
    /* True if the row ends in a byte, and a text if the byte is not 0. */
    bool optional_text;
    /* The bytes an empty row has after its count of 0. */
    uint32_t empty_size;
} Format;

static const Format FORMATS[DATABIN_TABLE_COUNT] = {
    [DATABIN_SHAPES]      = {true,  0, false, 9, 0, 0,  false, 0},
    [DATABIN_MATERIALS]   = {false, 0, false, 9, 0, 0,  false, 0},
    [DATABIN_MODIFIERS]   = {true,  0, true,  0, 0, 0,  false, 0},
    [DATABIN_ARMOUR]      = {true,  1, true,  0, 8, 0,  false, 0},
    [DATABIN_SHIELDS]     = {false, 1, true,  0, 8, 0,  false, 0},
    [DATABIN_WEAPONS]     = {false, 1, true,  0, 8, 0,  false, 0},
    [DATABIN_MAGIC_ITEMS] = {true,  1, true,  0, 0, 0,  true,  0},
    [DATABIN_MONSTERS]    = {true,  1, true,  0, 0, 2,  false, 2},
    [DATABIN_HUMANS]      = {true,  1, true,  0, 0, 10, false, 10},
    [DATABIN_BUILDINGS]   = {true,  1, true,  0, 0, 0,  false, 0},
    [DATABIN_SPELLS]      = {true,  2, true,  0, 0, 1,  false, 2},
};

/* Reads a text: a byte with its length, then the characters. `out` gets
 * a closing zero. */
static void read_text(Reader *r, char out[DATABIN_TEXT_SIZE])
{
    uint8_t length = reader_u8(r);
    const uint8_t *text = reader_bytes(r, length);
    if (reader_ok(r)) {
        if (length > 0) {
            memcpy(out, text, length);
        }
        out[length] = '\0';
    }
}

/* Reads a fraction: 8 bytes, an IEEE double, low byte first. */
static double read_fraction(Reader *r)
{
    uint64_t low = reader_u32(r);
    uint64_t high = reader_u32(r);
    uint64_t bits = low | (high << 32);
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

/* Reads one row in the given format. The reader fails if the row is cut
 * short or has too many numbers. */
static void read_row(Reader *r, const Format *format, DataBinRow *row)
{
    row->number_count = 0;
    row->fraction_count = 0;
    row->text_count = 0;
    read_text(r, row->name);

    for (uint32_t i = 0; i < format->fractions; i++) {
        row->fractions[row->fraction_count++] = read_fraction(r);
    }
    if (!format->counted) {
        return;
    }

    uint32_t count = reader_u16(r);
    if (count == 0 && format->empty_size > 0) {
        reader_skip(r, format->empty_size);
        return;
    }
    if (count + format->shorts > DATABIN_MAX_NUMBERS) {
        r->failed = true;
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        row->numbers[row->number_count++] = reader_i32(r);
    }
    for (uint32_t i = 0; i < format->shorts; i++) {
        row->numbers[row->number_count++] = reader_u16(r);
    }
    for (uint32_t i = 0; i < format->texts; i++) {
        read_text(r, row->texts[row->text_count++]);
    }
    if (format->optional_text && reader_u8(r) != 0) {
        read_text(r, row->texts[row->text_count++]);
    }
}

/* Skips the column names: a short number and that many texts. */
static void skip_header(Reader *r)
{
    char name[DATABIN_TEXT_SIZE];
    uint32_t count = reader_u16(r);
    for (uint32_t i = 0; i < count && reader_ok(r); i++) {
        read_text(r, name);
    }
}

bool databin_open(const uint8_t *data, size_t size, DataBin *out)
{
    /* static: a row is too large for the stack to be at ease. */
    static DataBinRow row;
    DataBin bin;
    Reader r = reader_init(data, size);
    const uint8_t *header = data;
    size_t header_size = size;

    for (int t = 0; t < DATABIN_TABLE_COUNT; t++) {
        const Format *format = &FORMATS[t];
        if (format->has_header) {
            header = data + (size - reader_remaining(&r));
            header_size = reader_remaining(&r);
            skip_header(&r);
        }
        uint32_t count = reader_u32(&r);
        if (!reader_ok(&r) || count < format->count_extra) {
            return false;
        }
        count -= format->count_extra;

        DataBinPlace *place = &bin.tables[t];
        place->header = header;
        place->header_size = header_size;
        place->rows = data + (size - reader_remaining(&r));
        place->rows_size = reader_remaining(&r);
        place->row_count = count;

        /* A count that is too large runs the reader off the end. */
        for (uint32_t i = 0; i < count && reader_ok(&r); i++) {
            read_row(&r, format, &row);
        }
        if (!reader_ok(&r)) {
            return false;
        }
    }
    *out = bin;
    return true;
}

DataBinRows databin_rows(const DataBin *bin, DataBinTable table)
{
    const DataBinPlace *place = &bin->tables[table];
    DataBinRows rows;
    rows.table = table;
    rows.reader = reader_init(place->rows, place->rows_size);
    rows.left = place->row_count;
    return rows;
}

bool databin_next(DataBinRows *rows, DataBinRow *out)
{
    /* static: see databin_open(). */
    static DataBinRow row;
    if (rows->left == 0) {
        return false;
    }
    read_row(&rows->reader, &FORMATS[rows->table], &row);
    if (!reader_ok(&rows->reader)) {
        rows->left = 0;
        return false;
    }
    rows->left--;
    *out = row;
    return true;
}

bool databin_column(const DataBin *bin, DataBinTable table,
    const char *name, uint32_t *out_index)
{
    const DataBinPlace *place = &bin->tables[table];
    Reader r = reader_init(place->header, place->header_size);
    char column[DATABIN_TEXT_SIZE];
    uint32_t count = reader_u16(&r);

    /* The first name is the column of the rows' names. The numbers of a
     * row belong to the names after it. */
    read_text(&r, column);
    for (uint32_t i = 1; i < count; i++) {
        read_text(&r, column);
        if (!reader_ok(&r)) {
            return false;
        }
        if (strcmp(column, name) == 0) {
            *out_index = i - 1;
            return true;
        }
    }
    return false;
}
