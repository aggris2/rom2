#include "import/res.h"

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

static void copy_name(char *dst, size_t dst_size, const uint8_t *field, size_t field_size)
{
    if (dst_size == 0) {
        return;
    }
    size_t i = 0;
    while ((i < dst_size - 1) && i < field_size && field[i] != 0) {
        dst[i] = (char)field[i];
        i++;
    }
    dst[i] = '\0';
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
    copy_name(out->name, sizeof out->name, name, RES_NAME_SIZE);
    return true;
}
