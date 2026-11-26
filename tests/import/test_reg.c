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

/* ---- reg_entry_at ---- */

/* A registry with one entry of every kind, built by hand.
 *
 *     entry 0   section  "Sec"   holds entries 1 to 5
 *     entry 1   number   "Num"   -2
 *     entry 2   decimal  "Flt"   1.0
 *     entry 3   text     "Txt"   "abc"       values 0 to 3
 *     entry 4   list     "Lst"   1, 2        values 4 to 11
 *     entry 5   text     "End"   "\x07"      values 14 to 15
 *
 * The values area is 16 bytes. Entry 0 ends exactly at the last entry and
 * entry 5 exactly at the last byte of the values area. */
enum {
    FULL_ENTRIES = 6,
    FULL_VALUES = 16,
    FULL_VALUES_OFFSET = 24 + FULL_ENTRIES * 32 + 4,
    FULL_SIZE = FULL_VALUES_OFFSET + FULL_VALUES,

    /* Where each part of an entry starts. */
    AT_ENTRY_A = 4,
    AT_ENTRY_B = 8,
    AT_ENTRY_TYPE = 12,
    AT_ENTRY_NAME = 16
};

static uint8_t *full_entry(uint8_t bytes[FULL_SIZE], uint32_t index)
{
    return bytes + 24 + index * 32;
}

/* Writes one table entry. `name` is copied without its closing zero. */
static void put_entry(uint8_t *entry, uint32_t type, uint32_t a, uint32_t b,
    const char *name)
{
    memset(entry, 0, 32);
    put_u32(entry, AT_ENTRY_A, a);
    put_u32(entry, AT_ENTRY_B, b);
    put_u32(entry, AT_ENTRY_TYPE, type);
    memcpy(entry + AT_ENTRY_NAME, name, strlen(name));
}

static void make_full_registry(uint8_t bytes[FULL_SIZE])
{
    const uint8_t values[FULL_VALUES] = {
        'a', 'b', 'c', 0,           /* "abc" */
        1, 0, 0, 0, 2, 0, 0, 0,     /* the numbers 1 and 2 */
        'x', 0,                     /* not used by any entry */
        7, 0,                       /* a one-character text */
    };
    memset(bytes, 0, FULL_SIZE);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_ROOT_FIRST, 0);
    put_u32(bytes, AT_ROOT_COUNT, 1);
    put_u32(bytes, AT_FLAGS, 17);
    put_u32(bytes, AT_ENTRY_COUNT, FULL_ENTRIES);
    put_entry(full_entry(bytes, 0), 1, 1, 5, "Sec");
    put_entry(full_entry(bytes, 1), 2, 0xfffffffeu, 0, "Num");
    put_entry(full_entry(bytes, 2), 4, 0x3f800000u, 0, "Flt");
    put_entry(full_entry(bytes, 3), 0, 0, 4, "Txt");
    put_entry(full_entry(bytes, 4), 6, 4, 8, "Lst");
    put_entry(full_entry(bytes, 5), 0, 14, 2, "End");
    put_u32(bytes, 24 + FULL_ENTRIES * 32, FULL_VALUES);
    memcpy(bytes + FULL_VALUES_OFFSET, values, FULL_VALUES);
}

/* An entry that has not been written to. */
static RegEntry untouched_reg_entry(void)
{
    RegEntry e = {
        .type = REG_FLOAT,
        .a = 0xaaaaaaaau,
        .b = 0xaaaaaaaau,
        .name = "untouched"
    };
    return e;
}

static bool reg_entry_is_untouched(const RegEntry *e)
{
    return e->type == REG_FLOAT
        && e->a == 0xaaaaaaaau
        && e->b == 0xaaaaaaaau
        && strcmp(e->name, "untouched") == 0;
}

/* Opens the full registry after one of its entries has been replaced, and
 * says whether that entry is accepted. A refused entry must leave `*out`
 * untouched. */
