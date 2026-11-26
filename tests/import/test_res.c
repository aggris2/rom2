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

/* ---- res_open and res_entry_at ---- */

/* A small complete archive, built by hand:
 *
 *     bytes 0 to 23      header
 *     bytes 24 to 31     file data, 8 bytes
 *     bytes 32 to 127    table of three entries
 *
 *     entry 0   file    "a.bin"   bytes 24 to 31
 *     entry 1   folder  "dir"     holds entry 2
 *     entry 2   file    "b.bin"   bytes 28 to 31
 *
 * The root folder is entries 0 and 1. As with the header tests, the numbers
 * sit on the limits: a.bin ends exactly where the table starts, and dir
 * ends exactly at the last entry. */
enum {
    SMALL_DATA_SIZE = 8,
    SMALL_ENTRY_COUNT = 3,
    SMALL_TABLE_OFFSET = 24 + SMALL_DATA_SIZE,
    SMALL_SIZE = SMALL_TABLE_OFFSET + SMALL_ENTRY_COUNT * ENTRY_SIZE
};

static uint8_t *small_entry(uint8_t bytes[SMALL_SIZE], uint32_t index)
{
    return bytes + SMALL_TABLE_OFFSET + index * ENTRY_SIZE;
}

static void make_small_archive(uint8_t bytes[SMALL_SIZE])
{
    memset(bytes, 0, SMALL_SIZE);
    put_u32(bytes, AT_SIGNATURE, 0x31415926u);
    put_u32(bytes, AT_ROOT_FIRST, 0);
    put_u32(bytes, AT_ROOT_COUNT, 2);
    put_u32(bytes, AT_FLAGS, 1);
    put_u32(bytes, AT_TABLE_OFFSET, SMALL_TABLE_OFFSET);
    put_u32(bytes, AT_ENTRY_COUNT, SMALL_ENTRY_COUNT);
    make_entry(small_entry(bytes, 0), 24, 8, 0, "a.bin");
    make_entry(small_entry(bytes, 1), 2, 1, 1, "dir");
    make_entry(small_entry(bytes, 2), 28, 4, 0, "b.bin");
}

UTEST(res, open_keeps_the_bytes_and_the_header)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    ResArchive a = {0};

    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));

    ASSERT_TRUE(a.data == bytes);
    ASSERT_EQ((size_t)SMALL_SIZE, a.size);
    ASSERT_EQ(0u, a.header.root_first);
    ASSERT_EQ(2u, a.header.root_count);
    ASSERT_EQ((uint32_t)SMALL_TABLE_OFFSET, a.header.table_offset);
    ASSERT_EQ((uint32_t)SMALL_ENTRY_COUNT, a.header.entry_count);
}

UTEST(res, open_of_a_bad_archive_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    put_u32(bytes, AT_SIGNATURE, 0);
    ResArchive a = {.data = NULL, .size = 12345};

    ASSERT_FALSE(res_open(bytes, sizeof bytes, &a));

    ASSERT_TRUE(a.data == NULL);
    ASSERT_EQ((size_t)12345, a.size);
}

UTEST(res, entry_at_reads_a_file)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_at(&a, 0, &e));

    ASSERT_FALSE(e.is_folder);
    ASSERT_EQ(24u, e.offset);
    ASSERT_EQ(8u, e.size);
    ASSERT_STREQ("a.bin", e.name);
}

UTEST(res, entry_at_reads_a_folder)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();
    e.is_folder = false;

    ASSERT_TRUE(res_entry_at(&a, 1, &e));

    ASSERT_TRUE(e.is_folder);
    ASSERT_EQ(2u, e.offset);
    ASSERT_EQ(1u, e.size);
    ASSERT_STREQ("dir", e.name);
}

UTEST(res, entry_at_reads_the_last_entry)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_at(&a, 2, &e));

    ASSERT_STREQ("b.bin", e.name);
}

UTEST(res, entry_at_one_past_the_last_entry_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 3, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_a_huge_index_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 0xffffffffu, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_with_an_unknown_type_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 0), 24, 8, 2, "a.bin");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 0, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_accepts_an_empty_file_at_the_end_of_the_data)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 0), SMALL_TABLE_OFFSET, 0, 0, "empty");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_at(&a, 0, &e));

    ASSERT_EQ(0u, e.size);
}

