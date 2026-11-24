#include "base/reader.h"

Reader reader_init(const uint8_t *data, size_t size)
{
    Reader r;
    r.data = data;
    r.size = size;
    r.pos = 0;
    r.failed = false;
    if (data == NULL && size != 0) {
        r.size = 0;
        r.failed = true;
    }
    return r;
}

size_t reader_remaining(const Reader *r)
{
    if (r->failed) {
        return 0;
    }
    return r->size - r->pos;
}

uint8_t reader_u8(Reader *r)
{
    if (reader_remaining(r) < 1) {
        r->failed = true;
        return 0;
    }
    uint8_t value = r->data[r->pos];
    r->pos += 1;
    return value;
}

bool reader_ok(const Reader *r)
{
    return !r->failed;
}
