#include "base/reader.h"

#include "utest.h"

/* ---- reader_init ---- */

UTEST(reader, new_reader_has_everything_remaining)
{
    const uint8_t bytes[] = {1, 2, 3, 4};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(4u, reader_remaining(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, empty_reader_is_valid_until_read)
{
    Reader r = reader_init(NULL, 0);

    ASSERT_TRUE(reader_ok(&r));
    ASSERT_EQ(0u, reader_remaining(&r));
    ASSERT_EQ(0, reader_u8(&r));
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(reader, null_data_with_a_size_starts_failed)
{
    Reader r = reader_init(NULL, 5);

    ASSERT_FALSE(reader_ok(&r));
    ASSERT_EQ(0u, reader_remaining(&r));
    ASSERT_EQ(0, reader_u8(&r));
}

/* ---- reader_u8 ---- */

UTEST(reader, u8_returns_bytes_in_order)
{
    const uint8_t bytes[] = {0x10, 0xff, 0x00};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(0x10, reader_u8(&r));
    ASSERT_EQ(0xff, reader_u8(&r));
    ASSERT_EQ(0x00, reader_u8(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, u8_reduces_remaining_by_one)
{
    const uint8_t bytes[] = {7, 8};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u8(&r);
    ASSERT_EQ(1u, reader_remaining(&r));
    reader_u8(&r);
    ASSERT_EQ(0u, reader_remaining(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, u8_past_the_end_fails_and_returns_zero)
{
    const uint8_t bytes[] = {42};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(42, reader_u8(&r));
    ASSERT_EQ(0, reader_u8(&r));
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(reader, u8_failed_read_does_not_move)
{
    const uint8_t bytes[] = {42};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u8(&r);
    size_t pos_before = r.pos;
    reader_u8(&r);
    ASSERT_EQ(pos_before, r.pos);
}

/* ---- reader_u16 ---- */

UTEST(reader, u16_reads_low_byte_first)
{
    const uint8_t bytes[] = {0x10, 0xff, 0x00, 0xaa};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(0xff10, reader_u16(&r));
    ASSERT_EQ(0xaa00, reader_u16(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, u16_reduces_remaining_by_two)
{
    const uint8_t bytes[] = {0x07, 0x08, 0x09, 0x03, 0x05};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u16(&r);
    ASSERT_EQ(3u, reader_remaining(&r));
    reader_u16(&r);
    ASSERT_EQ(1u, reader_remaining(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, u16_past_the_end_fails_and_returns_zero)
{
    const uint8_t bytes[] = {0x34, 0x12, 0x07};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(0x1234, reader_u16(&r));
    ASSERT_EQ(0, reader_u16(&r));
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(reader, u16_failed_read_does_not_move)
{
    const uint8_t bytes[] = {0x34, 0x12, 0x07};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u16(&r);
    size_t pos_before = r.pos;
    reader_u16(&r);
    ASSERT_EQ(pos_before, r.pos);
}

/* ---- reader_u32 ---- */

UTEST(reader, u32_reads_low_byte_first)
{
    const uint8_t bytes[] = {0x10, 0xff, 0x00, 0xaa, 0x34, 0x12, 0x07, 0x08};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(0xaa00ff10, reader_u32(&r));
    ASSERT_EQ(0x08071234, reader_u32(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, u32_reduces_remaining_by_four)
{
    const uint8_t bytes[] = {0x10, 0xff, 0x00, 0xaa, 0x34, 0x12, 0x07, 0x08, 0x77};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u32(&r);
    ASSERT_EQ(5u, reader_remaining(&r));
    reader_u32(&r);
    ASSERT_EQ(1u, reader_remaining(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, u32_past_the_end_fails_and_returns_zero)
{
    const uint8_t bytes[] = {0x10, 0xff, 0x00, 0xaa, 0x34};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(0xaa00ff10, reader_u32(&r));
    ASSERT_EQ(0, reader_u32(&r));
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(reader, u32_failed_read_does_not_move)
{
    const uint8_t bytes[] = {0x10, 0xff, 0x00, 0xaa, 0x34};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u32(&r);
    size_t pos_before = r.pos;
    reader_u32(&r);
    ASSERT_EQ(pos_before, r.pos);
}

/* ---- reader_i8 ---- */

UTEST(reader, i8_reads_negative_and_positive)
{
    const uint8_t bytes[] = {0xf0, 0x7f};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(-16, reader_i8(&r));
    ASSERT_EQ(127, reader_i8(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, i8_past_the_end_fails_and_returns_zero)
{
    Reader r = reader_init(NULL, 0);

    ASSERT_EQ(0, reader_i8(&r));
    ASSERT_FALSE(reader_ok(&r));
}

/* ---- reader_i16 ---- */

UTEST(reader, i16_reads_negative_and_positive)
{
    const uint8_t bytes[] = {0x00, 0xf0, 0x7f, 0x7f};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(-4096, reader_i16(&r));
    ASSERT_EQ(32639, reader_i16(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, i16_past_the_end_fails_and_returns_zero)
{
    Reader r = reader_init(NULL, 0);

    ASSERT_EQ(0, reader_i16(&r));
    ASSERT_FALSE(reader_ok(&r));
}

/* ---- reader_i32 ---- */

UTEST(reader, i32_reads_negative_and_positive)
{
    const uint8_t bytes[] = {0xf0, 0xf0, 0xff, 0xff, 0x01, 0x02, 0x03, 0x00};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(-3856, reader_i32(&r));
    ASSERT_EQ(0x00030201, reader_i32(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, i32_past_the_end_fails_and_returns_zero)
{
    Reader r = reader_init(NULL, 0);

    ASSERT_EQ(0, reader_i32(&r));
    ASSERT_FALSE(reader_ok(&r));
}

/* ---- reader_skip ---- */

UTEST(reader, skip_moves_past_count)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_skip(&r, 2);

    ASSERT_EQ(0x33, reader_u8(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, skip_to_the_exact_end_is_allowed)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_skip(&r, 4);

    ASSERT_TRUE(reader_ok(&r));
    ASSERT_EQ(0u, reader_remaining(&r));
}

UTEST(reader, skip_of_zero_does_nothing)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_skip(&r, 0);

    ASSERT_EQ(0x11, reader_u8(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, skip_of_zero_on_empty_reader)
{
    Reader r = reader_init(NULL, 0);

    reader_skip(&r, 0);

    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, skip_past_the_end_fails_and_does_not_move)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_skip(&r, 5);

    ASSERT_FALSE(reader_ok(&r));
    ASSERT_EQ(0u, r.pos);
}

UTEST(reader, skip_of_a_huge_count_fails)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_skip(&r, SIZE_MAX);

    ASSERT_FALSE(reader_ok(&r));
    ASSERT_EQ(0u, r.pos);
}

/* ---- reader_bytes ---- */

UTEST(reader, bytes_returns_pointer_to_current_position)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u8(&r);
    const uint8_t *p = reader_bytes(&r, 2);

    ASSERT_TRUE(p == bytes + 1);
    ASSERT_EQ(0x22, p[0]);
    ASSERT_EQ(0x33, p[1]);
    ASSERT_EQ(0x44, reader_u8(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, bytes_to_the_exact_end_is_allowed)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    const uint8_t *p = reader_bytes(&r, 4);

    ASSERT_TRUE(p == bytes);
    ASSERT_EQ(0x44, p[3]);
    ASSERT_TRUE(reader_ok(&r));
    ASSERT_EQ(0u, reader_remaining(&r));
}

UTEST(reader, bytes_past_the_end_fails_and_returns_null)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    const uint8_t *p = reader_bytes(&r, 5);

    ASSERT_TRUE(p == NULL);
    ASSERT_EQ(0u, r.pos);
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(reader, bytes_of_a_huge_count_fails)
{
    const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44};
    Reader r = reader_init(bytes, sizeof bytes);

    const uint8_t *p = reader_bytes(&r, SIZE_MAX);

    ASSERT_TRUE(p == NULL);
    ASSERT_EQ(0u, r.pos);
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(reader, bytes_of_zero_does_not_fail)
{
    const uint8_t bytes[] = {0x11, 0x22};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_bytes(&r, 0);

    ASSERT_TRUE(reader_ok(&r));
    ASSERT_EQ(0x11, reader_u8(&r));
}

UTEST(reader, bytes_of_zero_on_empty_reader_does_not_fail)
{
    Reader r = reader_init(NULL, 0);

    const uint8_t *p = reader_bytes(&r, 0);

    ASSERT_TRUE(p == NULL);
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(reader, bytes_past_the_end_of_an_empty_reader_fails)
{
    Reader r = reader_init(NULL, 0);

    const uint8_t *p = reader_bytes(&r, 1);

    ASSERT_TRUE(p == NULL);
    ASSERT_EQ(0u, r.pos);
    ASSERT_FALSE(reader_ok(&r));
}

/* ---- failure, for every kind of read ---- */

UTEST(reader, failure_is_sticky)
{
    const uint8_t bytes[] = {42};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u8(&r);
    reader_u8(&r); /* fails */
    size_t pos_after_failure = r.pos;

    ASSERT_EQ(0, reader_u8(&r));
    ASSERT_EQ(pos_after_failure, r.pos);
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(reader, remaining_is_zero_after_failure)
{
    const uint8_t bytes[] = {1, 2, 3};
    Reader r = reader_init(bytes, sizeof bytes);
    r.failed = true; /* as if an earlier read had failed */

    ASSERT_EQ(0u, reader_remaining(&r));
    ASSERT_EQ(0, reader_u8(&r));
    ASSERT_EQ(0u, r.pos);
}