UTEST(res, entry_at_file_starting_inside_the_header_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 0), 23, 1, 0, "a.bin");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 0, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_file_starting_inside_the_table_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 0), SMALL_TABLE_OFFSET + 1, 0, 0, "a.bin");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 0, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_file_running_into_the_table_fails)
{
    /* One byte more than there is between byte 24 and the table. */
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 0), 24, SMALL_DATA_SIZE + 1, 0, "a.bin");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 0, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_file_with_a_huge_size_fails)
{
    /* 28 + 0xfffffffc wraps round to 24 in 32 bits. */
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 0), 28, 0xfffffffcu, 0, "a.bin");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 0, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_accepts_an_empty_folder_at_the_end_of_the_table)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 1), SMALL_ENTRY_COUNT, 0, 1, "empty");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_entry_at(&a, 1, &e));

    ASSERT_TRUE(e.is_folder);
    ASSERT_EQ(0u, e.size);
}

UTEST(res, entry_at_folder_starting_past_the_table_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 1), SMALL_ENTRY_COUNT + 1, 0, 1, "dir");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 1, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_folder_with_one_child_too_many_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 1), 2, 2, 1, "dir");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 1, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, entry_at_folder_with_a_huge_child_count_fails)
{
    /* 2 + 0xffffffff wraps round to 1 in 32 bits. */
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    make_entry(small_entry(bytes, 1), 2, 0xffffffffu, 1, "dir");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_entry_at(&a, 1, &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

/* ---- res_file_reader and res_find ---- */

/* The small archive above, with real bytes in its data area so that a
 * file's contents can be told apart:
 *
 *     bytes 24 to 31     0x10 0x11 0x12 0x13 0x14 0x15 0x16 0x17
 *
 * a.bin is all eight of them. dir/b.bin is the last four. */
static void make_small_archive_with_data(uint8_t bytes[SMALL_SIZE])
{
    make_small_archive(bytes);
    for (uint32_t i = 0; i < SMALL_DATA_SIZE; i++) {
        bytes[24 + i] = (uint8_t)(0x10 + i);
    }
}

UTEST(res, file_reader_covers_exactly_the_file)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e;
    ASSERT_TRUE(res_entry_at(&a, 2, &e));

    Reader r = res_file_reader(&a, &e);

    ASSERT_EQ(4u, reader_remaining(&r));
    ASSERT_EQ(0x14, reader_u8(&r));
    ASSERT_EQ(0x15, reader_u8(&r));
    ASSERT_EQ(0x16, reader_u8(&r));
    ASSERT_EQ(0x17, reader_u8(&r));
    ASSERT_TRUE(reader_ok(&r));
    ASSERT_EQ(0, reader_u8(&r));
    ASSERT_FALSE(reader_ok(&r));
}

UTEST(res, file_reader_of_a_folder_is_failed)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e;
    ASSERT_TRUE(res_entry_at(&a, 1, &e));

    Reader r = res_file_reader(&a, &e);

    ASSERT_FALSE(reader_ok(&r));
    ASSERT_EQ(0u, reader_remaining(&r));
}

UTEST(res, find_a_file_in_the_root)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_find(&a, "a.bin", &e));

    ASSERT_FALSE(e.is_folder);
    ASSERT_EQ(24u, e.offset);
    ASSERT_EQ(8u, e.size);
    ASSERT_STREQ("a.bin", e.name);
}

UTEST(res, find_a_file_inside_a_folder)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_find(&a, "dir/b.bin", &e));

    ASSERT_FALSE(e.is_folder);
    ASSERT_EQ(28u, e.offset);
    ASSERT_EQ(4u, e.size);
    ASSERT_STREQ("b.bin", e.name);
}

UTEST(res, find_a_folder)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();
    e.is_folder = false;

    ASSERT_TRUE(res_find(&a, "dir", &e));

    ASSERT_TRUE(e.is_folder);
    ASSERT_STREQ("dir", e.name);
}