static bool entry_accepted(uint32_t index, uint32_t type, uint32_t a,
    uint32_t b)
{
    uint8_t bytes[FULL_SIZE];
    make_full_registry(bytes);
    put_entry(full_entry(bytes, index), type, a, b, "Changed");
    Reg r;
    RegEntry e = untouched_reg_entry();
    if (!reg_open(bytes, sizeof bytes, &r)) {
        return false;
    }
    bool ok = reg_entry_at(&r, index, &e);
    if (!ok && !reg_entry_is_untouched(&e)) {
        return true; /* a refusal that wrote to *out counts as wrong */
    }
    return ok;
}

UTEST(reg, entry_at_reads_one_entry_of_every_kind)
{
    uint8_t bytes[FULL_SIZE];
    make_full_registry(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    ASSERT_TRUE(reg_entry_at(&r, 0, &e));
    ASSERT_EQ((int)REG_SECTION, (int)e.type);
    ASSERT_EQ(1u, e.a);
    ASSERT_EQ(5u, e.b);
    ASSERT_STREQ("Sec", e.name);

    ASSERT_TRUE(reg_entry_at(&r, 1, &e));
    ASSERT_EQ((int)REG_INT, (int)e.type);
    ASSERT_EQ(0xfffffffeu, e.a);
    ASSERT_STREQ("Num", e.name);

    ASSERT_TRUE(reg_entry_at(&r, 2, &e));
    ASSERT_EQ((int)REG_FLOAT, (int)e.type);
    ASSERT_EQ(0x3f800000u, e.a);
    ASSERT_STREQ("Flt", e.name);

    ASSERT_TRUE(reg_entry_at(&r, 3, &e));
    ASSERT_EQ((int)REG_TEXT, (int)e.type);
    ASSERT_EQ(0u, e.a);
    ASSERT_EQ(4u, e.b);
    ASSERT_STREQ("Txt", e.name);

    ASSERT_TRUE(reg_entry_at(&r, 4, &e));
    ASSERT_EQ((int)REG_INT_LIST, (int)e.type);
    ASSERT_EQ(4u, e.a);
    ASSERT_EQ(8u, e.b);
    ASSERT_STREQ("Lst", e.name);

    ASSERT_TRUE(reg_entry_at(&r, 5, &e));
    ASSERT_EQ((int)REG_TEXT, (int)e.type);
    ASSERT_EQ(14u, e.a);
    ASSERT_EQ(2u, e.b);
    ASSERT_STREQ("End", e.name);
}

UTEST(reg, entry_at_past_the_last_entry_fails)
{
    uint8_t bytes[FULL_SIZE];
    make_full_registry(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    ASSERT_FALSE(reg_entry_at(&r, FULL_ENTRIES, &e));
    ASSERT_FALSE(reg_entry_at(&r, 0xffffffffu, &e));
    ASSERT_TRUE(reg_entry_is_untouched(&e));
}

UTEST(reg, entry_at_reads_a_name_that_fills_all_16_characters)
{
    uint8_t bytes[FULL_SIZE];
    make_full_registry(bytes);
    put_entry(full_entry(bytes, 1), 2, 5, 0, "0123456789abcdef");
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    ASSERT_TRUE(reg_entry_at(&r, 1, &e));

    ASSERT_STREQ("0123456789abcdef", e.name);
}

UTEST(reg, entry_at_ignores_the_unknown_first_field)
{
    uint8_t bytes[FULL_SIZE];
    make_full_registry(bytes);
    put_u32(full_entry(bytes, 1), 0, 0xffffffffu);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    ASSERT_TRUE(reg_entry_at(&r, 1, &e));

    ASSERT_EQ(0xfffffffeu, e.a);
    ASSERT_STREQ("Num", e.name);
}

UTEST(reg, entry_at_accepts_only_the_five_known_types)
{
    /* As a number or a decimal the entry has nothing to check. */
    ASSERT_TRUE(entry_accepted(1, 2, 0xffffffffu, 0xffffffffu));
    ASSERT_TRUE(entry_accepted(1, 4, 0xffffffffu, 0xffffffffu));

    ASSERT_FALSE(entry_accepted(1, 3, 0, 0));
    ASSERT_FALSE(entry_accepted(1, 5, 0, 0));
    ASSERT_FALSE(entry_accepted(1, 7, 0, 0));
    ASSERT_FALSE(entry_accepted(1, 0xffffffffu, 0, 0));
}

UTEST(reg, entry_at_checks_the_range_of_a_section)
{
    /* The table has 6 entries. */
    ASSERT_TRUE(entry_accepted(1, 1, 0, 6));
    ASSERT_TRUE(entry_accepted(1, 1, 5, 1));
    ASSERT_TRUE(entry_accepted(1, 1, 6, 0));

    ASSERT_FALSE(entry_accepted(1, 1, 7, 0));
    ASSERT_FALSE(entry_accepted(1, 1, 0, 7));
    ASSERT_FALSE(entry_accepted(1, 1, 5, 2));
    /* 2 + 0xffffffff wraps round to 1 in 32 bits. */
    ASSERT_FALSE(entry_accepted(1, 1, 2, 0xffffffffu));
}

UTEST(reg, entry_at_checks_the_range_of_a_text)
{
    /* The values area has 16 bytes. Bytes 3, 13 and 15 are zero. */
    ASSERT_TRUE(entry_accepted(1, 0, 0, 4));
    ASSERT_TRUE(entry_accepted(1, 0, 3, 1));
    ASSERT_TRUE(entry_accepted(1, 0, 15, 1));
    ASSERT_TRUE(entry_accepted(1, 0, 0, 16));

    ASSERT_FALSE(entry_accepted(1, 0, 16, 1));
    ASSERT_FALSE(entry_accepted(1, 0, 17, 0));
    ASSERT_FALSE(entry_accepted(1, 0, 0, 17));
    /* 4 + 0xfffffffc wraps round to 0 in 32 bits. */
    ASSERT_FALSE(entry_accepted(1, 0, 4, 0xfffffffcu));
}

UTEST(reg, entry_at_refuses_a_text_without_a_closing_zero)
{
    /* "abc" without its zero, and a text of no bytes at all, which has no
     * room for one. */
    ASSERT_FALSE(entry_accepted(1, 0, 0, 3));
    ASSERT_FALSE(entry_accepted(1, 0, 0, 0));
    ASSERT_FALSE(entry_accepted(1, 0, 16, 0));
    /* Bytes 12 to 14 are 'x', 0, 7: the zero is not the last byte. */
    ASSERT_FALSE(entry_accepted(1, 0, 12, 3));
}

UTEST(reg, entry_at_checks_the_range_and_size_of_a_list)
{
    ASSERT_TRUE(entry_accepted(1, 6, 4, 8));
    ASSERT_TRUE(entry_accepted(1, 6, 0, 16));
    ASSERT_TRUE(entry_accepted(1, 6, 12, 4));
    ASSERT_TRUE(entry_accepted(1, 6, 16, 0));

    ASSERT_FALSE(entry_accepted(1, 6, 4, 6));
    ASSERT_FALSE(entry_accepted(1, 6, 4, 7));
    ASSERT_FALSE(entry_accepted(1, 6, 16, 4));
    ASSERT_FALSE(entry_accepted(1, 6, 12, 8));
    ASSERT_FALSE(entry_accepted(1, 6, 17, 0));
    /* 4 + 0xfffffffc wraps round to 0 in 32 bits. */
    ASSERT_FALSE(entry_accepted(1, 6, 4, 0xfffffffcu));
}

/* ---- reg_find, reg_int, reg_text and reg_int_list ---- */

/* A registry with two sections that both have a key called "Num".
 *
 *     entry 0   section  "One"   holds entries 2 and 3
 *     entry 1   section  "Two"   holds entries 4 to 6
 *     entry 2   number   "Num"   5
 *     entry 3   text     "Txt"   "abc"          values 0 to 3
 *     entry 4   number   "Num"   -2
 *     entry 5   list     "Lst"   1, -1, 70000   values 4 to 15
 *     entry 6   list     "None"  no numbers     values 16 to 15
 */
enum {
    TWO_ENTRIES = 7,
    TWO_VALUES = 16,
    TWO_VALUES_OFFSET = 24 + TWO_ENTRIES * 32 + 4,
    TWO_SIZE = TWO_VALUES_OFFSET + TWO_VALUES
};

static void make_two_sections(uint8_t bytes[TWO_SIZE])
{
    const uint8_t values[TWO_VALUES] = {
        'a', 'b', 'c', 0,
        1, 0, 0, 0,                 /* 1 */
        0xff, 0xff, 0xff, 0xff,     /* -1 */
        0x70, 0x11, 0x01, 0x00,     /* 70000 */
    };
    memset(bytes, 0, TWO_SIZE);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_ROOT_FIRST, 0);
    put_u32(bytes, AT_ROOT_COUNT, 2);
    put_u32(bytes, AT_FLAGS, 17);
    put_u32(bytes, AT_ENTRY_COUNT, TWO_ENTRIES);
    put_entry(bytes + 24 + 0 * 32, 1, 2, 2, "One");
    put_entry(bytes + 24 + 1 * 32, 1, 4, 3, "Two");
    put_entry(bytes + 24 + 2 * 32, 2, 5, 0, "Num");
    put_entry(bytes + 24 + 3 * 32, 0, 0, 4, "Txt");
    put_entry(bytes + 24 + 4 * 32, 2, 0xfffffffeu, 0, "Num");
    put_entry(bytes + 24 + 5 * 32, 6, 4, 12, "Lst");
    put_entry(bytes + 24 + 6 * 32, 6, 16, 0, "None");
    put_u32(bytes, 24 + TWO_ENTRIES * 32, TWO_VALUES);
    memcpy(bytes + TWO_VALUES_OFFSET, values, TWO_VALUES);
}

UTEST(reg, find_gives_the_entry_of_a_key_in_its_section)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    ASSERT_TRUE(reg_find(&r, "Two", "Lst", &e));

    ASSERT_EQ((int)REG_INT_LIST, (int)e.type);
    ASSERT_EQ(4u, e.a);
    ASSERT_EQ(12u, e.b);
    ASSERT_STREQ("Lst", e.name);
}

