#include "import/projectile_toml.h"

#include <string.h>

#include "make_reg.h"
#include "utest.h"

/* Two projectiles: a fire bolt, ID 10, and an arrow, ID 1, which states
 * nothing but its file. */
static size_t make_projectiles(MakeReg *m)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "Count", 2);
    make_reg_section(m, "Projectile0");
    make_reg_int(m, "ID", 10);
    make_reg_text(m, "File", "firebolt\\sprites");
    make_reg_int(m, "Homing", 1);
    make_reg_int(m, "SFX", 1);
    make_reg_section(m, "Projectile1");
    make_reg_int(m, "ID", 1);
    make_reg_text(m, "File", "archer\\arrow");
    return make_reg_finish(m);
}

static bool holds(const ByteBuffer *b, const char *expected)
{
    size_t length = strlen(expected);
    return b->size == length && memcmp(b->data, expected, length) == 0;
}

UTEST(projectile_toml, folder_name_is_the_id_and_the_file_text)
{
    static MakeReg m;
    Reg reg;
    char name[NAME_FOLDER_SIZE] = "untouched";
    ASSERT_TRUE(reg_open(m.bytes, make_projectiles(&m), &reg));

    ASSERT_TRUE(projectile_toml_folder_name(&reg, 10, name));
    ASSERT_STREQ("010_firebolt_sprites", name);
    ASSERT_TRUE(projectile_toml_folder_name(&reg, 1, name));
    ASSERT_STREQ("001_archer_arrow", name);
    ASSERT_FALSE(projectile_toml_folder_name(&reg, 2, name));
    ASSERT_STREQ("001_archer_arrow", name);
}

UTEST(projectile_toml, write_gives_the_whole_file_of_a_projectile)
{
    static MakeReg m;
    Reg reg;
    ProjectileSheet sheet = {64, 48, 4, 16, NULL, NULL};
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_projectiles(&m), &reg));

    ASSERT_TRUE(projectile_toml_write(&b, &reg, 10, &sheet));

    ASSERT_TRUE(holds(&b,
        "# firebolt/sprites\n"
        "id = 10\n"
        "name = \"firebolt/sprites\"\n"
        "homing = 1\n"
        "sfx = 1\n"
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "alpha = \"alpha.png\"\n"
        "palettes = \"palettes.png\"\n"
        "palette_rows = 1\n"
        "cell = [64, 48]\n"
        "anchor = [32, 24]\n"
        "columns = 4\n"
        "rows = 16\n"));
    buffer_free(&b);
}

UTEST(projectile_toml, write_leaves_out_what_the_projectile_does_not_say)
{
    static MakeReg m;
    Reg reg;
    ProjectileSheet sheet = {64, 64, 1, 16, NULL, NULL};
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_projectiles(&m), &reg));

    ASSERT_TRUE(projectile_toml_write(&b, &reg, 1, &sheet));

    ASSERT_TRUE(holds(&b,
        "# archer/arrow\n"
        "id = 1\n"
        "name = \"archer/arrow\"\n"
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "alpha = \"alpha.png\"\n"
        "palettes = \"palettes.png\"\n"
        "palette_rows = 1\n"
        "cell = [64, 64]\n"
        "anchor = [32, 32]\n"
        "columns = 1\n"
        "rows = 16\n"));
    buffer_free(&b);
    /* A projectile that is not there. */
    ASSERT_FALSE(projectile_toml_write(&b, &reg, 2, &sheet));
    buffer_free(&b);
}
