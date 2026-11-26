#include "import/units.h"

#include <stdio.h>
#include <string.h>

#include "make_reg.h"
#include "utest.h"

/* Three units, in sections whose numbers are not their IDs:
 *
 *     Unit0   ID 3    a swordsman, who states everything
 *     Unit1   ID 14   an archer, a child of 3, who states only what
 *                     differs
 *     Unit2   ID 20   a child of 14, so a grandchild of 3
 */
static size_t make_three_units(MakeReg *m)
{
    const int32_t swordsman_times[] = {2, 2, 1};
    const int32_t archer_times[] = {2, 3, 3, 2};

    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "UnitCount", 3);

    make_reg_section(m, "Unit0");
    make_reg_int(m, "ID", 3);
    make_reg_text(m, "DescText", "Swordsman");
    make_reg_int(m, "MovePhases", 8);
    make_reg_int(m, "AttackPhases", 7);
    make_reg_int(m, "Width", 128);
    make_reg_int_list(m, "AttackAnimTime", swordsman_times, 3);
    make_reg_text(m, "InfoPicture", "SwordsMan");

    make_reg_section(m, "Unit1");
    make_reg_int(m, "ID", 14);
    make_reg_int(m, "Parent", 3);
    make_reg_text(m, "DescText", "Archer");
    make_reg_int(m, "AttackPhases", 9);
    make_reg_int_list(m, "AttackAnimTime", archer_times, 4);

    make_reg_section(m, "Unit2");
    make_reg_int(m, "ID", 20);
    make_reg_int(m, "Parent", 14);
    make_reg_int(m, "Width", -5);

    return make_reg_finish(m);
}

/* ---- units_section ---- */

UTEST(units, section_is_found_by_id_not_by_position)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    char name[UNITS_SECTION_SIZE] = "untouched";

    ASSERT_TRUE(units_section(&r, 3, name));
    ASSERT_STREQ("Unit0", name);
    ASSERT_TRUE(units_section(&r, 14, name));
    ASSERT_STREQ("Unit1", name);
    ASSERT_TRUE(units_section(&r, 20, name));
    ASSERT_STREQ("Unit2", name);
}

UTEST(units, section_of_an_id_no_unit_has_fails)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    char name[UNITS_SECTION_SIZE] = "untouched";

    /* 0, 1 and 2 are section numbers, not IDs. */
    ASSERT_FALSE(units_section(&r, 0, name));
    ASSERT_FALSE(units_section(&r, 1, name));
    ASSERT_FALSE(units_section(&r, 99, name));
    ASSERT_FALSE(units_section(&r, -1, name));
    ASSERT_STREQ("untouched", name);
}

UTEST(units, section_search_survives_a_wrong_unit_count)
{
    /* The registry says 50 units and has one. Looking for a unit that is
     * not there must end, not run on. */
    static MakeReg m;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 50);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 3);
    size_t size = make_reg_finish(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    char name[UNITS_SECTION_SIZE] = "untouched";

    ASSERT_TRUE(units_section(&r, 3, name));
    ASSERT_STREQ("Unit0", name);

    strcpy(name, "untouched");
    ASSERT_FALSE(units_section(&r, 4, name));
    ASSERT_STREQ("untouched", name);
}

UTEST(units, section_without_a_unit_count_fails)
{
    static MakeReg m;
    make_reg_start(&m);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 3);
    size_t size = make_reg_finish(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    char name[UNITS_SECTION_SIZE] = "untouched";

    ASSERT_FALSE(units_section(&r, 3, name));
    ASSERT_STREQ("untouched", name);
}

/* ---- units_int ---- */

UTEST(units, int_reads_what_the_unit_states_itself)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t value = 12345;

    ASSERT_TRUE(units_int(&r, 3, "AttackPhases", &value));
    ASSERT_EQ(7, value);
    ASSERT_TRUE(units_int(&r, 14, "AttackPhases", &value));
    ASSERT_EQ(9, value);
    ASSERT_TRUE(units_int(&r, 20, "Width", &value));
    ASSERT_EQ(-5, value);
}