UTEST(res, find_then_read_gives_the_bytes_of_the_file)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e;
    ASSERT_TRUE(res_find(&a, "dir/b.bin", &e));

    Reader r = res_file_reader(&a, &e);

    ASSERT_EQ(0x17161514u, reader_u32(&r));
    ASSERT_EQ(0u, reader_remaining(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(res, find_a_missing_name_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_find(&a, "c.bin", &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, find_does_not_match_the_start_of_a_longer_name)
{
    /* "a.bi" is the beginning of "a.bin" but is not an entry. */
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_find(&a, "a.bi", &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, find_does_not_match_a_name_with_more_after_it)
{
    /* "a.bin2" begins with the entry "a.bin" but is not that entry. */
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_find(&a, "a.bin2", &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, find_does_not_look_in_other_folders)
{
    /* b.bin exists, but inside dir, not in the root. */
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_find(&a, "b.bin", &e));
    ASSERT_FALSE(res_find(&a, "dir/a.bin", &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, find_through_a_file_fails)
{
    /* a.bin is a file, so nothing can be inside it. */
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_find(&a, "a.bin/b.bin", &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, find_with_an_empty_or_odd_path_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_find(&a, "", &e));
    ASSERT_FALSE(res_find(&a, "/", &e));
    ASSERT_FALSE(res_find(&a, "/a.bin", &e));
    ASSERT_FALSE(res_find(&a, "dir/", &e));
    ASSERT_FALSE(res_find(&a, "dir//b.bin", &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, find_with_a_name_longer_than_16_fails)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_find(&a, "a.bin_and_seventeen", &e));
    ASSERT_FALSE(res_find(&a, "dir/b.bin_and_seventeen", &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

UTEST(res, find_a_name_that_fills_all_16_characters)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    make_entry(small_entry(bytes, 0), 24, 8, 0, "0123456789abcdef");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_TRUE(res_find(&a, "0123456789abcdef", &e));

    ASSERT_STREQ("0123456789abcdef", e.name);
}

UTEST(res, find_in_a_damaged_folder_fails)
{
    /* The second root entry has a type that is neither file nor folder,
     * so the search cannot get past it. */
    uint8_t bytes[SMALL_SIZE];
    make_small_archive_with_data(bytes);
    make_entry(small_entry(bytes, 0), 24, 8, 2, "broken");
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    ResEntry e = untouched_entry();

    ASSERT_FALSE(res_find(&a, "dir", &e));
    ASSERT_TRUE(entry_is_untouched(&e));
}

/* ---- noting the files that are read ---- */

UTEST(res, open_notes_nothing_until_it_is_given_a_place)
{
    uint8_t bytes[SMALL_SIZE];
    make_small_archive(bytes);
    ResArchive a;
    memset(&a, 0xff, sizeof a);
    ResEntry e;

    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));

    ASSERT_TRUE(a.found == NULL);
    ASSERT_EQ(1, a.mark);
    /* Finding and noting are safe with no place to note in. */
    ASSERT_TRUE(res_find(&a, "a.bin", &e));
    res_note(&a, 0);
}

UTEST(res, find_notes_the_file_it_finds_with_the_mark)
{
    uint8_t bytes[SMALL_SIZE];
    uint8_t found[SMALL_ENTRY_COUNT] = {0};
    make_small_archive(bytes);
    ResArchive a;
    ResEntry e;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    a.found = found;
    a.mark = 7;

    ASSERT_TRUE(res_find(&a, "dir/b.bin", &e));

    /* Entry 2 is dir/b.bin. The folder it is in is not a file read. */
    ASSERT_EQ(0, found[0]);
    ASSERT_EQ(0, found[1]);
    ASSERT_EQ(7, found[2]);
}

UTEST(res, find_notes_nothing_for_a_folder_or_a_miss)
{
    uint8_t bytes[SMALL_SIZE];
    uint8_t found[SMALL_ENTRY_COUNT] = {0};
    make_small_archive(bytes);
    ResArchive a;
    ResEntry e;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    a.found = found;

    ASSERT_TRUE(res_find(&a, "dir", &e));
    ASSERT_FALSE(res_find(&a, "b.bin", &e));
    ASSERT_FALSE(res_find(&a, "dir/a.bin", &e));

    ASSERT_EQ(0, found[0]);
    ASSERT_EQ(0, found[1]);
    ASSERT_EQ(0, found[2]);
}

UTEST(res, note_marks_one_entry_and_leaves_alone_what_is_past_the_table)
{
    /* One byte more than the table has entries, to see it kept. */
    uint8_t bytes[SMALL_SIZE];
    uint8_t found[SMALL_ENTRY_COUNT + 1] = {0};
    make_small_archive(bytes);
    ResArchive a;
    ASSERT_TRUE(res_open(bytes, sizeof bytes, &a));
    a.found = found;
    a.mark = 3;

    res_note(&a, 0);
    res_note(&a, SMALL_ENTRY_COUNT);
    res_note(&a, 0xffffffffu);

    ASSERT_EQ(3, found[0]);
    ASSERT_EQ(0, found[1]);
    ASSERT_EQ(0, found[2]);
    ASSERT_EQ(0, found[SMALL_ENTRY_COUNT]);
}
