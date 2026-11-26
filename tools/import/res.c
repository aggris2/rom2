#include "import/res.h"

#include <string.h>

#include "import/name.h"

#define RES_SIGNATURE 0x31415926u
#define RES_HEADER_SIZE 24u
#define RES_ENTRY_SIZE 32u
#define RES_TYPE_FILE 0u
#define RES_TYPE_FOLDER 1u
#define RES_NAME_SIZE 16u

bool res_header_read(Reader *r, ResHeader *out)
{
    size_t size = reader_remaining(r);
    uint32_t sig = reader_u32(r);
    uint32_t root_first = reader_u32(r);
    uint32_t root_count = reader_u32(r);
    uint32_t flags = reader_u32(r);
    uint32_t table_offset = reader_u32(r);
    uint32_t entry_count = reader_u32(r);
    if (!reader_ok(r)
        || sig != RES_SIGNATURE
        || table_offset < RES_HEADER_SIZE
        || table_offset > size
        || entry_count > (size - table_offset) / RES_ENTRY_SIZE
        || root_first > entry_count
        || root_count > entry_count - root_first) {
        return false;
    }
    out->root_first = root_first;
    out->root_count = root_count;
    out->flags = flags;
    out->table_offset = table_offset;
    out->entry_count = entry_count;
    return true;
}

bool res_entry_read(Reader *r, ResEntry *out)
{
    reader_skip(r, 4);
    uint32_t offset = reader_u32(r);
    uint32_t size = reader_u32(r);
    uint32_t type = reader_u32(r);
    const uint8_t *name = reader_bytes(r, RES_NAME_SIZE);
    if (!reader_ok(r) || (type != RES_TYPE_FILE && type != RES_TYPE_FOLDER)) {
        return false;
    }
    out->is_folder = (type == RES_TYPE_FOLDER);
    out->offset = offset;
    out->size = size;
    name_copy(out->name, sizeof out->name, name, RES_NAME_SIZE);
    return true;
}

bool res_open(const uint8_t *data, size_t size, ResArchive *out)
{
    Reader r = reader_init(data, size);
    ResHeader header;
    if (!res_header_read(&r, &header)) {
        return false;
    }
    out->data = data;
    out->size = size;
    out->header = header;
    out->found = NULL;
    out->mark = 1;
    return true;
}

bool res_entry_at(const ResArchive *a, uint32_t index, ResEntry *out)
{
    if (index >= a->header.entry_count) {
        return false;
    }
    size_t at = (size_t)a->header.table_offset
        + (size_t)index * RES_ENTRY_SIZE;
    Reader r = reader_init(a->data + at, RES_ENTRY_SIZE);
    ResEntry entry;
    if (!res_entry_read(&r, &entry)) {
        return false;
    }
    if (entry.is_folder) {
        if (entry.offset > a->header.entry_count
            || entry.size > a->header.entry_count - entry.offset) {
            return false;
        }
    } else {
        if (entry.offset < RES_HEADER_SIZE
            || entry.offset > a->header.table_offset
            || entry.size > a->header.table_offset - entry.offset) {
            return false;
        }
    }
    *out = entry;
    return true;
}

Reader res_file_reader(const ResArchive *a, const ResEntry *entry)
{
    if (entry->is_folder) {
        Reader bad = reader_init(NULL, 0);
        bad.failed = true;
        return bad;
    }
    return reader_init(a->data + entry->offset, entry->size);
}

/* Looks in the folder made of `count` entries starting at entry `first`
 * for one whose name is the first `length` characters of `name`. */
static bool find_in_folder(const ResArchive *a, uint32_t first,
    uint32_t count, const char *name, size_t length, ResEntry *out,
    uint32_t *out_index)
{
    if (length > RES_NAME_SIZE) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        ResEntry entry;
        if (!res_entry_at(a, first + i, &entry)) {
            return false;
        }
        if (strncmp(entry.name, name, length) == 0
            && entry.name[length] == '\0') {
            *out = entry;
            *out_index = first + i;
            return true;
        }
    }
    return false;
}

void res_note(const ResArchive *a, uint32_t index)
{
    if (a->found != NULL && index < a->header.entry_count) {
        a->found[index] = a->mark;
    }
}

bool res_find(const ResArchive *a, const char *path, ResEntry *out)
{
    uint32_t first = a->header.root_first;
    uint32_t count = a->header.root_count;

    for (;;) {
        /* The next name runs up to the next '/', or to the end of the
         * path if there is no '/'. */
        const char *slash = strchr(path, '/');
        size_t length = slash != NULL ? (size_t)(slash - path) : strlen(path);

        ResEntry entry;
        uint32_t index = 0;
        if (!find_in_folder(a, first, count, path, length, &entry,
                &index)) {
            return false;
        }
        if (slash == NULL) {
            if (!entry.is_folder) {
                res_note(a, index);
            }
            *out = entry;
            return true;
        }

        /* More of the path follows, so this name has to be a folder. */
        if (!entry.is_folder) {
            return false;
        }
        first = entry.offset;
        count = entry.size;
        path = slash + 1;
    }
}
