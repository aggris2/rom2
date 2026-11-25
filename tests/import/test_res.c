#include "import/res.h"

#include <string.h>

#include "utest.h"

/* A small valid archive, built by hand: the 24-byte header and a table of
 * two entries directly after it. There are no files in it, and the entries
 * themselves are left as zeros, because these tests only read the header.
 *
 * Its numbers sit exactly on the limits the header check allows: the table
 * ends at the last byte, and the root folder ends at the last entry. Each
 * failure test below moves one number a single step past its limit. */
enum {
    ARCHIVE_SIZE = 24 + 2 * 32,

    /* Where each header field starts. */
    AT_SIGNATURE = 0,
    AT_ROOT_FIRST = 4,
    AT_ROOT_COUNT = 8,
    AT_FLAGS = 12,
    AT_TABLE_OFFSET = 16,
    AT_ENTRY_COUNT = 20
};

static void put_u32(uint8_t *bytes, size_t at, uint32_t value)
{
    bytes[at] = (uint8_t)(value & 0xff);
    bytes[at + 1] = (uint8_t)((value >> 8) & 0xff);
    bytes[at + 2] = (uint8_t)((value >> 16) & 0xff);
    bytes[at + 3] = (uint8_t)((value >> 24) & 0xff);
}

static void make_valid_archive(uint8_t bytes[ARCHIVE_SIZE])
{
    memset(bytes, 0, ARCHIVE_SIZE);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_ROOT_FIRST, 1);
    put_u32(bytes, AT_ROOT_COUNT, 1);
    put_u32(bytes, AT_FLAGS, 17);
    put_u32(bytes, AT_TABLE_OFFSET, 24);
    put_u32(bytes, AT_ENTRY_COUNT, 2);
}

/* A header that has not been written to. A failed read must leave it
 * exactly like this. */
static ResHeader untouched_header(void)
{
    ResHeader h = {
        .root_first = 0xaaaaaaaau,
        .root_count = 0xaaaaaaaau,
        .flags = 0xaaaaaaaau,
        .table_offset = 0xaaaaaaaau,
        .entry_count = 0xaaaaaaaau
    };
    return h;
}

static bool header_is_untouched(const ResHeader *h)
{
    return h->root_first == 0xaaaaaaaau
        && h->root_count == 0xaaaaaaaau
        && h->flags == 0xaaaaaaaau
        && h->table_offset == 0xaaaaaaaau
        && h->entry_count == 0xaaaaaaaau;
}

/* ---- res_header_read ---- */

UTEST(res, header_of_a_valid_archive_is_read)
{
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_TRUE(res_header_read(&r, &h));

    ASSERT_EQ(1u, h.root_first);
    ASSERT_EQ(1u, h.root_count);
    ASSERT_EQ(17u, h.flags);
    ASSERT_EQ(24u, h.table_offset);
    ASSERT_EQ(2u, h.entry_count);
}

UTEST(res, header_fields_are_not_mixed_up)
{
    /* Five different values, so that two swapped fields would show. The
     * archive is big enough for them: 9 entries starting at byte 30. */
    uint8_t bytes[30 + 9 * 32];
    memset(bytes, 0, sizeof bytes);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_ROOT_FIRST, 5);
    put_u32(bytes, AT_ROOT_COUNT, 3);
    put_u32(bytes, AT_FLAGS, 1);
    put_u32(bytes, AT_TABLE_OFFSET, 30);
    put_u32(bytes, AT_ENTRY_COUNT, 9);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_TRUE(res_header_read(&r, &h));

    ASSERT_EQ(5u, h.root_first);
    ASSERT_EQ(3u, h.root_count);
    ASSERT_EQ(1u, h.flags);
    ASSERT_EQ(30u, h.table_offset);
    ASSERT_EQ(9u, h.entry_count);
}

UTEST(res, header_of_an_archive_with_no_entries_is_read)
{
    /* The smallest valid archive: a header and nothing else. The empty
     * table starts where the archive ends. */
    uint8_t bytes[24];
    memset(bytes, 0, sizeof bytes);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_FLAGS, 1);
    put_u32(bytes, AT_TABLE_OFFSET, 24);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_TRUE(res_header_read(&r, &h));

    ASSERT_EQ(0u, h.root_count);
    ASSERT_EQ(24u, h.table_offset);
    ASSERT_EQ(0u, h.entry_count);
}

