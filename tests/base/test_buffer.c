#include "base/buffer.h"

#include "utest.h"

/* ---- buffer_push ---- */

UTEST(buffer, a_zeroed_buffer_is_empty)
{
    ByteBuffer b = {0};

    ASSERT_EQ(0u, b.size);
    ASSERT_TRUE(b.data == NULL);
    buffer_free(&b);
}

UTEST(buffer, push_adds_bytes_in_order)
{
    ByteBuffer b = {0};

    ASSERT_TRUE(buffer_push(&b, 0x10));
    ASSERT_TRUE(buffer_push(&b, 0xff));
    ASSERT_TRUE(buffer_push(&b, 0x00));

    ASSERT_EQ(3u, b.size);
    ASSERT_EQ(0x10, b.data[0]);
    ASSERT_EQ(0xff, b.data[1]);
    ASSERT_EQ(0x00, b.data[2]);
    buffer_free(&b);
}

UTEST(buffer, push_keeps_every_byte_while_the_buffer_grows)
{
    /* Far more than the first capacity, so the block is reallocated and
     * moved several times. */
    ByteBuffer b = {0};

    for (size_t i = 0; i < 5000; i++) {
        ASSERT_TRUE(buffer_push(&b, (uint8_t)(i % 251)));
    }

    ASSERT_EQ(5000u, b.size);
    ASSERT_TRUE(b.capacity >= b.size);
    for (size_t i = 0; i < 5000; i++) {
        ASSERT_EQ((uint8_t)(i % 251), b.data[i]);
    }
    buffer_free(&b);
}

/* ---- buffer_append ---- */

UTEST(buffer, append_adds_a_run_of_bytes)
{
    const uint8_t first[] = {1, 2, 3};
    const uint8_t second[] = {4, 5};
    ByteBuffer b = {0};

    ASSERT_TRUE(buffer_append(&b, first, sizeof first));
    ASSERT_TRUE(buffer_append(&b, second, sizeof second));

    ASSERT_EQ(5u, b.size);
    for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ((uint8_t)(i + 1), b.data[i]);
    }
    buffer_free(&b);
}

UTEST(buffer, append_of_a_run_larger_than_the_capacity)
{
    /* One append far bigger than doubling the first capacity gives. */
    uint8_t big[1000];
    for (size_t i = 0; i < sizeof big; i++) {
        big[i] = (uint8_t)(i % 199);
    }
    ByteBuffer b = {0};
    ASSERT_TRUE(buffer_push(&b, 0x77));

    ASSERT_TRUE(buffer_append(&b, big, sizeof big));

    ASSERT_EQ(1001u, b.size);
    ASSERT_EQ(0x77, b.data[0]);
    ASSERT_EQ(big[0], b.data[1]);
    ASSERT_EQ(big[999], b.data[1000]);
    buffer_free(&b);
}

UTEST(buffer, append_of_nothing_does_nothing)
{
    ByteBuffer b = {0};

    ASSERT_TRUE(buffer_append(&b, NULL, 0));
    ASSERT_EQ(0u, b.size);

    ASSERT_TRUE(buffer_push(&b, 9));
    ASSERT_TRUE(buffer_append(&b, NULL, 0));
    ASSERT_EQ(1u, b.size);
    ASSERT_EQ(9, b.data[0]);
    buffer_free(&b);
}

UTEST(buffer, append_of_an_impossible_size_fails_and_changes_nothing)
{
    /* One byte in use plus this many more is more than a size can hold,
     * so the buffer refuses before asking for any memory. The bytes are
     * never looked at. */
    const uint8_t byte = 0;
    ByteBuffer b = {0};
    ASSERT_TRUE(buffer_push(&b, 5));
    uint8_t *data_before = b.data;
    size_t capacity_before = b.capacity;

    ASSERT_FALSE(buffer_append(&b, &byte, SIZE_MAX));

    ASSERT_EQ(1u, b.size);
    ASSERT_TRUE(b.data == data_before);
    ASSERT_EQ(capacity_before, b.capacity);
    ASSERT_EQ(5, b.data[0]);
    buffer_free(&b);
}

/* ---- buffer_u32_le and buffer_u32_be ---- */

UTEST(buffer, u32_le_writes_the_low_byte_first)
{
    ByteBuffer b = {0};

    ASSERT_TRUE(buffer_u32_le(&b, 0x31415926u));

    ASSERT_EQ(4u, b.size);
    ASSERT_EQ(0x26, b.data[0]);
    ASSERT_EQ(0x59, b.data[1]);
    ASSERT_EQ(0x41, b.data[2]);
    ASSERT_EQ(0x31, b.data[3]);
    buffer_free(&b);
}

UTEST(buffer, u32_be_writes_the_high_byte_first)
{
    ByteBuffer b = {0};

    ASSERT_TRUE(buffer_u32_be(&b, 0x31415926u));

    ASSERT_EQ(4u, b.size);
    ASSERT_EQ(0x31, b.data[0]);
    ASSERT_EQ(0x41, b.data[1]);
    ASSERT_EQ(0x59, b.data[2]);
    ASSERT_EQ(0x26, b.data[3]);
    buffer_free(&b);
}

/* ---- buffer_free ---- */

UTEST(buffer, free_resets_the_buffer_so_it_can_be_used_again)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(buffer_push(&b, 1));

    buffer_free(&b);

    ASSERT_TRUE(b.data == NULL);
    ASSERT_EQ(0u, b.size);
    ASSERT_EQ(0u, b.capacity);

    ASSERT_TRUE(buffer_push(&b, 2));
    ASSERT_EQ(1u, b.size);
    ASSERT_EQ(2, b.data[0]);
    buffer_free(&b);
    buffer_free(&b); /* a second time must be harmless */
}
