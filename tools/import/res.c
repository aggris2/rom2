#include "import/res.h"

#define RES_SIGNATURE 0x31415926u
#define RES_HEADER_SIZE 24u
#define RES_ENTRY_SIZE 32u

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