UTEST(units, int_falls_back_to_the_parent_and_the_parents_parent)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t value = 12345;

    /* The archer states no MovePhases: the swordsman's 8. */
    ASSERT_TRUE(units_int(&r, 14, "MovePhases", &value));
    ASSERT_EQ(8, value);
    /* The grandchild states neither, and gets the nearest value: its
     * parent's AttackPhases, its grandparent's MovePhases. */
    ASSERT_TRUE(units_int(&r, 20, "AttackPhases", &value));
    ASSERT_EQ(9, value);
    ASSERT_TRUE(units_int(&r, 20, "MovePhases", &value));
    ASSERT_EQ(8, value);
    /* The archer's Width is the swordsman's, not its own child's. */
    ASSERT_TRUE(units_int(&r, 14, "Width", &value));
    ASSERT_EQ(128, value);
}

UTEST(units, int_that_nobody_states_fails)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t value = 12345;

    ASSERT_FALSE(units_int(&r, 3, "Flip", &value));
    ASSERT_FALSE(units_int(&r, 20, "Flip", &value));
    ASSERT_FALSE(units_int(&r, 99, "MovePhases", &value));
    ASSERT_EQ(12345, value);
}

UTEST(units, int_does_not_skip_a_value_of_another_kind)
{
    /* The archer has a DescText, which is a text. Asking for it as a
     * number fails. It does not go on to look in the parent. */
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t value = 12345;

    ASSERT_FALSE(units_int(&r, 14, "DescText", &value));
    ASSERT_FALSE(units_int(&r, 14, "AttackAnimTime", &value));
    ASSERT_EQ(12345, value);
}

UTEST(units, int_with_a_parent_that_does_not_exist_fails)
{
    static MakeReg m;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 1);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 5);
    make_reg_int(&m, "Parent", 77);
    make_reg_int(&m, "Width", 64);
    size_t size = make_reg_finish(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t value = 12345;

    ASSERT_TRUE(units_int(&r, 5, "Width", &value));
    ASSERT_EQ(64, value);

    value = 12345;
    ASSERT_FALSE(units_int(&r, 5, "Height", &value));
    ASSERT_EQ(12345, value);
}

UTEST(units, int_with_parents_in_a_circle_ends)
{
    /* 1 is a child of 2 and 2 a child of 1, and one unit is its own
     * parent. A search for something none of them states must give up. */
    static MakeReg m;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 3);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 1);
    make_reg_int(&m, "Parent", 2);
    make_reg_section(&m, "Unit1");
    make_reg_int(&m, "ID", 2);
    make_reg_int(&m, "Parent", 1);
    make_reg_int(&m, "Width", 64);
    make_reg_section(&m, "Unit2");
    make_reg_int(&m, "ID", 3);
    make_reg_int(&m, "Parent", 3);
    size_t size = make_reg_finish(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t value = 12345;

    ASSERT_TRUE(units_int(&r, 1, "Width", &value));
    ASSERT_EQ(64, value);

    value = 12345;
    ASSERT_FALSE(units_int(&r, 1, "Height", &value));
    ASSERT_FALSE(units_int(&r, 3, "Height", &value));
    ASSERT_EQ(12345, value);
}

/* A chain of units 1, 2, 3 ... each the child of the next. Only the last
 * one states "Width". */
static size_t make_chain(MakeReg *m, int units)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "UnitCount", units);
    for (int i = 0; i < units; i++) {
        char name[UNITS_SECTION_SIZE];
        snprintf(name, sizeof name, "Unit%d", i);
        make_reg_section(m, name);
        make_reg_int(m, "ID", i + 1);
        if (i + 1 < units) {
            make_reg_int(m, "Parent", i + 2);
        } else {
            make_reg_int(m, "Width", 64);
        }
    }
    return make_reg_finish(m);
}

UTEST(units, int_follows_at_most_eight_parents)
{
    static MakeReg m;
    Reg r;
    int32_t value = 12345;

    /* Nine units: the value is eight parents up. */
    size_t size = make_chain(&m, 9);
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    ASSERT_TRUE(units_int(&r, 1, "Width", &value));
    ASSERT_EQ(64, value);

    /* Ten units: nine parents up is one too many. */
    value = 12345;
    size = make_chain(&m, 10);
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    ASSERT_FALSE(units_int(&r, 1, "Width", &value));
    ASSERT_EQ(12345, value);
    /* From the second unit it is eight again. */
    ASSERT_TRUE(units_int(&r, 2, "Width", &value));
    ASSERT_EQ(64, value);
}

