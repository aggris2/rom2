#include "import/reg.h"

#include "base/reader.h"

#define REG_SIGNATURE 0x31415926u
#define REG_HEADER_SIZE 24u
#define REG_ENTRY_SIZE 32u

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
