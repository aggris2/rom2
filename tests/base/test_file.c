#include "base/file.h"

#include <stdio.h>
#include <stdlib.h>

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