UTEST(reg, find_of_a_missing_section_or_key_fails)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    ASSERT_FALSE(reg_find(&r, "Three", "Num", &e));
    ASSERT_FALSE(reg_find(&r, "One", "Nothing", &e));
    ASSERT_FALSE(reg_find(&r, "", "", &e));
    ASSERT_TRUE(reg_entry_is_untouched(&e));
}

UTEST(reg, find_matches_whole_names_and_capitals)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    /* The start of a name, a name with more after it, other capitals. */
    ASSERT_FALSE(reg_find(&r, "On", "Num", &e));
    ASSERT_FALSE(reg_find(&r, "One", "Nu", &e));
    ASSERT_FALSE(reg_find(&r, "One", "Numb", &e));
    ASSERT_FALSE(reg_find(&r, "one", "Num", &e));
    ASSERT_FALSE(reg_find(&r, "One", "num", &e));
    ASSERT_TRUE(reg_entry_is_untouched(&e));
}

UTEST(reg, find_looks_only_in_the_section_it_was_given)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    /* "Txt" is in One only, and "Lst" in Two only. */
    ASSERT_FALSE(reg_find(&r, "Two", "Txt", &e));
    ASSERT_FALSE(reg_find(&r, "One", "Lst", &e));
    /* A key is not a section, and a section is not a key of itself. */
    ASSERT_FALSE(reg_find(&r, "Num", "Num", &e));
    ASSERT_FALSE(reg_find(&r, "One", "One", &e));
    ASSERT_TRUE(reg_entry_is_untouched(&e));
}

