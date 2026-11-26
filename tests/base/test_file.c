#include "base/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utest.h"

/* These tests write a small file into the folder they are run from, read
 * it back and delete it again. */
static const char *const TEST_PATH = "rom2_test_file.tmp";

static bool write_test_file(const uint8_t *bytes, size_t size)
{
    FILE *f = fopen(TEST_PATH, "wb");
    if (f == NULL) {
        return false;
    }
    size_t written = 0;
    if (size > 0) {
        written = fwrite(bytes, 1, size, f);
    }
    return fclose(f) == 0 && written == size;
}

/* ---- file_read_all ---- */

UTEST(file, read_all_returns_the_bytes_of_the_file)
{
    const uint8_t bytes[] = {0x26, 0x59, 0x41, 0x31, 0x37};
    ASSERT_TRUE(write_test_file(bytes, sizeof bytes));
    uint8_t *data = NULL;
    size_t size = 0;

    bool ok = file_read_all(TEST_PATH, &data, &size);
    remove(TEST_PATH);

    ASSERT_TRUE(ok);
    ASSERT_EQ(sizeof bytes, size);
    ASSERT_EQ(0x26, data[0]);
    ASSERT_EQ(0x59, data[1]);
    ASSERT_EQ(0x41, data[2]);
    ASSERT_EQ(0x31, data[3]);
    ASSERT_EQ(0x37, data[4]);
    free(data);
}

UTEST(file, read_all_does_not_change_any_byte)
{
    /* Zero, line feed, carriage return and end-of-file marker: the bytes
     * that get changed or cut off when a file is opened as text instead of
     * as binary. */
    const uint8_t bytes[] = {0x00, 0x0a, 0x0d, 0x0a, 0x1a, 0xff, 0x00};
    ASSERT_TRUE(write_test_file(bytes, sizeof bytes));
    uint8_t *data = NULL;
    size_t size = 0;

    bool ok = file_read_all(TEST_PATH, &data, &size);
    remove(TEST_PATH);

    ASSERT_TRUE(ok);
    ASSERT_EQ(sizeof bytes, size);
    for (size_t i = 0; i < sizeof bytes; i++) {
        ASSERT_EQ(bytes[i], data[i]);
    }
    free(data);
}

UTEST(file, read_all_of_an_empty_file_gives_no_bytes)
{
    ASSERT_TRUE(write_test_file(NULL, 0));
    uint8_t *data = NULL;
    size_t size = 12345;

    bool ok = file_read_all(TEST_PATH, &data, &size);
    remove(TEST_PATH);

    ASSERT_TRUE(ok);
    ASSERT_EQ(0u, size);
    ASSERT_TRUE(data != NULL);
    free(data);
}

UTEST(file, read_all_of_a_missing_file_fails)
{
    uint8_t marker = 0;
    uint8_t *data = &marker;
    size_t size = 12345;
    remove(TEST_PATH);

    ASSERT_FALSE(file_read_all(TEST_PATH, &data, &size));

    ASSERT_TRUE(data == &marker);
    ASSERT_EQ((size_t)12345, size);
}

/* ---- file_write_all ---- */

UTEST(file, write_all_then_read_all_gives_the_same_bytes)
{
    const uint8_t bytes[] = {0x00, 0x0a, 0x0d, 0x0a, 0x1a, 0xff, 0x89, 0x50};
    uint8_t *data = NULL;
    size_t size = 0;

    bool written = file_write_all(TEST_PATH, bytes, sizeof bytes);
    bool read = file_read_all(TEST_PATH, &data, &size);
    remove(TEST_PATH);

    ASSERT_TRUE(written);
    ASSERT_TRUE(read);
    ASSERT_EQ(sizeof bytes, size);
    for (size_t i = 0; i < sizeof bytes; i++) {
        ASSERT_EQ(bytes[i], data[i]);
    }
    free(data);
}

UTEST(file, write_all_replaces_an_existing_file)
{
    const uint8_t longer[] = {1, 2, 3, 4, 5, 6};
    const uint8_t shorter[] = {9, 8};
    uint8_t *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(file_write_all(TEST_PATH, longer, sizeof longer));

    bool written = file_write_all(TEST_PATH, shorter, sizeof shorter);
    bool read = file_read_all(TEST_PATH, &data, &size);
    remove(TEST_PATH);

    ASSERT_TRUE(written);
    ASSERT_TRUE(read);
    ASSERT_EQ(2u, size);
    ASSERT_EQ(9, data[0]);
    ASSERT_EQ(8, data[1]);
    free(data);
}

UTEST(file, write_all_of_no_bytes_makes_an_empty_file)
{
    uint8_t *data = NULL;
    size_t size = 12345;

    bool written = file_write_all(TEST_PATH, NULL, 0);
    bool read = file_read_all(TEST_PATH, &data, &size);
    remove(TEST_PATH);

    ASSERT_TRUE(written);
    ASSERT_TRUE(read);
    ASSERT_EQ(0u, size);
    free(data);
}

