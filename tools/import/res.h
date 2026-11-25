/* Reading the original game's .res archives.
 *
 * An archive is a 24-byte header, then the files one after another, then a
 * table of 32-byte entries that runs to the end of the archive. All numbers
 * are unsigned 32-bit, low byte first.
 */
#ifndef ROM2_IMPORT_RES_H
#define ROM2_IMPORT_RES_H

#include <stdbool.h>
#include <stdint.h>

#include "base/reader.h"

typedef struct ResHeader {
    uint32_t root_first;   /* index of the root folder's first entry */
    uint32_t root_count;   /* number of entries in the root folder */
    uint32_t flags;        /* 1 or 17 in the original game; meaning unknown */
    uint32_t table_offset; /* where the entry table starts */
    uint32_t entry_count;  /* number of entries in the table */
} ResHeader;

/* Reads and checks the header of a .res archive. `r` must cover the whole
 * archive and stand at its start.
 *
 * Returns false, and leaves `*out` alone, if the archive is too short, the
 * signature is wrong, the table does not fit inside the archive, or the
 * root folder's entries are not all inside the table. */
bool res_header_read(Reader *r, ResHeader *out);

typedef struct ResEntry {
    bool is_folder;
    uint32_t offset;
    uint32_t size;
    char name[17];   /* up to 16 characters and the closing zero */
} ResEntry;

/* Reads one 32-byte table entry. Returns false, and leaves `*out` alone,
 * if fewer than 32 bytes are left or the type is neither file nor folder. */
bool res_entry_read(Reader *r, ResEntry *out);

/* An open archive: its bytes and its checked header. Does not own the
 * bytes. */
typedef struct ResArchive {
    const uint8_t *data;
    size_t size;
    ResHeader header;
} ResArchive;

/* Opens an archive held in memory. Returns false, and leaves `*out` alone,
 * if the header is not valid. */
bool res_open(const uint8_t *data, size_t size, ResArchive *out);

/* Reads entry number `index` of the table and checks it. Returns false,
 * and leaves `*out` alone, if there is no such entry, it cannot be read,
 * or it points outside the archive. */
bool res_entry_at(const ResArchive *a, uint32_t index, ResEntry *out);

#endif