UTEST(reg, find_of_a_root_entry_that_is_not_a_section_fails)
{
    /* The root entry "One" is turned into a number whose value happens to
     * be 2, where its entries used to start. */
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    put_entry(bytes + 24, 2, 2, 2, "One");
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    ASSERT_FALSE(reg_find(&r, "One", "Num", &e));
    ASSERT_TRUE(reg_entry_is_untouched(&e));
}

UTEST(reg, find_in_a_damaged_section_fails)
{
    /* An entry of a type that does not exist sits before the key. */
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    put_entry(bytes + 24 + 4 * 32, 3, 0, 0, "Broken");
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    RegEntry e = untouched_reg_entry();

    ASSERT_FALSE(reg_find(&r, "Two", "Lst", &e));
    ASSERT_TRUE(reg_entry_is_untouched(&e));
}

UTEST(reg, int_reads_the_number_of_the_right_section)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    int32_t value = 12345;

    ASSERT_TRUE(reg_int(&r, "One", "Num", &value));
    ASSERT_EQ(5, value);

    ASSERT_TRUE(reg_int(&r, "Two", "Num", &value));
    ASSERT_EQ(-2, value);
}

UTEST(reg, int_of_a_missing_key_or_another_kind_fails)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    int32_t value = 12345;

    ASSERT_FALSE(reg_int(&r, "One", "Missing", &value));
    ASSERT_FALSE(reg_int(&r, "One", "Txt", &value));
    ASSERT_FALSE(reg_int(&r, "Two", "Lst", &value));
    ASSERT_EQ(12345, value);
}

