#include "import/item_names.h"

#include <string.h>

#include "utest.h"

/* Three items: common bronze bracers, which are material 1, slot 9,
 * class 0, place 20, and so the number 0x1914; a magic item, slot 14
 * and place 1, the number 0x0e01; and material 15, slot 15, class 7,
 * place 31, the number 0xffff. */
static const uint8_t NUMBERS[] = {0x14, 0x19, 0x01, 0x0e, 0xff, 0xff};
static const char SERVER[] =
    "Common Bronze Bracers\r\nBook Air\r\nLast One\r\n";
static const char SHOWN[] = "Bronze Bracers\r\nBook\r\nThe Last\r\n";

static bool open_three(ItemNames *out)
{
    return item_names_open(NUMBERS, sizeof NUMBERS,
        (const uint8_t *)SERVER, sizeof SERVER - 1,
        (const uint8_t *)SHOWN, sizeof SHOWN - 1, out);
}

UTEST(item_names, shown_finds_an_item_by_its_four_numbers)
{
    ItemNames names;
    char name[ITEM_NAME_SIZE];
    ASSERT_TRUE(open_three(&names));
    ASSERT_EQ(3u, names.count);

    ASSERT_TRUE(item_names_shown(&names, 1, 9, 0, 20, name));
    ASSERT_STREQ("Bronze Bracers", name);
    ASSERT_TRUE(item_names_shown(&names, 0, 14, 0, 1, name));
    ASSERT_STREQ("Book", name);
    ASSERT_TRUE(item_names_shown(&names, 15, 15, 7, 31, name));
    ASSERT_STREQ("The Last", name);
}

UTEST(item_names, shown_of_an_item_that_is_not_there_fails)
{
    ItemNames names;
    char name[ITEM_NAME_SIZE] = "untouched";
    ASSERT_TRUE(open_three(&names));

    /* Each number one off in turn. */
    ASSERT_FALSE(item_names_shown(&names, 2, 9, 0, 20, name));
    ASSERT_FALSE(item_names_shown(&names, 1, 8, 0, 20, name));
    ASSERT_FALSE(item_names_shown(&names, 1, 9, 1, 20, name));
    ASSERT_FALSE(item_names_shown(&names, 1, 9, 0, 21, name));
    ASSERT_STREQ("untouched", name);
}

UTEST(item_names, shown_refuses_a_number_that_does_not_fit_its_bits)
{
    /* 16 as a material would run into nothing, and 32 as a place into
     * the class: place 32 of class 0 is not place 0 of class 1. */
    const uint8_t numbers[] = {0x20, 0x00};
    ItemNames names;
    char name[ITEM_NAME_SIZE] = "untouched";
    ASSERT_TRUE(item_names_open(numbers, sizeof numbers,
        (const uint8_t *)"a\n", 2, (const uint8_t *)"b\n", 2, &names));
    ASSERT_TRUE(item_names_shown(&names, 0, 0, 1, 0, name));
    strcpy(name, "untouched");

    ASSERT_FALSE(item_names_shown(&names, 0, 0, 0, 32, name));
    ASSERT_FALSE(item_names_shown(&names, 16, 0, 1, 0, name));
    ASSERT_FALSE(item_names_shown(&names, 0, 16, 1, 0, name));
    ASSERT_FALSE(item_names_shown(&names, 0, 0, 8, 0, name));
    ASSERT_FALSE(item_names_shown(&names, -1, 0, 1, 0, name));
    ASSERT_FALSE(item_names_shown(&names, 0, 0, 1, -1, name));
    ASSERT_STREQ("untouched", name);
}

UTEST(item_names, shown_for_finds_an_item_by_the_servers_name)
{
    ItemNames names;
    char name[ITEM_NAME_SIZE] = "untouched";
    ASSERT_TRUE(open_three(&names));

    ASSERT_TRUE(item_names_shown_for(&names, "Book Air", name));
    ASSERT_STREQ("Book", name);
    ASSERT_TRUE(item_names_shown_for(&names, "Last One", name));
    ASSERT_STREQ("The Last", name);

    /* The whole name, not its start or more than it. */
    strcpy(name, "untouched");
    ASSERT_FALSE(item_names_shown_for(&names, "Book", name));
    ASSERT_FALSE(item_names_shown_for(&names, "Book Air ", name));
    ASSERT_FALSE(item_names_shown_for(&names, "", name));
    ASSERT_STREQ("untouched", name);
}

