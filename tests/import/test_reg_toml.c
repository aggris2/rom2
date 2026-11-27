#include "import/reg_toml.h"

#include <string.h>

#include "expect.h"
#include "make_reg.h"
#include "utest.h"

UTEST(reg_toml, write_gives_each_section_as_a_table_in_the_order_it_has)
{
    static MakeReg m;
    Reg reg;
    ByteBuffer b = {0};
    const int32_t faces[] = {32, 33, -1};
    make_reg_start(&m);
    make_reg_section(&m, "Terrain");
    make_reg_int(&m, "CostLand", 8);
    make_reg_int(&m, "PassWater", -2);
    make_reg_section(&m, "Common");
    make_reg_text(&m, "Path", "units\\sword");
    make_reg_int_list(&m, "FacesMF", faces, 3);
    make_reg_float(&m, "endfade", 0, 0x3ff00000u);
    make_reg_float(&m, "startfade", 0, 0);
    make_reg_float(&m, "half", 0, 0x3fe00000u);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(reg_toml_write(&b, &reg));

    ASSERT_TRUE(holds(&b,
        "\n"
        "[Terrain]\n"
        "CostLand = 8\n"
        "PassWater = -2\n"
        "\n"
        "[Common]\n"
        "Path = \"units\\\\sword\"\n"
        "FacesMF = [32, 33, -1]\n"
        "endfade = 1.0\n"
        "startfade = 0.0\n"
        "half = 0.5\n"));
    buffer_free(&b);
}

UTEST(reg_toml, a_name_that_cannot_stand_bare_is_put_in_quotes)
{
    static MakeReg m;
    Reg reg;
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Path Finding");
    make_reg_int(&m, "Scan.Ahead", 5);
    make_reg_int(&m, "Scan_Ahead-2", 6);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(reg_toml_write(&b, &reg));

    ASSERT_TRUE(holds(&b,
        "\n"
        "[\"Path Finding\"]\n"
        "\"Scan.Ahead\" = 5\n"
        "Scan_Ahead-2 = 6\n"));
    buffer_free(&b);
}

UTEST(reg_toml, a_section_without_values_is_an_empty_table)
{
    static MakeReg m;
    Reg reg;
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Empty");
    make_reg_section(&m, "Global");
    make_reg_int(&m, "Count", 0);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(reg_toml_write(&b, &reg));

    ASSERT_TRUE(holds(&b, "\n[Empty]\n\n[Global]\nCount = 0\n"));
    buffer_free(&b);
}

UTEST(reg_toml, a_name_with_a_quote_or_a_backslash_is_refused)
{
    static MakeReg m;
    Reg reg;
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "a\"b", 1);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));
    ASSERT_FALSE(reg_toml_write(&b, &reg));

    make_reg_start(&m);
    make_reg_section(&m, "a\\b");
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));
    b.size = 0;
    ASSERT_FALSE(reg_toml_write(&b, &reg));
    buffer_free(&b);
}

UTEST(reg_toml, a_list_longer_than_the_most_is_refused)
{
    static MakeReg m;
    static int32_t numbers[REG_TOML_MAX_LIST + 1];
    Reg reg;
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int_list(&m, "List", numbers, REG_TOML_MAX_LIST);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));
    ASSERT_TRUE(reg_toml_write(&b, &reg));

    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int_list(&m, "List", numbers, REG_TOML_MAX_LIST + 1);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));
    b.size = 0;
    ASSERT_FALSE(reg_toml_write(&b, &reg));
    buffer_free(&b);
}