UTEST(res, header_of_an_empty_archive_fails)
{
    Reader r = reader_init(NULL, 0);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_shorter_than_24_bytes_fails)
{
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    Reader r = reader_init(bytes, 23);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_wrong_signature_fails)
{
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    put_u32(bytes, AT_SIGNATURE, 0x31415927u);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_table_inside_the_header_fails)
{
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    put_u32(bytes, AT_TABLE_OFFSET, 23);
    put_u32(bytes, AT_ENTRY_COUNT, 0);
    put_u32(bytes, AT_ROOT_FIRST, 0);
    put_u32(bytes, AT_ROOT_COUNT, 0);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_table_past_the_end_fails)
{
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    put_u32(bytes, AT_TABLE_OFFSET, ARCHIVE_SIZE + 1);
    put_u32(bytes, AT_ENTRY_COUNT, 0);
    put_u32(bytes, AT_ROOT_FIRST, 0);
    put_u32(bytes, AT_ROOT_COUNT, 0);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_one_entry_too_many_fails)
{
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    put_u32(bytes, AT_ENTRY_COUNT, 3);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_a_table_that_ends_mid_entry_fails)
{
    /* One byte short of the second entry. */
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    Reader r = reader_init(bytes, ARCHIVE_SIZE - 1);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_a_huge_entry_count_fails)
{
    /* 0x08000002 * 32 wraps round to 64 in 32 bits, which is exactly the
     * space this archive has for its table. */
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    put_u32(bytes, AT_ENTRY_COUNT, 0x08000002u);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_root_starting_past_the_table_fails)
{
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    put_u32(bytes, AT_ROOT_FIRST, 3);
    put_u32(bytes, AT_ROOT_COUNT, 0);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_root_running_past_the_table_fails)
{
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    put_u32(bytes, AT_ROOT_COUNT, 2);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

UTEST(res, header_with_a_huge_root_count_fails)
{
    /* root_first + root_count wraps round to 1 in 32 bits. */
    uint8_t bytes[ARCHIVE_SIZE];
    make_valid_archive(bytes);
    put_u32(bytes, AT_ROOT_FIRST, 2);
    put_u32(bytes, AT_ROOT_COUNT, 0xffffffffu);
    Reader r = reader_init(bytes, sizeof bytes);
    ResHeader h = untouched_header();

    ASSERT_FALSE(res_header_read(&r, &h));
    ASSERT_TRUE(header_is_untouched(&h));
}

/* ---- res_entry_read ---- */

enum {
    ENTRY_SIZE = 32,

    /* Where each entry field starts. */
    AT_ENTRY_UNKNOWN = 0,
    AT_ENTRY_OFFSET = 4,
    AT_ENTRY_SIZE = 8,
    AT_ENTRY_TYPE = 12,
    AT_ENTRY_NAME = 16
};

/* Builds one table entry. `name` is copied without its closing zero, and
 * the rest of the 16-byte name field is filled with zeros. */
static void make_entry(uint8_t bytes[ENTRY_SIZE], uint32_t offset,
                       uint32_t size, uint32_t type, const char *name)
{
    memset(bytes, 0, ENTRY_SIZE);
    put_u32(bytes, AT_ENTRY_OFFSET, offset);
    put_u32(bytes, AT_ENTRY_SIZE, size);
    put_u32(bytes, AT_ENTRY_TYPE, type);
    memcpy(bytes + AT_ENTRY_NAME, name, strlen(name));
}

/* An entry that has not been written to. A failed read must leave it
 * exactly like this. */
static ResEntry untouched_entry(void)
{
    ResEntry e = {
        .is_folder = true,
        .offset = 0xaaaaaaaau,
        .size = 0xaaaaaaaau,
        .name = "untouched"
    };
    return e;
}

static bool entry_is_untouched(const ResEntry *e)
{
    return e->is_folder
        && e->offset == 0xaaaaaaaau
        && e->size == 0xaaaaaaaau
        && strcmp(e->name, "untouched") == 0;
}

UTEST(res, entry_of_a_file_is_read)
{
    uint8_t bytes[ENTRY_SIZE];
    make_entry(bytes, 24, 80172, 0, "objects.reg");
    Reader r = reader_init(bytes, sizeof bytes);
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_read(&r, &e));

    ASSERT_FALSE(e.is_folder);
    ASSERT_EQ(24u, e.offset);
    ASSERT_EQ(80172u, e.size);
    ASSERT_STREQ("objects.reg", e.name);
}