UTEST(item_names, lines_may_end_plainly_and_the_last_without_an_end)
{
    const uint8_t numbers[] = {1, 0, 2, 0};
    const char server[] = "one\ntwo";
    const char shown[] = "First\nSecond";
    ItemNames names;
    char name[ITEM_NAME_SIZE];
    ASSERT_TRUE(item_names_open(numbers, sizeof numbers,
        (const uint8_t *)server, sizeof server - 1,
        (const uint8_t *)shown, sizeof shown - 1, &names));

    ASSERT_TRUE(item_names_shown(&names, 0, 0, 0, 2, name));
    ASSERT_STREQ("Second", name);
    ASSERT_TRUE(item_names_shown_for(&names, "two", name));
    ASSERT_STREQ("Second", name);
}

UTEST(item_names, a_name_is_given_as_utf8)
{
    /* 0xc0 is the Cyrillic A in the original's code page. */
    const uint8_t numbers[] = {1, 0};
    const uint8_t shown[] = {'a', 0xc0, '\r', '\n'};
    ItemNames names;
    char name[ITEM_NAME_SIZE];
    ASSERT_TRUE(item_names_open(numbers, sizeof numbers,
        (const uint8_t *)"x\n", 2, shown, sizeof shown, &names));

    ASSERT_TRUE(item_names_shown(&names, 0, 0, 0, 1, name));
    ASSERT_STREQ("a\xd0\x90", name);
}

UTEST(item_names, an_empty_line_is_an_empty_name)
{
    const uint8_t numbers[] = {1, 0, 2, 0};
    ItemNames names;
    char name[ITEM_NAME_SIZE] = "untouched";
    ASSERT_TRUE(item_names_open(numbers, sizeof numbers,
        (const uint8_t *)"a\nb\n", 4, (const uint8_t *)"\r\nB\r\n", 5,
        &names));

    ASSERT_TRUE(item_names_shown(&names, 0, 0, 0, 1, name));
    ASSERT_STREQ("", name);
}

UTEST(item_names, open_refuses_files_that_do_not_fit_each_other)
{
    ItemNames names;
    memset(&names, 0x5a, sizeof names);
    ItemNames before = names;

    /* Half a number, and a text with a line too few. */
    ASSERT_FALSE(item_names_open(NUMBERS, sizeof NUMBERS - 1,
        (const uint8_t *)SERVER, sizeof SERVER - 1,
        (const uint8_t *)SHOWN, sizeof SHOWN - 1, &names));
    ASSERT_FALSE(item_names_open(NUMBERS, sizeof NUMBERS,
        (const uint8_t *)"a\nb\n", 4,
        (const uint8_t *)SHOWN, sizeof SHOWN - 1, &names));
    ASSERT_FALSE(item_names_open(NUMBERS, sizeof NUMBERS,
        (const uint8_t *)SERVER, sizeof SERVER - 1,
        (const uint8_t *)"a\nb\n", 4, &names));
    ASSERT_EQ(0, memcmp(&names, &before, sizeof names));

    /* No items at all is no fault. */
    ASSERT_TRUE(item_names_open(NUMBERS, 0, NULL, 0, NULL, 0, &names));
    ASSERT_EQ(0u, names.count);
}

UTEST(item_names, a_name_too_long_to_hold_fails)
{
    static char shown[ITEM_NAME_SIZE + 8];
    const uint8_t numbers[] = {1, 0};
    ItemNames names;
    char name[ITEM_NAME_SIZE] = "untouched";
    /* One character short of the room is the longest that fits. */
    memset(shown, 'x', ITEM_NAME_SIZE - 1);
    shown[ITEM_NAME_SIZE - 1] = '\n';
    ASSERT_TRUE(item_names_open(numbers, sizeof numbers,
        (const uint8_t *)"a\n", 2, (const uint8_t *)shown, ITEM_NAME_SIZE,
        &names));
    ASSERT_TRUE(item_names_shown(&names, 0, 0, 0, 1, name));
    ASSERT_EQ(ITEM_NAME_SIZE - 1u, (unsigned)strlen(name));

    strcpy(name, "untouched");
    memset(shown, 'x', ITEM_NAME_SIZE);
    shown[ITEM_NAME_SIZE] = '\n';
    ASSERT_TRUE(item_names_open(numbers, sizeof numbers,
        (const uint8_t *)"a\n", 2, (const uint8_t *)shown,
        ITEM_NAME_SIZE + 1, &names));
    ASSERT_FALSE(item_names_shown(&names, 0, 0, 0, 1, name));
    ASSERT_STREQ("untouched", name);
}
