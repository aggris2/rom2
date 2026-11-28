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
    UnitKind kind = {0x5a5a, 0x5a5a, 0x5a, 0x5a, 0x5a5a, 0x5a5a, 0x5a,
        0x5a, 0x5a};
    return !reads(text, &kind) && kind.id == 0x5a5a && kind.face == 0x5a5a
        && kind.rotation_speed == 0x5a && kind.speed == 0x5a
        && kind.health == 0x5a5a
        && kind.mana == 0x5a5a && kind.size == 0x5a && kind.mover == 0x5a
        && kind.sight == 0x5a;
}

#define LIFE "speed = 18\nhealth = 45\nmana = 0\n"

UTEST(unit_kind, reads_the_health_and_the_mana)
{
    UnitKind kind;
    ASSERT_TRUE(reads("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 9\nhealth = 3600\nmana = 22500\n",
        &kind));
    EXPECT_EQ(3600, kind.health);
    EXPECT_EQ(22500, kind.mana);
    /* A few kinds have -1 for one or the other. */
    ASSERT_TRUE(reads("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 9\nhealth = -1\nmana = -1\n", &kind));
    EXPECT_EQ(-1, kind.health);
    EXPECT_EQ(-1, kind.mana);
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 9\nhealth = -2\nmana = 0\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 9\nhealth = 45\nmana = -2\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 9\nmana = 0\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 9\nhealth = 45\n"));
}

#define KIND "[[kind]]\nid = 600\nface = 2\nrotation_speed = 16\n" LIFE

UTEST(unit_kind, reads_its_speed)
{
    UnitKind kind;
    ASSERT_TRUE(reads("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 35\nhealth = 45\nmana = 0\n",
        &kind));
    EXPECT_EQ(35u, kind.speed);
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nhealth = 45\nmana = 0\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 0\nhealth = 45\nmana = 0\n"));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 16\nspeed = 256\nhealth = 45\nmana = 0\n"));
}

UTEST(unit_kind, reads_its_size_and_how_it_moves)
{
    UnitKind kind;
    ASSERT_TRUE(reads(KIND "token_size = 3\nmovement_type = 3\n", &kind));
    EXPECT_EQ(3u, kind.size);
    EXPECT_EQ((unsigned)UNIT_FLIES, kind.mover);
    ASSERT_TRUE(reads(KIND "token_size = 2\nmovement_type = 2\n", &kind));
    EXPECT_EQ(2u, kind.size);
    EXPECT_EQ((unsigned)UNIT_HOVERS, kind.mover);
    /* A kind that says -1, or nothing, covers a square and walks. */
    ASSERT_TRUE(reads(KIND "token_size = -1\nmovement_type = -1\n",
        &kind));
    EXPECT_EQ(1u, kind.size);
    EXPECT_EQ((unsigned)UNIT_WALKS, kind.mover);
    ASSERT_TRUE(reads(KIND, &kind));
    EXPECT_EQ(1u, kind.size);
    EXPECT_EQ((unsigned)UNIT_WALKS, kind.mover);
}

UTEST(unit_kind, reads_how_far_it_sees)
{
    UnitKind kind;
    ASSERT_TRUE(reads(KIND "scan_range = 6\n", &kind));
    EXPECT_EQ(6u, kind.sight);
    /* A kind that does not say sees nothing. */
    ASSERT_TRUE(reads(KIND, &kind));
    EXPECT_EQ(0u, kind.sight);
    EXPECT_TRUE(refuses(KIND "scan_range = -1\n"));
    EXPECT_TRUE(refuses(KIND "scan_range = 256\n"));
    EXPECT_TRUE(refuses(KIND "scan_range = \"far\"\n"));
}

UTEST(unit_kind, refuses_a_size_or_a_way_of_moving_there_is_not)
{
    EXPECT_TRUE(refuses(KIND "token_size = 0\n"));
    EXPECT_TRUE(refuses(KIND "token_size = 4\n"));
    EXPECT_TRUE(refuses(KIND "token_size = -2\n"));
    EXPECT_TRUE(refuses(KIND "token_size = \"big\"\n"));
    EXPECT_TRUE(refuses(KIND "movement_type = 0\n"));
    EXPECT_TRUE(refuses(KIND "movement_type = 4\n"));
    EXPECT_TRUE(refuses(KIND "movement_type = -2\n"));
}

UTEST(unit_kind, reads_the_id_the_face_and_the_rotation_speed)
{
    UnitKind kind;
    ASSERT_TRUE(reads("[[kind]]\nid = 600\nname = \"Goblin_Pike\"\n"
        "rotation_speed = 16\nface = 2\n" LIFE, &kind));
    EXPECT_EQ(600u, kind.id);
    EXPECT_EQ(2u, kind.face);
    EXPECT_EQ(16u, kind.rotation_speed);
    ASSERT_TRUE(reads("[[kind]]\nid = 16383\nface = 0\n"
        "rotation_speed = 255\n" LIFE, &kind));
    EXPECT_EQ(16383u, kind.id);
    EXPECT_EQ(0u, kind.face);
    EXPECT_EQ(255u, kind.rotation_speed);
}

UTEST(unit_kind, refuses_a_kind_that_lacks_a_part)
{
    EXPECT_TRUE(refuses("[[kind]]\nface = 2\nrotation_speed = 16\n" LIFE));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nrotation_speed = 16\n" LIFE));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n" LIFE));
}

#define SPEED "rotation_speed = 16\n" LIFE

UTEST(unit_kind, refuses_a_number_that_cannot_be)
{
    EXPECT_TRUE(refuses("[[kind]]\nid = -1\nface = 2\n" SPEED));
    EXPECT_TRUE(refuses("[[kind]]\nid = 16384\nface = 2\n" SPEED));
    EXPECT_TRUE(refuses("[[kind]]\nid = \"600\"\nface = 2\n" SPEED));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = -1\n" SPEED));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 65536\n" SPEED));
    /* A unit that cannot turn, or turns faster than a full turn. */
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 0\n" LIFE));
    EXPECT_TRUE(refuses("[[kind]]\nid = 600\nface = 2\n"
        "rotation_speed = 256\n" LIFE));
}
