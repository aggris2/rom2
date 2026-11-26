#include "import/reg.h"

#include <string.h>

#include "utest.h"

/* A small valid registry, built by hand: the 24-byte header, a table of
 * two entries, the size of the values area, and 8 bytes of values. The
 * entries and the values are left as zeros, because these tests only read
 * the header.
 *
 *     bytes 0 to 23      header
 *     bytes 24 to 87     table, 2 entries of 32 bytes
 *     bytes 88 to 91     the number 8
 *     bytes 92 to 99     values area
 */
enum {
    REG_SIZE = 24 + 2 * 32 + 4 + 8,
    VALUES_OFFSET = 24 + 2 * 32 + 4,

    /* Where each number starts. */
    AT_SIGNATURE = 0,
    AT_ROOT_FIRST = 4,
    AT_ROOT_COUNT = 8,
    AT_FLAGS = 12,
    AT_ENTRY_COUNT = 16,
    AT_UNUSED = 20,
    AT_VALUES_SIZE = 24 + 2 * 32
};

static void put_u32(uint8_t *bytes, size_t at, uint32_t value)
{
    bytes[at] = (uint8_t)(value & 0xff);
    bytes[at + 1] = (uint8_t)((value >> 8) & 0xff);
    bytes[at + 2] = (uint8_t)((value >> 16) & 0xff);
    bytes[at + 3] = (uint8_t)((value >> 24) & 0xff);
}

static void make_valid_registry(uint8_t bytes[REG_SIZE])
{
    memset(bytes, 0, REG_SIZE);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_ROOT_FIRST, 0);
    put_u32(bytes, AT_ROOT_COUNT, 2);
    put_u32(bytes, AT_FLAGS, 17);
    put_u32(bytes, AT_ENTRY_COUNT, 2);
    put_u32(bytes, AT_VALUES_SIZE, 8);
}

/* A Reg that has not been written to. A failed open must leave it exactly
 * like this. */
static Reg untouched_reg(void)
{
    Reg r = {
        .data = NULL,
        .size = 12345,
        .root_first = 0xaaaaaaaau,
        .root_count = 0xaaaaaaaau,
        .entry_count = 0xaaaaaaaau,
        .values_offset = 54321,
        .values_size = 0xaaaaaaaau
    };
    return r;
}

static bool reg_is_untouched(const Reg *r)
{
    return r->data == NULL
        && r->size == 12345
        && r->root_first == 0xaaaaaaaau
        && r->root_count == 0xaaaaaaaau
        && r->entry_count == 0xaaaaaaaau
        && r->values_offset == 54321
        && r->values_size == 0xaaaaaaaau;
}

/* ---- reg_open ---- */

UTEST(reg, open_reads_the_header_and_finds_the_values_area)
{
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    Reg r = untouched_reg();

    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));

    ASSERT_TRUE(r.data == bytes);
    ASSERT_EQ((size_t)REG_SIZE, r.size);
    ASSERT_EQ(0u, r.root_first);
    ASSERT_EQ(2u, r.root_count);
    ASSERT_EQ(2u, r.entry_count);
    ASSERT_EQ((size_t)VALUES_OFFSET, r.values_offset);
    ASSERT_EQ(8u, r.values_size);
}

UTEST(reg, open_does_not_mix_up_the_header_numbers)
{
    /* Different values everywhere, so that two swapped numbers would
     * show: 5 entries, root entries 1 to 3, and 2 bytes of values. */
    uint8_t bytes[24 + 5 * 32 + 4 + 2];
    memset(bytes, 0, sizeof bytes);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_ROOT_FIRST, 1);
    put_u32(bytes, AT_ROOT_COUNT, 3);
    put_u32(bytes, AT_FLAGS, 17);
    put_u32(bytes, AT_ENTRY_COUNT, 5);
    put_u32(bytes, 24 + 5 * 32, 2);
    Reg r = untouched_reg();

    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));

    ASSERT_EQ(1u, r.root_first);
    ASSERT_EQ(3u, r.root_count);
    ASSERT_EQ(5u, r.entry_count);
    ASSERT_EQ((size_t)(24 + 5 * 32 + 4), r.values_offset);
    ASSERT_EQ(2u, r.values_size);
}

UTEST(reg, open_of_a_registry_with_nothing_in_it)
{
    /* The smallest valid registry: a header, no entries, and a values
     * area of size 0. */
    uint8_t bytes[24 + 4];
    memset(bytes, 0, sizeof bytes);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_FLAGS, 17);
    Reg r = untouched_reg();

    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));

    ASSERT_EQ(0u, r.root_count);
    ASSERT_EQ(0u, r.entry_count);
    ASSERT_EQ(28u, r.values_offset);
    ASSERT_EQ(0u, r.values_size);
}

UTEST(reg, open_of_an_empty_file_fails)
{
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(NULL, 0, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_of_a_file_cut_off_anywhere_fails)
{
    /* Every shorter version of the registry, from one byte up to one
     * byte short of whole. */
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    Reg r = untouched_reg();

    for (size_t size = 1; size < REG_SIZE; size++) {
        ASSERT_FALSE(reg_open(bytes, size, &r));
        ASSERT_TRUE(reg_is_untouched(&r));
    }
}

UTEST(reg, open_with_wrong_signature_fails)
{
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_SIGNATURE, 0x31415927u);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_one_entry_too_many_fails)
{
    /* Three entries would need 96 bytes of table and 4 more, and only 76
     * bytes follow the header. */
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_ENTRY_COUNT, 3);
    put_u32(bytes, AT_ROOT_COUNT, 0);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_a_huge_entry_count_fails)
{
    /* 0x08000002 * 32 wraps round to 64 in 32 bits, which is exactly the
     * size of this registry's table. */
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_ENTRY_COUNT, 0x08000002u);
    put_u32(bytes, AT_ROOT_COUNT, 0);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_fewer_entries_than_the_file_holds_fails)
{
    /* With one entry the values size would be read from the middle of
     * the second entry, where it is 0, and 40 bytes would be left over. */
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_ENTRY_COUNT, 1);
    put_u32(bytes, AT_ROOT_COUNT, 1);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_a_values_area_one_byte_short_fails)
{
    /* The file says 9 bytes of values and has 8. */
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_VALUES_SIZE, 9);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_bytes_left_over_after_the_values_fails)
{
    /* The file says 7 bytes of values and has 8. */
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_VALUES_SIZE, 7);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_a_huge_values_size_fails)
{
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_VALUES_SIZE, 0xffffffffu);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_root_starting_past_the_table_fails)
{
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_ROOT_FIRST, 3);
    put_u32(bytes, AT_ROOT_COUNT, 0);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_root_running_past_the_table_fails)
{
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_ROOT_COUNT, 3);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_with_a_huge_root_count_fails)
{
    /* root_first + root_count wraps round to 1 in 32 bits. */
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_ROOT_FIRST, 2);
    put_u32(bytes, AT_ROOT_COUNT, 0xffffffffu);
    Reg r = untouched_reg();

    ASSERT_FALSE(reg_open(bytes, sizeof bytes, &r));
    ASSERT_TRUE(reg_is_untouched(&r));
}

UTEST(reg, open_accepts_a_root_that_ends_exactly_at_the_last_entry)
{
    uint8_t bytes[REG_SIZE];
    make_valid_registry(bytes);
    put_u32(bytes, AT_ROOT_FIRST, 1);
    put_u32(bytes, AT_ROOT_COUNT, 1);
    Reg r = untouched_reg();

    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));

    ASSERT_EQ(1u, r.root_first);
    ASSERT_EQ(1u, r.root_count);
}
