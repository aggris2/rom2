#include "data/unit_kind.h"

#include <string.h>

#include "utest.h"

/* Reads the first [[kind]] of a definition written as text. */
static bool reads(const char *text, UnitKind *out)
{
    TomlTable *top = NULL;
    if (!toml_read((const uint8_t *)text, strlen(text), &top, NULL)) {
        return false;
    }
    bool ok = toml_get_table_count(top, "kind") > 0
        && unit_kind_read(toml_get_table_at(top, "kind", 0), out);
    toml_free(top);
    return ok;
}

/* True if the kind is refused and `out` is left as it was. */
static bool refuses(const char *text)
{
    UnitKind kind = {0x5a5a, 0x5a5a};
    return !reads(text, &kind) && kind.id == 0x5a5a && kind.face == 0x5a5a;
}

UTEST(unit_kind, reads_the_id_and_the_face)
{
    UnitKind kind;
    ASSERT_TRUE(reads("[[kind]]\nid = 600\nname = \"Goblin_Pike\"\n"
        "face = 2\n", &kind));
    EXPECT_EQ(600u, kind.id);
    EXPECT_EQ(2u, kind.face);
    ASSERT_TRUE(reads("[[kind]]\nid = 16383\nface = 0\n", &kind));
    EXPECT_EQ(16383u, kind.id);
    EXPECT_EQ(0u, kind.face);
}

UTEST(unit_kind, refuses_a_kind_that_lacks_a_part)
{
    EXPECT_TRUE(refuses("[[kind]]\nface = 2\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\n"));
}

UTEST(unit_kind, refuses_a_number_that_cannot_be)
{
    EXPECT_TRUE(refuses("[[kind]]\nid = -1\nface = 2\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 16384\nface = 2\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = \"600\"\nface = 2\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = -1\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 65536\n"));
}