UTEST(res, entry_of_a_folder_is_read)
{
    uint8_t bytes[ENTRY_SIZE];
    make_entry(bytes, 977, 6, 1, "units");
    Reader r = reader_init(bytes, sizeof bytes);
    ResEntry e = untouched_entry();
    e.is_folder = false;

    ASSERT_TRUE(res_entry_read(&r, &e));

    ASSERT_TRUE(e.is_folder);
    ASSERT_EQ(977u, e.offset);
    ASSERT_EQ(6u, e.size);
    ASSERT_STREQ("units", e.name);
}

UTEST(res, entry_read_moves_past_the_entry)
{
    /* Two entries one after the other, as they sit in the table. */
    uint8_t bytes[2 * ENTRY_SIZE];
    make_entry(bytes, 1, 2, 0, "first");
    make_entry(bytes + ENTRY_SIZE, 3, 4, 1, "second");
    Reader r = reader_init(bytes, sizeof bytes);
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_read(&r, &e));
    ASSERT_STREQ("first", e.name);
    ASSERT_TRUE(res_entry_read(&r, &e));
    ASSERT_STREQ("second", e.name);
    ASSERT_EQ(0u, reader_remaining(&r));
}

UTEST(res, entry_ignores_the_unknown_first_field)
{
    uint8_t bytes[ENTRY_SIZE];
    make_entry(bytes, 24, 100, 0, "a.bmp");
    put_u32(bytes, AT_ENTRY_UNKNOWN, 0xffffffffu);
    Reader r = reader_init(bytes, sizeof bytes);
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_read(&r, &e));

    ASSERT_EQ(24u, e.offset);
    ASSERT_EQ(100u, e.size);
    ASSERT_STREQ("a.bmp", e.name);
}

UTEST(res, entry_name_stops_at_the_first_zero)
{
    /* Some original archives have leftover bytes after the zero. */
    uint8_t bytes[ENTRY_SIZE];
    make_entry(bytes, 24, 100, 0, "pine1");
    memcpy(bytes + AT_ENTRY_NAME + 6, "leftover", 8);
    Reader r = reader_init(bytes, sizeof bytes);
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_read(&r, &e));

    ASSERT_STREQ("pine1", e.name);
}

UTEST(res, entry_name_may_fill_the_whole_field)
{
    /* 16 characters and no zero byte in the field at all. */
    uint8_t bytes[ENTRY_SIZE];
    make_entry(bytes, 24, 100, 0, "0123456789abcdef");
    Reader r = reader_init(bytes, sizeof bytes);
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_read(&r, &e));

    ASSERT_STREQ("0123456789abcdef", e.name);
}

UTEST(res, entry_name_may_be_empty)
{
    uint8_t bytes[ENTRY_SIZE];
    make_entry(bytes, 24, 100, 0, "");
    Reader r = reader_init(bytes, sizeof bytes);
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_read(&r, &e));

    ASSERT_STREQ("", e.name);
}

UTEST(res, entry_shorter_than_32_bytes_fails)
{
    uint8_t bytes[ENTRY_SIZE];
    make_entry(bytes, 24, 100, 0, "a.bmp");
    Reader r = reader_init(bytes, ENTRY_SIZE - 1);
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_read(&r, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_with_an_unknown_type_fails)
{
    uint8_t bytes[ENTRY_SIZE];
    make_entry(bytes, 24, 100, 2, "a.bmp");
    Reader r = reader_init(bytes, sizeof bytes);
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_read(&r, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}