/* ---- units_text ---- */

UTEST(units, text_is_the_units_own_or_its_parents)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    const char *text = NULL;

    ASSERT_TRUE(units_text(&r, 3, "DescText", &text));
    ASSERT_STREQ("Swordsman", text);
    ASSERT_TRUE(units_text(&r, 14, "DescText", &text));
    ASSERT_STREQ("Archer", text);
    /* Stated by the swordsman only. */
    ASSERT_TRUE(units_text(&r, 14, "InfoPicture", &text));
    ASSERT_STREQ("SwordsMan", text);
    ASSERT_TRUE(units_text(&r, 20, "DescText", &text));
    ASSERT_STREQ("Archer", text);
}

UTEST(units, text_that_nobody_states_or_of_another_kind_fails)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    const char *marker = "untouched";
    const char *text = marker;

    ASSERT_FALSE(units_text(&r, 14, "Nothing", &text));
    ASSERT_FALSE(units_text(&r, 14, "AttackPhases", &text));
    ASSERT_FALSE(units_text(&r, 99, "DescText", &text));
    ASSERT_TRUE(text == marker);
}

/* ---- units_int_list ---- */

UTEST(units, int_list_is_the_units_own_or_its_parents)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t numbers[8];
    size_t count = 0;

    ASSERT_TRUE(units_int_list(&r, 3, "AttackAnimTime", numbers, 8, &count));
    ASSERT_EQ(3u, count);
    ASSERT_EQ(1, numbers[2]);

    ASSERT_TRUE(units_int_list(&r, 14, "AttackAnimTime", numbers, 8, &count));
    ASSERT_EQ(4u, count);
    ASSERT_EQ(3, numbers[1]);

    /* The grandchild has none of its own and gets the archer's four. */
    ASSERT_TRUE(units_int_list(&r, 20, "AttackAnimTime", numbers, 8, &count));
    ASSERT_EQ(4u, count);
    ASSERT_EQ(2, numbers[3]);
}

UTEST(units, int_list_that_does_not_fit_fails_without_using_the_parents)
{
    /* The archer's list has four numbers and the swordsman's three. With
     * room for three, the archer's does not fit. Its parent's would, but
     * handing that over would be wrong. */
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t numbers[3] = {99, 99, 99};
    size_t count = 12345;

    ASSERT_FALSE(units_int_list(&r, 14, "AttackAnimTime", numbers, 3, &count));

    ASSERT_EQ(99, numbers[0]);
    ASSERT_EQ((size_t)12345, count);
}

UTEST(units, int_list_that_nobody_states_fails)
{
    static MakeReg m;
    size_t size = make_three_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    int32_t numbers[8];
    size_t count = 12345;

    ASSERT_FALSE(units_int_list(&r, 14, "Sound", numbers, 8, &count));
    ASSERT_FALSE(units_int_list(&r, 14, "Width", numbers, 8, &count));
    ASSERT_EQ((size_t)12345, count);
}

/* ---- units_has ---- */

UTEST(units, has_is_true_for_a_value_of_the_unit_or_a_parent)
{
    static MakeReg m;
    Reg reg;
    ASSERT_TRUE(reg_open(m.bytes, make_three_units(&m), &reg));

    ASSERT_TRUE(units_has(&reg, 3, "MovePhases"));
    ASSERT_TRUE(units_has(&reg, 3, "DescText"));
    /* The grandchild has it from the swordsman. */
    ASSERT_TRUE(units_has(&reg, 20, "MovePhases"));
}

UTEST(units, has_is_false_for_a_value_nobody_states)
{
    static MakeReg m;
    Reg reg;
    ASSERT_TRUE(reg_open(m.bytes, make_three_units(&m), &reg));

    ASSERT_FALSE(units_has(&reg, 3, "NoSuchKey"));
    ASSERT_FALSE(units_has(&reg, 20, "NoSuchKey"));
    /* And for a unit that is not there. */
    ASSERT_FALSE(units_has(&reg, 99, "MovePhases"));
}
