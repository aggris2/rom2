#include "import/reg.h"

#include "base/reader.h"
#include "import/name.h"

#define REG_SIGNATURE 0x31415926u
#define REG_HEADER_SIZE 24u
#define REG_ENTRY_SIZE 32u
#define REG_NAME_SIZE 16u

bool reg_open(const uint8_t *data, size_t size, Reg *out)
{
    Reader r = reader_init(data, size);
    uint32_t sig = reader_u32(&r);
    uint32_t root_first = reader_u32(&r);
    uint32_t root_count = reader_u32(&r);
    reader_u32(&r); /* flags, not used */
    uint32_t entry_count = reader_u32(&r);
    reader_u32(&r); /* always 0 */
    if (!reader_ok(&r)
        || sig != REG_SIGNATURE
        || size < 28
        || entry_count > (size - REG_HEADER_SIZE - 4) / REG_ENTRY_SIZE) {
        return false;
    }
    reader_skip(&r, (size_t)entry_count * REG_ENTRY_SIZE);
    uint32_t values_size = reader_u32(&r);
    size_t values_offset = r.pos;
    if (!reader_ok(&r)
        || reader_remaining(&r) != values_size
        || root_first > entry_count
        || root_count > entry_count - root_first) {
        return false;
    }
    out->data = data;
    out->size = size;
    out->root_first = root_first;
    out->root_count = root_count;
    out->entry_count = entry_count;
    out->values_offset = values_offset;
    out->values_size = values_size;
    return true;
}

bool reg_entry_at(const Reg *r, uint32_t index, RegEntry *out)
{
    if (index >= r->entry_count) {
        return false;
    }
    const uint8_t *at = r->data + REG_HEADER_SIZE
        + (size_t)index * REG_ENTRY_SIZE;
    Reader entry_reader = reader_init(at, REG_ENTRY_SIZE);
    reader_skip(&entry_reader, 4); /* unknown first 4 bytes */
    uint32_t a = reader_u32(&entry_reader);
    uint32_t b = reader_u32(&entry_reader);
    uint32_t type = reader_u32(&entry_reader);
    const uint8_t *name = reader_bytes(&entry_reader, REG_NAME_SIZE);
    if (!reader_ok(&entry_reader)) {
        return false;
    }
    switch (type) {
    case REG_INT:
    case REG_FLOAT:
        break;
    case REG_SECTION:
        if (a > r->entry_count || b > r->entry_count - a) {
            return false;
        }
        break;
    case REG_TEXT:
        if (a > r->values_size
            || b > r->values_size - a
            || b < 1 || r->data[r->values_offset + a + b - 1] != 0) {
            return false;
        }
        break;
    case REG_INT_LIST:
        if (a > r->values_size
            || b > r->values_size - a
            || b % 4 != 0) {
            return false;
        }
        break;
    default:
        return false;
    }
    RegEntry entry;
    entry.type = (RegType)type;
    entry.a = a;
    entry.b = b;
    name_copy(entry.name, sizeof entry.name, name, REG_NAME_SIZE);
    *out = entry;
    return true;
}
