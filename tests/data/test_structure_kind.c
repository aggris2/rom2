#include "data/structure_kind.h"

#include <string.h>

#include "utest.h"

#define SPRITE "[sprite]\nsheet = \"sprites.png\"\ncell = [64, 96]\n"

/* Reads a definition written as text. */
static bool reads(const char *text, StructureKind *out)
{
    TomlTable *top = NULL;
    if (!toml_read((const uint8_t *)text, strlen(text), &top, NULL)) {
        return false;
    }
    bool ok = structure_kind_read(top, out);
    toml_free(top);
    return ok;
}

/* True if the definition is refused and `out` is left as it was. */
static bool refuses(const char *text)
{
    StructureKind kind;
    memset(&kind, 0x5a, sizeof kind);
    StructureKind before = kind;
    return !reads(text, &kind) && memcmp(&kind, &before, sizeof kind) == 0;
}

UTEST(structure_kind, reads_the_base_the_height_and_the_sheet)
{
    StructureKind kind;
    ASSERT_TRUE(reads("id = 12\nsize = [2, 1]\nheight = 3\n" SPRITE,
        &kind));
    EXPECT_EQ(2u, kind.width);
    EXPECT_EQ(1u, kind.depth);
    EXPECT_EQ(3u, kind.height);
    EXPECT_STREQ("sprites.png", kind.sheet.file);
    EXPECT_EQ(64u, kind.sheet.cell_width);
    EXPECT_EQ(96u, kind.sheet.cell_height);
}

UTEST(structure_kind, a_picture_may_be_as_high_as_the_base_is_deep)
{
    StructureKind kind;
    EXPECT_TRUE(reads("size = [3, 3]\nheight = 3\n" SPRITE, &kind));
    EXPECT_TRUE(reads("size = [64, 64]\nheight = 64\n" SPRITE, &kind));
}

UTEST(structure_kind, a_structure_of_variable_size_is_one_of_pieces)
{
    StructureKind kind;
    ASSERT_TRUE(reads("size = [1, 1]\nheight = 1\nvariable_size = 1\n"
        SPRITE, &kind));
    EXPECT_TRUE(kind.pieces);
    ASSERT_TRUE(reads("size = [1, 1]\nheight = 1\nvariable_size = 0\n"
        SPRITE, &kind));
    EXPECT_FALSE(kind.pieces);
    ASSERT_TRUE(reads("size = [1, 1]\nheight = 1\n" SPRITE, &kind));
    EXPECT_FALSE(kind.pieces);
    EXPECT_TRUE(refuses("size = [1, 1]\nheight = 1\n"
        "variable_size = \"yes\"\n" SPRITE));
}

UTEST(structure_kind, refuses_a_definition_that_lacks_a_part)
{
    EXPECT_TRUE(refuses("height = 3\n" SPRITE));
    EXPECT_TRUE(refuses("size = [2, 2]\n" SPRITE));
    EXPECT_TRUE(refuses("size = [2, 2]\nheight = 3\n"));
}

UTEST(structure_kind, refuses_a_size_that_cannot_be)
{
    EXPECT_TRUE(refuses("size = [0, 2]\nheight = 3\n" SPRITE));
    EXPECT_TRUE(refuses("size = [2, 0]\nheight = 3\n" SPRITE));
    EXPECT_TRUE(refuses("size = [65, 2]\nheight = 3\n" SPRITE));
    EXPECT_TRUE(refuses("size = [2]\nheight = 3\n" SPRITE));
    EXPECT_TRUE(refuses("size = [2, 2, 2]\nheight = 3\n" SPRITE));
    EXPECT_TRUE(refuses("size = 2\nheight = 3\n" SPRITE));
    EXPECT_TRUE(refuses("size = [2, 2]\nheight = 65\n" SPRITE));
    EXPECT_TRUE(refuses("size = [2, 2]\nheight = \"3\"\n" SPRITE));
    /* A picture less high than the base is deep. */
    EXPECT_TRUE(refuses("size = [2, 3]\nheight = 2\n" SPRITE));
}
