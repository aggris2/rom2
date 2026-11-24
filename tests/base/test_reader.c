#include "base/reader.h"

#include "utest.h"

UTEST(reader, new_reader_has_everything_remaining)
{
    const uint8_t bytes[] = {1, 2, 3, 4};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(4u, reader_remaining(&r));
    ASSERT_TRUE(reader_ok(&r));
}

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

UTEST(reader, reading_past_the_end_fails_and_returns_zero)
{
    const uint8_t bytes[] = {42};
    Reader r = reader_init(bytes, sizeof bytes);

    ASSERT_EQ(42, reader_u8(&r));
    ASSERT_EQ(0, reader_u8(&r));
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(reader, failed_read_does_not_move)
{
    const uint8_t bytes[] = {42};
    Reader r = reader_init(bytes, sizeof bytes);

    reader_u8(&r);
    size_t pos_before = r.pos;
    reader_u8(&r);
    ASSERT_EQ(pos_before, r.pos);
}

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
