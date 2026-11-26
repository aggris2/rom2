#include "import/things.h"

#include <string.h>

#include "make_reg.h"
#include "utest.h"

/* Three trees, as objects.reg has its things:
 *
 *     Object0   ID 5    a pine, which states everything
 *     Object1   ID 9    a child of 5
 *     Object2   ID 2    a child of 9, so a grandchild of 5
 */
static size_t make_three_objects(MakeReg *m)
{
    const int32_t times[] = {4, 4, 4};

    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "ObjectCount", 3);

    make_reg_section(m, "Object0");
    make_reg_int(m, "ID", 5);
    make_reg_text(m, "DescText", "Pine 1");
    make_reg_int(m, "Phases", 3);
    make_reg_int_list(m, "AnimationTime", times, 3);

    make_reg_section(m, "Object1");
    make_reg_int(m, "ID", 9);
    make_reg_int(m, "Parent", 5);
    make_reg_text(m, "DescText", "Pine 2");

    make_reg_section(m, "Object2");
    make_reg_int(m, "ID", 2);
    make_reg_int(m, "Parent", 9);
    return make_reg_finish(m);
}

UTEST(things, count_reads_the_number_under_the_given_name)
{
    static MakeReg m;
    Reg reg;
    uint32_t count = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_three_objects(&m), &reg));
    Things t = {&reg, "ObjectCount", "Object"};

    ASSERT_TRUE(things_count(&t, &count));
    ASSERT_EQ(3u, count);
}

UTEST(things, count_fails_without_the_number_or_with_a_negative_one)
{
    static MakeReg m;
    Reg reg;
    uint32_t count = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_three_objects(&m), &reg));
    Things units = {&reg, "UnitCount", "Unit"};
    ASSERT_FALSE(things_count(&units, &count));

    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "ObjectCount", -1);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));
    Things t = {&reg, "ObjectCount", "Object"};
    ASSERT_FALSE(things_count(&t, &count));
    ASSERT_EQ(99u, count);
}

UTEST(things, id_at_reads_the_id_of_the_section_at_a_position)
{
    static MakeReg m;
    Reg reg;
    int32_t id = 77;
    ASSERT_TRUE(reg_open(m.bytes, make_three_objects(&m), &reg));
    Things t = {&reg, "ObjectCount", "Object"};

    ASSERT_TRUE(things_id_at(&t, 0, &id));
    ASSERT_EQ(5, id);
    ASSERT_TRUE(things_id_at(&t, 2, &id));
    ASSERT_EQ(2, id);
    ASSERT_FALSE(things_id_at(&t, 3, &id));
    ASSERT_EQ(2, id);
}

UTEST(things, section_finds_a_thing_by_its_id_under_the_prefix)
{
    static MakeReg m;
    Reg reg;
    char section[THINGS_SECTION_SIZE] = "untouched";
    ASSERT_TRUE(reg_open(m.bytes, make_three_objects(&m), &reg));
    Things t = {&reg, "ObjectCount", "Object"};

    ASSERT_TRUE(things_section(&t, 9, section));
    ASSERT_STREQ("Object1", section);
    ASSERT_TRUE(things_section(&t, 2, section));
    ASSERT_STREQ("Object2", section);
    ASSERT_FALSE(things_section(&t, 1, section));
    ASSERT_STREQ("Object2", section);
}

UTEST(things, values_come_from_the_nearest_parent_that_states_them)
{
    static MakeReg m;
    Reg reg;
    int32_t phases = 0;
    const char *name = NULL;
    int32_t times[4];
    size_t count = 0;
    ASSERT_TRUE(reg_open(m.bytes, make_three_objects(&m), &reg));
    Things t = {&reg, "ObjectCount", "Object"};

    /* The grandchild has the pine's number and its parent's name. */
    ASSERT_TRUE(things_int(&t, 2, "Phases", &phases));
    ASSERT_EQ(3, phases);
    ASSERT_TRUE(things_text(&t, 2, "DescText", &name));
    ASSERT_STREQ("Pine 2", name);
    ASSERT_TRUE(things_int_list(&t, 2, "AnimationTime", times, 4, &count));
    ASSERT_EQ(3u, count);
    ASSERT_EQ(4, times[2]);
    ASSERT_TRUE(things_has(&t, 2, "Phases"));
    ASSERT_FALSE(things_has(&t, 2, "Width"));
    ASSERT_FALSE(things_int(&t, 2, "Width", &phases));
    ASSERT_EQ(3, phases);
}

UTEST(things, a_prefix_does_not_match_the_things_of_another_kind)
{
    /* Asking for units in a registry of objects finds nothing. */
    static MakeReg m;
    Reg reg;
    char section[THINGS_SECTION_SIZE];
    int32_t value = 0;
    ASSERT_TRUE(reg_open(m.bytes, make_three_objects(&m), &reg));
    Things t = {&reg, "ObjectCount", "Unit"};

    ASSERT_FALSE(things_section(&t, 5, section));
    ASSERT_FALSE(things_int(&t, 5, "Phases", &value));
}