UTEST(reg, text_points_at_the_text_inside_the_registry)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    const char *text = NULL;

    ASSERT_TRUE(reg_text(&r, "One", "Txt", &text));

    ASSERT_STREQ("abc", text);
    ASSERT_TRUE((const uint8_t *)text == bytes + TWO_VALUES_OFFSET);
}

UTEST(reg, text_of_a_missing_key_or_another_kind_fails)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    const char *marker = "untouched";
    const char *text = marker;

    ASSERT_FALSE(reg_text(&r, "One", "Missing", &text));
    ASSERT_FALSE(reg_text(&r, "One", "Num", &text));
    ASSERT_FALSE(reg_text(&r, "Two", "Lst", &text));
    ASSERT_TRUE(text == marker);
}

UTEST(reg, int_list_reads_the_numbers_and_their_count)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    int32_t numbers[5] = {99, 99, 99, 99, 99};
    size_t count = 12345;

    ASSERT_TRUE(reg_int_list(&r, "Two", "Lst", numbers, 5, &count));

    ASSERT_EQ(3u, count);
    ASSERT_EQ(1, numbers[0]);
    ASSERT_EQ(-1, numbers[1]);
    ASSERT_EQ(70000, numbers[2]);
    ASSERT_EQ(99, numbers[3]); /* nothing written past the list */
    ASSERT_EQ(99, numbers[4]);
}

UTEST(reg, int_list_fits_exactly_and_fails_when_one_too_small)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    int32_t numbers[4] = {99, 99, 99, 99};
    size_t count = 12345;

    /* Room for 2 is not enough for 3. Nothing may be written. */
    ASSERT_FALSE(reg_int_list(&r, "Two", "Lst", numbers, 2, &count));
    ASSERT_EQ(99, numbers[0]);
    ASSERT_EQ(99, numbers[1]);
    ASSERT_EQ((size_t)12345, count);

    ASSERT_TRUE(reg_int_list(&r, "Two", "Lst", numbers, 3, &count));
    ASSERT_EQ(3u, count);
    ASSERT_EQ(70000, numbers[2]);
    ASSERT_EQ(99, numbers[3]);
}

UTEST(reg, int_list_with_no_numbers_gives_a_count_of_zero)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    int32_t numbers[1] = {99};
    size_t count = 12345;

    ASSERT_TRUE(reg_int_list(&r, "Two", "None", numbers, 1, &count));
    ASSERT_EQ(0u, count);
    ASSERT_EQ(99, numbers[0]);

    /* It also fits where there is room for nothing. */
    count = 12345;
    ASSERT_TRUE(reg_int_list(&r, "Two", "None", NULL, 0, &count));
    ASSERT_EQ(0u, count);
}

UTEST(reg, int_list_of_a_missing_key_or_another_kind_fails)
{
    uint8_t bytes[TWO_SIZE];
    make_two_sections(bytes);
    Reg r;
    ASSERT_TRUE(reg_open(bytes, sizeof bytes, &r));
    int32_t numbers[4] = {99, 99, 99, 99};
    size_t count = 12345;

    ASSERT_FALSE(reg_int_list(&r, "Two", "Missing", numbers, 4, &count));
    ASSERT_FALSE(reg_int_list(&r, "Two", "Num", numbers, 4, &count));
    ASSERT_FALSE(reg_int_list(&r, "One", "Txt", numbers, 4, &count));
    ASSERT_EQ(99, numbers[0]);
    ASSERT_EQ((size_t)12345, count);
}