UTEST(file, write_all_into_a_missing_folder_fails)
{
    const uint8_t bytes[] = {1};

    ASSERT_FALSE(file_write_all("rom2_no_such_folder/file.tmp", bytes, 1));
}

/* ---- file_make_folder ---- */

/* The test folder is removed again, so a test run leaves nothing behind.
 * The C library has no call for that, so this asks the system. */
#ifdef _WIN32
#include <direct.h>
#define remove_folder _rmdir
#else
#include <unistd.h>
#define remove_folder rmdir
#endif

UTEST(file, make_folder_makes_a_folder_a_file_can_be_written_in)
{
    const uint8_t bytes[] = {1, 2, 3};
    remove("rom2_test_folder/file.tmp");

    ASSERT_TRUE(file_make_folder("rom2_test_folder"));
    ASSERT_TRUE(file_write_all("rom2_test_folder/file.tmp", bytes, 3));

    remove("rom2_test_folder/file.tmp");
    remove_folder("rom2_test_folder");
}

UTEST(file, make_folder_of_an_existing_folder_succeeds)
{
    ASSERT_TRUE(file_make_folder("rom2_test_folder"));
    ASSERT_TRUE(file_make_folder("rom2_test_folder"));
    remove_folder("rom2_test_folder");
}

UTEST(file, make_folder_inside_a_missing_folder_fails)
{
    ASSERT_FALSE(file_make_folder("rom2_no_such_folder/inside"));
}

/* ---- file_list_folder ---- */

/* Says whether `name` is one of the `count` names in the block. */
static bool lists(const char *names, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(names, name) == 0) {
            return true;
        }
        names += strlen(names) + 1;
    }
    return false;
}

UTEST(file, list_folder_gives_the_files_and_the_folders_in_it)
{
    const uint8_t bytes[] = {1};
    char *names = NULL;
    size_t count = 0;
    ASSERT_TRUE(file_make_folder("rom2_test_list"));
    ASSERT_TRUE(file_make_folder("rom2_test_list/inner"));
    ASSERT_TRUE(file_write_all("rom2_test_list/a.tmp", bytes, 1));
    ASSERT_TRUE(file_write_all("rom2_test_list/b.tmp", bytes, 1));

    ASSERT_TRUE(file_list_folder("rom2_test_list", &names, &count));

    /* Three names, and neither "." nor "..". */
    ASSERT_EQ(3u, (unsigned)count);
    ASSERT_TRUE(lists(names, count, "a.tmp"));
    ASSERT_TRUE(lists(names, count, "b.tmp"));
    ASSERT_TRUE(lists(names, count, "inner"));
    free(names);
    remove("rom2_test_list/a.tmp");
    remove("rom2_test_list/b.tmp");
    remove_folder("rom2_test_list/inner");
    remove_folder("rom2_test_list");
}

UTEST(file, list_folder_of_an_empty_folder_gives_no_names)
{
    char *names = NULL;
    size_t count = 99;
    ASSERT_TRUE(file_make_folder("rom2_test_empty"));

    ASSERT_TRUE(file_list_folder("rom2_test_empty", &names, &count));

    ASSERT_EQ(0u, (unsigned)count);
    ASSERT_TRUE(names != NULL);
    free(names);
    remove_folder("rom2_test_empty");
}

UTEST(file, list_folder_holds_many_long_names)
{
    /* Enough names to make the block grow more than once. */
    const uint8_t bytes[] = {1};
    char path[128];
    char *names = NULL;
    size_t count = 0;
    ASSERT_TRUE(file_make_folder("rom2_test_many"));
    for (int i = 0; i < 40; i++) {
        snprintf(path, sizeof path, "rom2_test_many/a_rather_long_name_"
            "to_fill_the_block_sooner_%02d.tmp", i);
        ASSERT_TRUE(file_write_all(path, bytes, 1));
    }

    ASSERT_TRUE(file_list_folder("rom2_test_many", &names, &count));

    ASSERT_EQ(40u, (unsigned)count);
    ASSERT_TRUE(lists(names, count,
        "a_rather_long_name_to_fill_the_block_sooner_00.tmp"));
    ASSERT_TRUE(lists(names, count,
        "a_rather_long_name_to_fill_the_block_sooner_39.tmp"));
    free(names);
    for (int i = 0; i < 40; i++) {
        snprintf(path, sizeof path, "rom2_test_many/a_rather_long_name_"
            "to_fill_the_block_sooner_%02d.tmp", i);
        remove(path);
    }
    remove_folder("rom2_test_many");
}

UTEST(file, list_folder_of_a_missing_folder_fails)
{
    char *names = (char *)1;
    size_t count = 99;

    ASSERT_FALSE(file_list_folder("rom2_no_such_folder", &names, &count));

    ASSERT_TRUE(names == (char *)1);
    ASSERT_EQ(99u, (unsigned)count);
}
