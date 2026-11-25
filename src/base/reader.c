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

uint16_t reader_u16(Reader *r)
{
    if (reader_remaining(r) < 2) {
        r->failed = true;
        return 0;
    }
    const uint8_t *d = r->data + r->pos;
    uint16_t b = (uint16_t)(d[0] | (d[1] << 8));
    r->pos += 2;
    return b;
}

uint32_t reader_u32(Reader *r)
{
    if (reader_remaining(r) < 4) {
        r->failed = true;
        return 0;
    }
    const uint8_t *d = r->data + r->pos;
    uint32_t b = (uint32_t)d[0]
        | ((uint32_t)d[1] << 8)
        | ((uint32_t)d[2] << 16)
        | ((uint32_t)d[3] << 24);
    r->pos += 4;
    return b;
}

int8_t reader_i8(Reader *r)
{
    return (int8_t)reader_u8(r);
}

int16_t reader_i16(Reader *r)
{
    return (int16_t)reader_u16(r);
}

int32_t reader_i32(Reader *r)
{
    return (int32_t)reader_u32(r);
}

bool reader_ok(const Reader *r)
{
    return !r->failed;
}
