#include "import/unit.h"

#include <stdio.h>
#include <string.h>

#include "expect.h"
#include "make_databin.h"
#include "make_reg.h"
#include "make_sprite.h"
#include "utest.h"

/* ======== units ======== */

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

/* ======== unit_layout ======== */

/* Four units:
 *
 *     ID 3    a swordsman: 2 + 8 move frames, 7 attack frames, and his
 *             dying frames are in the file of unit 4
 *     ID 4    his child with a shield, who has the dying frames: 4 dying,
 *             3 bones
 *     ID 1    an unarmed child of 3 who is flipped and has his own dying
 *             frames
 *     ID 50   a creature with every kind of animation
 */
static size_t make_units(MakeReg *m)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "UnitCount", 4);

    make_reg_section(m, "Unit0");
    make_reg_int(m, "ID", 3);
    make_reg_int(m, "MoveBeginPhases", 2);
    make_reg_int(m, "MovePhases", 8);
    make_reg_int(m, "AttackPhases", 7);
    make_reg_int(m, "Dying", 4);

    make_reg_section(m, "Unit1");
    make_reg_int(m, "ID", 4);
    make_reg_int(m, "Parent", 3);
    make_reg_int(m, "DyingPhases", 4);
    make_reg_int(m, "BonePhases", 3);
    make_reg_int(m, "Dying", 4);

    make_reg_section(m, "Unit2");
    make_reg_int(m, "ID", 1);
    make_reg_int(m, "Parent", 3);
    make_reg_int(m, "Flip", 1);
    make_reg_int(m, "DyingPhases", 4);
    make_reg_int(m, "BonePhases", 3);
    make_reg_int(m, "Dying", 1);

    make_reg_section(m, "Unit3");
    make_reg_int(m, "ID", 50);
    make_reg_int(m, "Flip", 0);
    make_reg_int(m, "MovePhases", 5);
    make_reg_int(m, "AttackPhases", 3);
    make_reg_int(m, "DyingPhases", 2);
    make_reg_int(m, "BonePhases", 1);
    make_reg_int(m, "IdlePhases", 6);

    return make_reg_finish(m);
}

/* ---- unit_layout_read ---- */

UTEST(unit_layout, read_of_a_unit_whose_dying_frames_are_elsewhere)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;

    ASSERT_TRUE(unit_layout_read(&r, 3, &l));

    ASSERT_FALSE(l.flip);
    ASSERT_EQ(2u, l.move_begin);
    ASSERT_EQ(10u, l.phases[UNIT_ANIM_MOVE]);
    ASSERT_EQ(7u, l.phases[UNIT_ANIM_ATTACK]);
    ASSERT_EQ(0u, l.phases[UNIT_ANIM_DYING]);
    ASSERT_EQ(4, l.dying_id);
    ASSERT_FALSE(l.own_dying);
    ASSERT_EQ(16u, l.first[UNIT_ANIM_MOVE]);
    ASSERT_EQ(96u, l.first[UNIT_ANIM_ATTACK]);
    /* 16 + (2 + 8 + 7) * 8: the real swordsman's file has 152 frames. */
    ASSERT_EQ(152u, l.frame_count);
}

UTEST(unit_layout, read_of_a_child_with_its_own_dying_frames)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;

    ASSERT_TRUE(unit_layout_read(&r, 4, &l));

    /* Moving and attacking are the parent's. */
    ASSERT_EQ(10u, l.phases[UNIT_ANIM_MOVE]);
    ASSERT_EQ(7u, l.phases[UNIT_ANIM_ATTACK]);
    ASSERT_EQ(4u, l.phases[UNIT_ANIM_DYING]);
    ASSERT_EQ(3u, l.phases[UNIT_ANIM_BONES]);
    ASSERT_TRUE(l.own_dying);
    ASSERT_EQ(152u, l.first[UNIT_ANIM_DYING]);
    ASSERT_EQ(184u, l.first[UNIT_ANIM_BONES]);
    /* 152 + (4 + 3) * 8: the real file has 208. */
    ASSERT_EQ(208u, l.frame_count);
}

UTEST(unit_layout, read_of_a_flipped_unit_counts_9_steps_and_5_directions)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;

    ASSERT_TRUE(unit_layout_read(&r, 1, &l));

    ASSERT_TRUE(l.flip);
    ASSERT_EQ(9u, l.first[UNIT_ANIM_MOVE]);
    ASSERT_EQ(59u, l.first[UNIT_ANIM_ATTACK]);
    ASSERT_EQ(94u, l.first[UNIT_ANIM_DYING]);
    ASSERT_EQ(114u, l.first[UNIT_ANIM_BONES]);
    /* 9 + (10 + 7 + 4 + 3) * 5: the real file has 129. */
    ASSERT_EQ(129u, l.frame_count);
}

UTEST(unit_layout, read_of_a_unit_that_names_nobody_has_its_own_dying)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;

    ASSERT_TRUE(unit_layout_read(&r, 50, &l));

    ASSERT_FALSE(l.flip); /* Flip stated as 0 */
    ASSERT_EQ(0u, l.move_begin);
    ASSERT_EQ(50, l.dying_id);
    ASSERT_TRUE(l.own_dying);
    /* The blocks in order: move, attack, dying, bones, idle. */
    ASSERT_EQ(16u, l.first[UNIT_ANIM_MOVE]);
    ASSERT_EQ(56u, l.first[UNIT_ANIM_ATTACK]);
    ASSERT_EQ(80u, l.first[UNIT_ANIM_DYING]);
    ASSERT_EQ(96u, l.first[UNIT_ANIM_BONES]);
    ASSERT_EQ(104u, l.first[UNIT_ANIM_IDLE]);
    ASSERT_EQ(152u, l.frame_count);
}

UTEST(unit_layout, read_of_an_unknown_unit_fails)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;
    l.frame_count = 12345;

    ASSERT_FALSE(unit_layout_read(&r, 99, &l));
    ASSERT_EQ(12345u, l.frame_count);
}

/* A registry with one unit whose AttackPhases is `phases`. */
static size_t make_one_unit(MakeReg *m, int32_t phases)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "UnitCount", 1);
    make_reg_section(m, "Unit0");
    make_reg_int(m, "ID", 7);
    make_reg_int(m, "AttackPhases", phases);
    return make_reg_finish(m);
}

UTEST(unit_layout, read_refuses_phase_counts_that_make_no_sense)
{
    static MakeReg m;
    Reg r;
    UnitLayout l;
    l.frame_count = 12345;

    size_t size = make_one_unit(&m, 64);
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    ASSERT_TRUE(unit_layout_read(&r, 7, &l));
    ASSERT_EQ(16u + 64u * 8u, l.frame_count);

    l.frame_count = 12345;
    size = make_one_unit(&m, 65);
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    ASSERT_FALSE(unit_layout_read(&r, 7, &l));

    size = make_one_unit(&m, -1);
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    ASSERT_FALSE(unit_layout_read(&r, 7, &l));
    ASSERT_EQ(12345u, l.frame_count);
}

/* ---- unit_layout_rotation ---- */

UTEST(unit_layout, rotation_of_a_unit_that_stores_every_step)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;
    ASSERT_TRUE(unit_layout_read(&r, 3, &l));

    for (uint32_t step = 0; step < 16; step++) {
        uint32_t frame = 999;
        bool mirror = true;
        ASSERT_TRUE(unit_layout_rotation(&l, step, &frame, &mirror));
        ASSERT_EQ(step, frame);
        ASSERT_FALSE(mirror);
    }
}

UTEST(unit_layout, rotation_of_a_flipped_unit_mirrors_the_second_half)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;
    ASSERT_TRUE(unit_layout_read(&r, 1, &l));
    uint32_t frame = 999;
    bool mirror = true;

    /* Steps 0 to 8 are stored, the two ends included. */
    ASSERT_TRUE(unit_layout_rotation(&l, 0, &frame, &mirror));
    ASSERT_EQ(0u, frame);
    ASSERT_FALSE(mirror);
    ASSERT_TRUE(unit_layout_rotation(&l, 8, &frame, &mirror));
    ASSERT_EQ(8u, frame);
    ASSERT_FALSE(mirror);
    /* Step 9 is step 7 mirrored, and so on back to step 15 as step 1. */
    ASSERT_TRUE(unit_layout_rotation(&l, 9, &frame, &mirror));
    ASSERT_EQ(7u, frame);
    ASSERT_TRUE(mirror);
    ASSERT_TRUE(unit_layout_rotation(&l, 12, &frame, &mirror));
    ASSERT_EQ(4u, frame);
    ASSERT_TRUE(mirror);
    ASSERT_TRUE(unit_layout_rotation(&l, 15, &frame, &mirror));
    ASSERT_EQ(1u, frame);
    ASSERT_TRUE(mirror);
}

UTEST(unit_layout, rotation_past_the_last_step_fails)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;
    ASSERT_TRUE(unit_layout_read(&r, 3, &l));
    uint32_t frame = 999;
    bool mirror = true;

    ASSERT_FALSE(unit_layout_rotation(&l, 16, &frame, &mirror));
    ASSERT_EQ(999u, frame);
    ASSERT_TRUE(mirror);
}

/* ---- unit_layout_frame ---- */

UTEST(unit_layout, frame_counts_direction_by_direction_inside_a_block)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;
    ASSERT_TRUE(unit_layout_read(&r, 3, &l));
    uint32_t frame = 999;
    bool mirror = true;

    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_MOVE, 0, 0, &frame, &mirror));
    ASSERT_EQ(16u, frame);
    ASSERT_FALSE(mirror);
    /* Direction 3, frame 4: after three directions of 10 frames. */
    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_MOVE, 3, 4, &frame, &mirror));
    ASSERT_EQ(50u, frame);
    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_ATTACK, 0, 0, &frame,
        &mirror));
    ASSERT_EQ(96u, frame);
    /* The last attack frame of the last direction is the last frame. */
    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_ATTACK, 7, 6, &frame,
        &mirror));
    ASSERT_EQ(151u, frame);
    ASSERT_FALSE(mirror);
}

UTEST(unit_layout, frame_of_a_flipped_unit_mirrors_directions_5_to_7)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;
    ASSERT_TRUE(unit_layout_read(&r, 1, &l));
    uint32_t frame = 999;
    bool mirror = true;

    /* Directions 0 to 4 are stored. */
    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_MOVE, 4, 2, &frame, &mirror));
    ASSERT_EQ(9u + 4u * 10u + 2u, frame);
    ASSERT_FALSE(mirror);
    /* 5 is 3 mirrored, 6 is 2, and 7 is 1. */
    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_MOVE, 5, 2, &frame, &mirror));
    ASSERT_EQ(9u + 3u * 10u + 2u, frame);
    ASSERT_TRUE(mirror);
    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_ATTACK, 7, 0, &frame,
        &mirror));
    ASSERT_EQ(59u + 1u * 7u, frame);
    ASSERT_TRUE(mirror);
    /* The very last frame of the file: bones, direction 4, frame 2. */
    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_BONES, 4, 2, &frame,
        &mirror));
    ASSERT_EQ(128u, frame);
    ASSERT_FALSE(mirror);
}

UTEST(unit_layout, frame_out_of_range_fails)
{
    static MakeReg m;
    size_t size = make_units(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;
    ASSERT_TRUE(unit_layout_read(&r, 3, &l));
    uint32_t frame = 999;
    bool mirror = true;

    ASSERT_FALSE(unit_layout_frame(&l, UNIT_ANIM_MOVE, 8, 0, &frame, &mirror));
    ASSERT_FALSE(unit_layout_frame(&l, UNIT_ANIM_MOVE, 0, 10, &frame,
        &mirror));
    ASSERT_FALSE(unit_layout_frame(&l, UNIT_ANIM_ATTACK, 0, 7, &frame,
        &mirror));
    /* This unit has no idle animation. */
    ASSERT_FALSE(unit_layout_frame(&l, UNIT_ANIM_IDLE, 0, 0, &frame,
        &mirror));
    ASSERT_FALSE(unit_layout_frame(&l, UNIT_ANIM_COUNT, 0, 0, &frame,
        &mirror));
    ASSERT_EQ(999u, frame);
    ASSERT_TRUE(mirror);
}

UTEST(unit_layout, dying_frames_in_another_units_file_are_not_given)
{
    /* A unit that states dying phases but names another unit for them. */
    static MakeReg m;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 1);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 90);
    make_reg_int(&m, "MovePhases", 8);
    make_reg_int(&m, "DyingPhases", 6);
    make_reg_int(&m, "BonePhases", 3);
    make_reg_int(&m, "IdlePhases", 2);
    make_reg_int(&m, "Dying", 89);
    size_t size = make_reg_finish(&m);
    Reg r;
    ASSERT_TRUE(reg_open(m.bytes, size, &r));
    UnitLayout l;
    ASSERT_TRUE(unit_layout_read(&r, 90, &l));
    uint32_t frame = 999;
    bool mirror = true;

    ASSERT_FALSE(l.own_dying);
    ASSERT_EQ(89, l.dying_id);
    ASSERT_EQ(6u, l.phases[UNIT_ANIM_DYING]);
    /* Dying and bones take no room, so idle follows moving directly. */
    ASSERT_EQ(16u + 8u * 8u, l.first[UNIT_ANIM_IDLE]);
    ASSERT_EQ(16u + (8u + 2u) * 8u, l.frame_count);

    ASSERT_FALSE(unit_layout_frame(&l, UNIT_ANIM_DYING, 0, 0, &frame,
        &mirror));
    ASSERT_FALSE(unit_layout_frame(&l, UNIT_ANIM_BONES, 0, 0, &frame,
        &mirror));
    ASSERT_EQ(999u, frame);
    ASSERT_TRUE(unit_layout_frame(&l, UNIT_ANIM_IDLE, 7, 1, &frame, &mirror));
    ASSERT_EQ(16u + 64u + 7u * 2u + 1u, frame);
}

/* ---- unit_layout_fit_dying ---- */

/* A unit like the WarSheep: 8 moving phases, 4 dying and 3 bones, all
 * in its own file, not mirrored. 16 + 8 * (8 + 4 + 3) is 136 frames. */
static size_t make_sheep(MakeReg *m)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "UnitCount", 1);
    make_reg_section(m, "Unit0");
    make_reg_int(m, "ID", 107);
    make_reg_int(m, "MovePhases", 8);
    make_reg_int(m, "DyingPhases", 4);
    make_reg_int(m, "BonePhases", 3);
    make_reg_int(m, "Dying", 107);
    return make_reg_finish(m);
}

UTEST(unit_layout, fit_dying_gives_the_frames_over_to_the_dying)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    ASSERT_TRUE(reg_open(m.bytes, make_sheep(&m), &reg));
    ASSERT_TRUE(unit_layout_read(&reg, 107, &layout));
    ASSERT_EQ(136u, layout.frame_count);
    ASSERT_EQ(80u, layout.first[UNIT_ANIM_DYING]);
    ASSERT_EQ(112u, layout.first[UNIT_ANIM_BONES]);

    /* 144 frames: 8 over, one dying phase more for each direction. */
    ASSERT_TRUE(unit_layout_fit_dying(&layout, 144));

    ASSERT_EQ(5u, layout.phases[UNIT_ANIM_DYING]);
    ASSERT_EQ(3u, layout.phases[UNIT_ANIM_BONES]);
    ASSERT_EQ(8u, layout.phases[UNIT_ANIM_MOVE]);
    ASSERT_EQ(80u, layout.first[UNIT_ANIM_DYING]);
    ASSERT_EQ(120u, layout.first[UNIT_ANIM_BONES]);
    ASSERT_EQ(144u, layout.first[UNIT_ANIM_IDLE]);
    ASSERT_EQ(144u, layout.frame_count);

    /* Two phases more at once. */
    ASSERT_TRUE(unit_layout_fit_dying(&layout, 160));
    ASSERT_EQ(7u, layout.phases[UNIT_ANIM_DYING]);
    ASSERT_EQ(136u, layout.first[UNIT_ANIM_BONES]);
}

UTEST(unit_layout, fit_dying_leaves_alone_what_it_cannot_fit)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    ASSERT_TRUE(reg_open(m.bytes, make_sheep(&m), &reg));
    ASSERT_TRUE(unit_layout_read(&reg, 107, &layout));
    UnitLayout before = layout;

    /* It fits already, the file is shorter, and frames over that are
     * not a whole phase. */
    ASSERT_FALSE(unit_layout_fit_dying(&layout, 136));
    ASSERT_FALSE(unit_layout_fit_dying(&layout, 128));
    ASSERT_FALSE(unit_layout_fit_dying(&layout, 143));
    ASSERT_FALSE(unit_layout_fit_dying(&layout, 137));
    ASSERT_EQ(0, memcmp(&layout, &before, sizeof layout));

    /* A unit whose dying frames are in another unit's file. */
    layout.own_dying = false;
    before = layout;
    ASSERT_FALSE(unit_layout_fit_dying(&layout, 144));
    ASSERT_EQ(0, memcmp(&layout, &before, sizeof layout));
}

UTEST(unit_layout, fit_dying_of_a_mirrored_unit_counts_its_five_directions)
{
    /* Mirrored, a phase is 5 frames: 9 + 5 * (8 + 4 + 3) is 84. */
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 1);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 1);
    make_reg_int(&m, "Flip", 1);
    make_reg_int(&m, "MovePhases", 8);
    make_reg_int(&m, "DyingPhases", 4);
    make_reg_int(&m, "BonePhases", 3);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));
    ASSERT_TRUE(unit_layout_read(&reg, 1, &layout));
    ASSERT_EQ(84u, layout.frame_count);

    ASSERT_FALSE(unit_layout_fit_dying(&layout, 92));
    ASSERT_TRUE(unit_layout_fit_dying(&layout, 89));
    ASSERT_EQ(5u, layout.phases[UNIT_ANIM_DYING]);
    ASSERT_EQ(89u, layout.frame_count);
}

/* ======== unit_sheet ======== */

/* The layout of a unit that is not flipped, with `move` moving frames for
 * each direction and nothing else: 16 + move * 8 frames. */
static UnitLayout plain_layout(uint32_t move)
{
    UnitLayout l;
    memset(&l, 0, sizeof l);
    l.phases[UNIT_ANIM_MOVE] = move;
    l.own_dying = true;
    for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
        l.first[anim] = 16;
    }
    for (int anim = UNIT_ANIM_ATTACK; anim < UNIT_ANIM_COUNT; anim++) {
        l.first[anim] = 16 + move * 8;
    }
    l.frame_count = 16 + move * 8;
    return l;
}

/* The same for a flipped unit: 9 + move * 5 frames. */
static UnitLayout flipped_layout(uint32_t move)
{
    UnitLayout l = plain_layout(move);
    l.flip = true;
    l.first[UNIT_ANIM_MOVE] = 9;
    for (int anim = UNIT_ANIM_ATTACK; anim < UNIT_ANIM_COUNT; anim++) {
        l.first[anim] = 9 + move * 5;
    }
    l.frame_count = 9 + move * 5;
    return l;
}

/* The pixel at `x`, `y` inside the cell at `column`, `row`. */
static uint8_t cell_at(const UnitSheet *u, uint32_t column, uint32_t row,
    uint32_t x, uint32_t y)
{
    const Sheet *s = &u->sheet;
    size_t px = (size_t)column * s->cell_width + x;
    size_t py = (size_t)row * s->cell_height + y;
    return s->pixels[py * sheet_width(s) + px];
}

/* ---- unit_sheet_build ---- */

UTEST(unit_sheet, build_puts_every_frame_in_its_row_and_column)
{
    /* 24 frames of 2 by 2, each filled with its own number from 1 up.
     * The box is 8 by 6, so every frame sits at 3, 2 in it. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 24; i++) {
        make_sprite_solid(&m, 2, 2, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(1);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 8, 6, 4, 5, &u));

    /* The cell is the part of the box the frames use: 2 by 2. */
    ASSERT_EQ(2u, u.sheet.cell_width);
    ASSERT_EQ(2u, u.sheet.cell_height);
    ASSERT_EQ(16u, u.sheet.columns);
    ASSERT_EQ(9u, u.sheet.rows);
    /* The centre 4, 5 of the box, counted from the cell's corner 3, 2. */
    ASSERT_EQ(1, u.anchor_x);
    ASSERT_EQ(3, u.anchor_y);
    ASSERT_EQ(0u, u.column[UNIT_ANIM_MOVE]);
    ASSERT_EQ(1u, u.columns[UNIT_ANIM_MOVE]);
    ASSERT_EQ(0u, u.columns[UNIT_ANIM_ATTACK]);

    /* Row 0: the 16 standing frames, 1 to 16. */
    for (uint32_t step = 0; step < 16; step++) {
        ASSERT_EQ(step + 1, cell_at(&u, step, 0, 0, 0));
        ASSERT_EQ(step + 1, cell_at(&u, step, 0, 1, 1));
    }
    /* Rows 1 to 8: one moving frame for each direction, 17 to 24. */
    for (uint32_t d = 0; d < 8; d++) {
        ASSERT_EQ(17 + d, cell_at(&u, 0, 1 + d, 0, 0));
        ASSERT_EQ(0, cell_at(&u, 1, 1 + d, 0, 0));
    }
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_keeps_frames_of_different_sizes_in_place)
{
    /* Frame 0 is 6 by 2 and the rest 2 by 4, in a box of 8 by 8. The
     * wide one sits at 1, 3 in the box and the tall ones at 3, 2, so the
     * cell is 6 wide and 4 high, starting at 1, 2. */
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 6, 2, 9);
    for (uint8_t i = 1; i < 16; i++) {
        make_sprite_solid(&m, 2, 4, 5);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 8, 8, 4, 6, &u));

    ASSERT_EQ(6u, u.sheet.cell_width);
    ASSERT_EQ(4u, u.sheet.cell_height);
    ASSERT_EQ(3, u.anchor_x);
    ASSERT_EQ(4, u.anchor_y);
    /* The wide frame: rows 1 and 2 of its cell, all 6 columns. */
    ASSERT_EQ(0, cell_at(&u, 0, 0, 0, 0));
    ASSERT_EQ(9, cell_at(&u, 0, 0, 0, 1));
    ASSERT_EQ(9, cell_at(&u, 0, 0, 5, 2));
    ASSERT_EQ(0, cell_at(&u, 0, 0, 5, 3));
    /* A tall frame: columns 2 and 3 of its cell, all 4 rows. */
    ASSERT_EQ(0, cell_at(&u, 1, 0, 1, 0));
    ASSERT_EQ(5, cell_at(&u, 1, 0, 2, 0));
    ASSERT_EQ(5, cell_at(&u, 1, 0, 3, 3));
    ASSERT_EQ(0, cell_at(&u, 1, 0, 4, 3));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_writes_out_the_mirrored_half_of_a_flipped_unit)
{
    /* Nine standing frames and five directions of one moving frame. Each
     * frame is 2 by 1 with its number on the left and 100 more on the
     * right, so a mirrored one can be told from a plain one. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 14; i++) {
        const uint8_t pixels[2] = {(uint8_t)(i + 1), (uint8_t)(i + 101)};
        make_sprite_frame(&m, 2, 1, pixels);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = flipped_layout(1);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 4, 4, 2, 2, &u));

    ASSERT_EQ(2u, u.sheet.cell_width);
    /* Steps 0 to 8 as stored. */
    ASSERT_EQ(1, cell_at(&u, 0, 0, 0, 0));
    ASSERT_EQ(101, cell_at(&u, 0, 0, 1, 0));
    ASSERT_EQ(9, cell_at(&u, 8, 0, 0, 0));
    /* Step 9 is frame 7 mirrored: its number 8 is now on the right. */
    ASSERT_EQ(108, cell_at(&u, 9, 0, 0, 0));
    ASSERT_EQ(8, cell_at(&u, 9, 0, 1, 0));
    /* Step 15 is frame 1 mirrored. */
    ASSERT_EQ(102, cell_at(&u, 15, 0, 0, 0));
    ASSERT_EQ(2, cell_at(&u, 15, 0, 1, 0));
    /* Direction 4 is the last stored one, frame 9 + 4. */
    ASSERT_EQ(14, cell_at(&u, 0, 1 + 4, 0, 0));
    /* Direction 5 is direction 3 mirrored, frame 9 + 3. */
    ASSERT_EQ(113, cell_at(&u, 0, 1 + 5, 0, 0));
    ASSERT_EQ(13, cell_at(&u, 0, 1 + 5, 1, 0));
    /* Direction 7 is direction 1 mirrored, frame 9 + 1. */
    ASSERT_EQ(111, cell_at(&u, 0, 1 + 7, 0, 0));
    ASSERT_EQ(11, cell_at(&u, 0, 1 + 7, 1, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_mirrors_about_the_middle_of_the_box)
{
    /* A box 5 wide and a frame 2 wide cannot be centred exactly: the
     * frame sits at 1, with 1 pixel free on its left and 2 on its right.
     * Mirrored it sits at 2. The cell must hold both: 3 wide, from 1. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 9; i++) {
        const uint8_t pixels[2] = {(uint8_t)(i + 1), (uint8_t)(i + 101)};
        make_sprite_frame(&m, 2, 1, pixels);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = flipped_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 5, 1, 2, 0, &u));

    ASSERT_EQ(3u, u.sheet.cell_width);
    ASSERT_EQ(1, u.anchor_x);
    /* Step 1 as stored: at the left of the cell. */
    ASSERT_EQ(2, cell_at(&u, 1, 0, 0, 0));
    ASSERT_EQ(102, cell_at(&u, 1, 0, 1, 0));
    ASSERT_EQ(0, cell_at(&u, 1, 0, 2, 0));
    /* Step 15, the same frame mirrored: at the right of the cell. */
    ASSERT_EQ(0, cell_at(&u, 15, 0, 0, 0));
    ASSERT_EQ(102, cell_at(&u, 15, 0, 1, 0));
    ASSERT_EQ(2, cell_at(&u, 15, 0, 2, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_lays_the_animations_side_by_side)
{
    /* 2 moving and 3 attack frames for each direction: 16 + 5 * 8. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 56; i++) {
        make_sprite_solid(&m, 1, 1, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(2);
    layout.phases[UNIT_ANIM_ATTACK] = 3;
    layout.first[UNIT_ANIM_ATTACK] = 16 + 2 * 8;
    layout.frame_count = 56;
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 1, 1, 0, 0, &u));

    ASSERT_EQ(0u, u.column[UNIT_ANIM_MOVE]);
    ASSERT_EQ(2u, u.columns[UNIT_ANIM_MOVE]);
    ASSERT_EQ(2u, u.column[UNIT_ANIM_ATTACK]);
    ASSERT_EQ(3u, u.columns[UNIT_ANIM_ATTACK]);
    /* Never fewer than the 16 columns the standing row needs. */
    ASSERT_EQ(16u, u.sheet.columns);
    /* Direction 3: moving frames 16 + 3 * 2 + 0 and + 1, then attack
     * frames 32 + 3 * 3 + 0, + 1, + 2. Frame n holds the number n + 1. */
    ASSERT_EQ(23, cell_at(&u, 0, 1 + 3, 0, 0));
    ASSERT_EQ(24, cell_at(&u, 1, 1 + 3, 0, 0));
    ASSERT_EQ(42, cell_at(&u, 2, 1 + 3, 0, 0));
    ASSERT_EQ(43, cell_at(&u, 3, 1 + 3, 0, 0));
    ASSERT_EQ(44, cell_at(&u, 4, 1 + 3, 0, 0));
    ASSERT_EQ(0, cell_at(&u, 5, 1 + 3, 0, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_with_more_than_16_columns_of_animation)
{
    /* 20 moving frames for each direction: the sheet is 20 wide. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 16 + 20 * 8; i++) {
        make_sprite_solid(&m, 1, 1, (uint8_t)(i % 200 + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(20);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 1, 1, 0, 0, &u));

    ASSERT_EQ(20u, u.sheet.columns);
    /* The last frame: direction 7, frame 19, which is frame 175. */
    ASSERT_EQ(176, cell_at(&u, 19, 8, 0, 0));
    /* The standing row is empty past its 16 steps. */
    ASSERT_EQ(0, cell_at(&u, 16, 0, 0, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_refuses_a_sprite_with_the_wrong_number_of_frames)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 23; i++) {
        make_sprite_solid(&m, 2, 2, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(1); /* wants 24 */
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, NULL, 8, 6, 4, 5, &u));

    ASSERT_EQ(12345, u.anchor_x);
    ASSERT_TRUE(u.sheet.pixels == NULL);
}

UTEST(unit_sheet, build_refuses_a_frame_larger_than_the_box)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 16; i++) {
        make_sprite_solid(&m, 2, 2, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    /* A box of exactly the frame's size is fine. */
    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 2, 2, 1, 1, &u));
    unit_sheet_free(&u);

    u.anchor_x = 12345;
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, NULL, 1, 2, 0, 0, &u));
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, NULL, 2, 1, 0, 0, &u));
    /* A box of no size, or beyond the limit. */
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, NULL, 0, 2, 0, 0, &u));
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, NULL, 2, 0, 0, 0, &u));
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, NULL, 2049, 2, 0, 0, &u));
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, NULL, 2, 2049, 0, 0, &u));
    ASSERT_EQ(12345, u.anchor_x);
}

UTEST(unit_sheet, build_refuses_a_damaged_frame)
{
    /* The last frame's codes fill only one of its two pixels. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 15; i++) {
        make_sprite_solid(&m, 2, 1, 1);
    }
    uint8_t *broken = m.bytes + m.size;
    make_sprite_solid(&m, 1, 1, 1);
    put_u32(broken, 2);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;
    memset(&u, 0, sizeof u);

    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, NULL, 4, 4, 2, 2, &u));
    ASSERT_TRUE(u.sheet.pixels == NULL);
}

UTEST(unit_sheet, build_of_a_unit_whose_frames_are_all_empty)
{
    /* 16 frames of no size at all. There is nothing to show, and the
     * sheet is one transparent pixel for each cell. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 16; i++) {
        make_sprite_solid(&m, 0, 0, 0);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 8, 8, 4, 4, &u));

    ASSERT_EQ(1u, u.sheet.cell_width);
    ASSERT_EQ(1u, u.sheet.cell_height);
    ASSERT_EQ(0, cell_at(&u, 0, 0, 0, 0));
    unit_sheet_free(&u);
}

/* ---- unit_sheet_build_dying ---- */

/* The layout of a unit with every kind of frame in its own file, in the
 * file's order: standing, moving, attack, dying, bones, idle. */
static UnitLayout full_layout(bool flip, uint32_t move, uint32_t attack,
    uint32_t dying, uint32_t bones, uint32_t idle)
{
    UnitLayout l;
    memset(&l, 0, sizeof l);
    l.flip = flip;
    l.own_dying = true;
    l.phases[UNIT_ANIM_MOVE] = move;
    l.phases[UNIT_ANIM_ATTACK] = attack;
    l.phases[UNIT_ANIM_DYING] = dying;
    l.phases[UNIT_ANIM_BONES] = bones;
    l.phases[UNIT_ANIM_IDLE] = idle;
    uint32_t next = flip ? 9 : 16;
    for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
        l.first[anim] = next;
        next += l.phases[anim] * (flip ? 5u : 8u);
    }
    l.frame_count = next;
    return l;
}

UTEST(unit_sheet, build_dying_puts_dying_then_bones_in_each_row)
{
    /* 1 moving, 2 dying and 1 bones frame for each direction:
     * 16 + 8 + 16 + 8 frames, each 1 by 1 and holding its number + 1. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 48; i++) {
        make_sprite_solid(&m, 1, 1, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = full_layout(false, 1, 0, 2, 1, 0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build_dying(&layout, &sprite, NULL, 1, 1, 0, 0, &u));

    ASSERT_EQ(3u, u.sheet.columns);
    ASSERT_EQ(8u, u.sheet.rows);
    ASSERT_EQ(0u, u.first_direction_row);
    ASSERT_EQ(0u, u.column[UNIT_ANIM_DYING]);
    ASSERT_EQ(2u, u.columns[UNIT_ANIM_DYING]);
    ASSERT_EQ(2u, u.column[UNIT_ANIM_BONES]);
    ASSERT_EQ(1u, u.columns[UNIT_ANIM_BONES]);
    ASSERT_EQ(0u, u.columns[UNIT_ANIM_MOVE]);
    /* Dying starts at frame 24 and bones at frame 40. */
    for (uint32_t d = 0; d < 8; d++) {
        ASSERT_EQ(25 + d * 2, cell_at(&u, 0, d, 0, 0));
        ASSERT_EQ(26 + d * 2, cell_at(&u, 1, d, 0, 0));
        ASSERT_EQ(41 + d, cell_at(&u, 2, d, 0, 0));
    }
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_dying_writes_out_the_mirrored_directions)
{
    /* A flipped unit with one dying frame for each of its 5 stored
     * directions: 9 + 5 frames, each with its number on the left and 100
     * more on the right. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 14; i++) {
        const uint8_t pixels[2] = {(uint8_t)(i + 1), (uint8_t)(i + 101)};
        make_sprite_frame(&m, 2, 1, pixels);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = full_layout(true, 0, 0, 1, 0, 0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build_dying(&layout, &sprite, NULL, 4, 4, 2, 2, &u));

    ASSERT_EQ(1u, u.sheet.columns);
    ASSERT_EQ(8u, u.sheet.rows);
    /* Direction 4 as stored, frame 9 + 4. */
    ASSERT_EQ(14, cell_at(&u, 0, 4, 0, 0));
    ASSERT_EQ(114, cell_at(&u, 0, 4, 1, 0));
    /* Direction 5 is direction 3 mirrored, frame 9 + 3. */
    ASSERT_EQ(113, cell_at(&u, 0, 5, 0, 0));
    ASSERT_EQ(13, cell_at(&u, 0, 5, 1, 0));
    /* Direction 7 is direction 1 mirrored, frame 9 + 1. */
    ASSERT_EQ(111, cell_at(&u, 0, 7, 0, 0));
    ASSERT_EQ(11, cell_at(&u, 0, 7, 1, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, each_sheet_is_sized_by_its_own_frames_only)
{
    /* Standing and idle frames are 2 by 2 and filled with 1 and 3. The
     * dying frames, between them in the file, are 6 by 1 and filled with
     * 2. The main sheet must not grow to hold a dying frame, and the
     * dying sheet must not grow to hold a standing one. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 16; i++) {
        make_sprite_solid(&m, 2, 2, 1);
    }
    for (int i = 0; i < 8; i++) {
        make_sprite_solid(&m, 6, 1, 2);
    }
    for (int i = 0; i < 8; i++) {
        make_sprite_solid(&m, 2, 2, 3);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = full_layout(false, 0, 0, 1, 0, 1);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 8, 8, 4, 4, &u));
    ASSERT_EQ(2u, u.sheet.cell_width);
    ASSERT_EQ(2u, u.sheet.cell_height);
    ASSERT_EQ(0u, u.columns[UNIT_ANIM_DYING]);
    ASSERT_EQ(0u, u.column[UNIT_ANIM_IDLE]);
    ASSERT_EQ(1u, u.columns[UNIT_ANIM_IDLE]);
    /* The idle frames come after the dying ones in the file. */
    ASSERT_EQ(3, cell_at(&u, 0, 1, 0, 0));
    ASSERT_EQ(3, cell_at(&u, 0, 8, 1, 1));
    unit_sheet_free(&u);

    ASSERT_TRUE(unit_sheet_build_dying(&layout, &sprite, NULL, 8, 8, 4, 4, &u));
    ASSERT_EQ(6u, u.sheet.cell_width);
    ASSERT_EQ(1u, u.sheet.cell_height);
    /* The frame sits at 1, 3 in the box, and so does the cell. */
    ASSERT_EQ(3, u.anchor_x);
    ASSERT_EQ(1, u.anchor_y);
    ASSERT_EQ(2, cell_at(&u, 0, 0, 0, 0));
    ASSERT_EQ(2, cell_at(&u, 0, 7, 5, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_dying_refuses_a_unit_with_frames_in_another_file)
{
    /* The layout of a unit that borrows: its file has no dying frames. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 16; i++) {
        make_sprite_solid(&m, 1, 1, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    layout.phases[UNIT_ANIM_DYING] = 2;
    layout.own_dying = false;
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    ASSERT_FALSE(unit_sheet_build_dying(&layout, &sprite, NULL, 1, 1, 0, 0,
        &u));

    ASSERT_EQ(12345, u.anchor_x);
    ASSERT_TRUE(u.sheet.pixels == NULL);
    /* Its main sheet is still fine. */
    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 1, 1, 0, 0, &u));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_dying_refuses_a_unit_without_dying_frames)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 24; i++) {
        make_sprite_solid(&m, 1, 1, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(1);
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    ASSERT_FALSE(unit_sheet_build_dying(&layout, &sprite, NULL, 1, 1, 0, 0,
        &u));

    ASSERT_EQ(12345, u.anchor_x);
    ASSERT_TRUE(u.sheet.pixels == NULL);
}

UTEST(unit_sheet, build_dying_refuses_a_sprite_with_too_few_frames)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 23; i++) {
        make_sprite_solid(&m, 1, 1, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = full_layout(false, 0, 0, 1, 0, 0); /* wants 24 */
    UnitSheet u;
    memset(&u, 0, sizeof u);

    ASSERT_FALSE(unit_sheet_build_dying(&layout, &sprite, NULL, 1, 1, 0, 0,
        &u));
    ASSERT_TRUE(u.sheet.pixels == NULL);
}

/* ---- the soft edges ---- */

/* A sprite of 16 standing frames: frame 0 is `width` by `height` and
 * filled with `value`, the others are 1 by 1 and filled with 1. */
static bool make_standing(MakeSprite *m, uint32_t width, uint32_t height,
    uint8_t value, Sprite256 *out)
{
    make_sprite_start(m);
    make_sprite_solid(m, width, height, value);
    for (int i = 1; i < 16; i++) {
        make_sprite_solid(m, 1, 1, 1);
    }
    return make_sprite_finish(m, out);
}

UTEST(unit_sheet, build_without_edges_has_no_edge_sheet)
{
    static MakeSprite m;
    Sprite256 sprite;
    ASSERT_TRUE(make_standing(&m, 2, 2, 5, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, NULL, 8, 8, 4, 4, &u));

    ASSERT_TRUE(u.edges.pixels == NULL);
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_puts_the_edges_on_a_sheet_with_the_same_cells)
{
    /* The figure's frame 0 is 2 by 2 and its edge 4 by 4, in a box of 8
     * by 8: the figure sits at 3, 3 and the edge at 2, 2, around it. The
     * cell has to hold the edge, so it is 4 by 4, from 2, 2. */
    static MakeSprite m;
    static MakeSprite e;
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_standing(&m, 2, 2, 5, &sprite));
    ASSERT_TRUE(make_standing(&e, 4, 4, 9, &edges));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, &edges, 8, 8, 4, 6, &u));

    ASSERT_EQ(4u, u.sheet.cell_width);
    ASSERT_EQ(4u, u.sheet.cell_height);
    ASSERT_EQ(4u, u.edges.cell_width);
    ASSERT_EQ(4u, u.edges.cell_height);
    ASSERT_EQ(u.sheet.columns, u.edges.columns);
    ASSERT_EQ(u.sheet.rows, u.edges.rows);
    ASSERT_EQ(2, u.anchor_x);
    ASSERT_EQ(4, u.anchor_y);
    /* The figure in the middle of its cell. */
    ASSERT_EQ(0, cell_at(&u, 0, 0, 0, 0));
    ASSERT_EQ(5, cell_at(&u, 0, 0, 1, 1));
    ASSERT_EQ(5, cell_at(&u, 0, 0, 2, 2));
    ASSERT_EQ(0, cell_at(&u, 0, 0, 3, 3));
    /* The edge fills the whole of the same cell on its own sheet. */
    ASSERT_EQ(9, u.edges.pixels[0]);
    ASSERT_EQ(9, u.edges.pixels[3 * sheet_width(&u.edges) + 3]);
    /* And not the next cell, where the edge is a single pixel. */
    ASSERT_EQ(0, u.edges.pixels[4]);
    unit_sheet_free(&u);
    ASSERT_TRUE(u.edges.pixels == NULL);
}

UTEST(unit_sheet, build_mirrors_the_edges_like_the_figure)
{
    /* A flipped unit with 9 standing frames. The edge frames are 2 by 1
     * with their number on the left and 100 more on the right. */
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    for (uint8_t i = 0; i < 9; i++) {
        const uint8_t pixels[2] = {(uint8_t)(i + 1), (uint8_t)(i + 101)};
        make_sprite_solid(&m, 2, 1, 7);
        make_sprite_frame(&e, 2, 1, pixels);
    }
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    UnitLayout layout = flipped_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, &edges, 4, 4, 2, 2, &u));

    uint32_t cell = u.edges.cell_width;
    ASSERT_EQ(2u, cell);
    /* Step 1 as stored. */
    ASSERT_EQ(2, u.edges.pixels[1 * cell]);
    ASSERT_EQ(102, u.edges.pixels[1 * cell + 1]);
    /* Step 15 is frame 1 mirrored. */
    ASSERT_EQ(102, u.edges.pixels[15 * cell]);
    ASSERT_EQ(2, u.edges.pixels[15 * cell + 1]);
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_dying_takes_edges_too)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    for (int i = 0; i < 24; i++) {
        make_sprite_solid(&m, 1, 1, 3);
        make_sprite_solid(&e, 3, 1, 4);
    }
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    UnitLayout layout = full_layout(false, 0, 0, 1, 0, 0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build_dying(&layout, &sprite, &edges, 5, 1, 2, 0,
        &u));

    ASSERT_EQ(3u, u.sheet.cell_width);
    ASSERT_EQ(3u, u.edges.cell_width);
    ASSERT_EQ(8u, u.edges.rows);
    ASSERT_EQ(0, cell_at(&u, 0, 0, 0, 0));
    ASSERT_EQ(3, cell_at(&u, 0, 0, 1, 0));
    ASSERT_EQ(4, u.edges.pixels[0]);
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_refuses_edges_with_another_number_of_frames)
{
    static MakeSprite m;
    static MakeSprite e;
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_standing(&m, 2, 2, 5, &sprite));
    make_sprite_start(&e);
    for (int i = 0; i < 15; i++) {
        make_sprite_solid(&e, 1, 1, 1);
    }
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, &edges, 8, 8, 4, 4, &u));

    ASSERT_EQ(12345, u.anchor_x);
    ASSERT_TRUE(u.sheet.pixels == NULL);
    ASSERT_TRUE(u.edges.pixels == NULL);
}

UTEST(unit_sheet, build_refuses_an_edge_frame_larger_than_the_box)
{
    static MakeSprite m;
    static MakeSprite e;
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_standing(&m, 2, 2, 5, &sprite));
    ASSERT_TRUE(make_standing(&e, 9, 2, 9, &edges));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;
    memset(&u, 0, sizeof u);

    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, &edges, 8, 8, 4, 4, &u));
    ASSERT_TRUE(u.sheet.pixels == NULL);
}

/* ======== unit_toml ======== */

/* Two units:
 *
 *     Unit0   ID 3    a swordsman, who states everything he has
 *     Unit1   ID 14   an archer, a child of 3, who shoots
 */
static size_t make_two_units(MakeReg *m)
{
    const int32_t move_frames[] = {0, 1};
    const int32_t move_times[] = {2, 3};
    const int32_t attack_frames[] = {0, 1, 2};
    const int32_t attack_times[] = {2, 1, 2};
    const int32_t sounds[] = {100, 200};
    const int32_t shoot[] = {
        61, 90, 36, 80, 28, 58, 40, 39, 65, 30, 85, 40, 96, 60, 87, 81
    };

    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "UnitCount", 2);
    make_reg_section(m, "Unit0");
    make_reg_int(m, "ID", 3);
    make_reg_text(m, "DescText", "Human Swordsman");
    make_reg_text(m, "InfoPicture", "SwordsMan");
    make_reg_int(m, "InMapEditor", 1);
    make_reg_int(m, "CenterX", 64);
    make_reg_int(m, "CenterY", 78);
    make_reg_int(m, "SelectionX1", 48);
    make_reg_int(m, "SelectionY1", 48);
    make_reg_int(m, "SelectionX2", 80);
    make_reg_int(m, "SelectionY2", 90);
    make_reg_int_list(m, "MoveAnimFrame", move_frames, 2);
    make_reg_int_list(m, "MoveAnimTime", move_times, 2);
    make_reg_int_list(m, "AttackAnimFrame", attack_frames, 3);
    make_reg_int_list(m, "AttackAnimTime", attack_times, 3);
    make_reg_int(m, "AttackDelay", 4);
    make_reg_int_list(m, "Sound", sounds, 2);

    make_reg_section(m, "Unit1");
    make_reg_int(m, "ID", 14);
    make_reg_int(m, "Parent", 3);
    make_reg_text(m, "DescText", "Human Archer");
    make_reg_int(m, "ShootDelay", 21);
    make_reg_int(m, "Projectile", 1);
    make_reg_int_list(m, "ShootOffset", shoot, 16);
    return make_reg_finish(m);
}

/* A main sheet with 4 moving columns, of which `layout` says 2 are begin
 * frames, and 3 attack columns. No pixels: the writer does not look. */
static UnitSheet make_main_sheet(UnitLayout *layout)
{
    UnitSheet s;
    memset(&s, 0, sizeof s);
    memset(layout, 0, sizeof *layout);
    layout->move_begin = 2;
    s.sheet.cell_width = 88;
    s.sheet.cell_height = 72;
    s.anchor_x = 44;
    s.anchor_y = 50;
    s.first_direction_row = 1;
    s.column[UNIT_ANIM_MOVE] = 0;
    s.columns[UNIT_ANIM_MOVE] = 4;
    s.column[UNIT_ANIM_ATTACK] = 4;
    s.columns[UNIT_ANIM_ATTACK] = 3;
    s.column[UNIT_ANIM_IDLE] = 7;
    return s;
}

static UnitSheet make_dying_sheet(void)
{
    UnitSheet s;
    memset(&s, 0, sizeof s);
    s.sheet.cell_width = 48;
    s.sheet.cell_height = 64;
    s.anchor_x = 24;
    s.anchor_y = 46;
    s.columns[UNIT_ANIM_DYING] = 4;
    s.column[UNIT_ANIM_BONES] = 4;
    s.columns[UNIT_ANIM_BONES] = 3;
    return s;
}

/* ---- unit_toml_folder_name ---- */

UTEST(unit_toml, folder_name_is_the_id_and_the_name_in_lower_case)
{
    static MakeReg m;
    Reg reg;
    char name[UNIT_TOML_FOLDER_SIZE];
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_folder_name(&reg, 3, name));
    ASSERT_STREQ("003_human_swordsman", name);
    ASSERT_TRUE(unit_toml_folder_name(&reg, 14, name));
    ASSERT_STREQ("014_human_archer", name);
}

UTEST(unit_toml, folder_name_turns_other_characters_into_one_underscore)
{
    static MakeReg m;
    Reg reg;
    char name[UNIT_TOML_FOLDER_SIZE];
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 2);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 66);
    make_reg_text(&m, "DescText", " Ogre  (Lord) 2-handed! ");
    make_reg_section(&m, "Unit1");
    make_reg_int(&m, "ID", 7);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(unit_toml_folder_name(&reg, 66, name));
    ASSERT_STREQ("066_ogre_lord_2_handed", name);
    /* A unit without a name is its number alone. */
    ASSERT_TRUE(unit_toml_folder_name(&reg, 7, name));
    ASSERT_STREQ("007", name);
}

UTEST(unit_toml, folder_name_cuts_a_name_that_is_too_long)
{
    static MakeReg m;
    Reg reg;
    char text[200];
    char name[UNIT_TOML_FOLDER_SIZE + 1];
    memset(text, 'a', sizeof text - 1);
    text[sizeof text - 1] = '\0';
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 1);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 5);
    make_reg_text(&m, "DescText", text);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));
    memset(name, 'x', sizeof name);

    ASSERT_TRUE(unit_toml_folder_name(&reg, 5, name));

    ASSERT_EQ((size_t)UNIT_TOML_FOLDER_SIZE - 1, strlen(name));
    ASSERT_EQ('x', name[UNIT_TOML_FOLDER_SIZE]);
    ASSERT_EQ(0, strncmp("005_aaaa", name, 8));
}

UTEST(unit_toml, folder_name_refuses_an_unknown_or_unusable_id)
{
    static MakeReg m;
    Reg reg;
    char name[UNIT_TOML_FOLDER_SIZE] = "untouched";
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 2);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 1000);
    make_reg_section(&m, "Unit1");
    make_reg_int(&m, "ID", -1);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(unit_toml_folder_name(&reg, 4, name));
    ASSERT_FALSE(unit_toml_folder_name(&reg, 1000, name));
    ASSERT_FALSE(unit_toml_folder_name(&reg, -1, name));
    ASSERT_STREQ("untouched", name);
}

/* ---- unit_toml_write ---- */

UTEST(unit_toml, write_gives_the_whole_file_of_a_unit)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    UnitSheet dying = make_dying_sheet();
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 3, &layout, &sheet, &dying,
        NULL));

    ASSERT_TRUE(holds(&b,
        "# Human Swordsman\n"
        "id = 3\n"
        "name = \"Human Swordsman\"\n"
        "info_picture = \"SwordsMan\"\n"
        "in_map_editor = 1\n"
        "sounds = [100, 200]\n"
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "cell = [88, 72]\n"
        "anchor = [44, 50]\n"
        "selection = [-16, -30, 16, 12]\n"
        "\n"
        "[anim.move]\n"
        "column = 0\n"
        "columns = 4\n"
        "begin = 2\n"
        "frames = [0, 1]\n"
        "ticks = [2, 3]\n"
        "\n"
        "[anim.attack]\n"
        "column = 4\n"
        "columns = 3\n"
        "frames = [0, 1, 2]\n"
        "ticks = [2, 1, 2]\n"
        "delay = 4\n"
        "\n"
        "[dying]\n"
        "sheet = \"dying.png\"\n"
        "cell = [48, 64]\n"
        "anchor = [24, 46]\n"
        "dying = 4\n"
        "bones = 3\n"));
    buffer_free(&b);
}

UTEST(unit_toml, write_sprite_gives_the_part_about_the_pictures_alone)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    UnitSheet dying = make_dying_sheet();
    ByteBuffer whole = {0};
    ByteBuffer part = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));
    ASSERT_TRUE(unit_toml_write(&whole, &reg, 3, &layout, &sheet, &dying,
        NULL));

    ASSERT_TRUE(unit_toml_write_sprite(&part, &reg, 3, &layout, &sheet,
        &dying, NULL));

    /* It is the end of the whole file, from the table "sprite" on, and
     * has none of the unit's own lines. */
    const char *start = "\n[sprite]\n";
    ASSERT_TRUE(part.size > strlen(start) && part.size < whole.size);
    ASSERT_EQ(0, memcmp(part.data, start, strlen(start)));
    ASSERT_EQ(0, memcmp(part.data, whole.data + whole.size - part.size,
        part.size));
    ASSERT_EQ('\n', whole.data[whole.size - part.size - 1]);
    buffer_free(&whole);
    buffer_free(&part);
}

UTEST(unit_toml, write_sprite_refuses_a_unit_without_a_centre)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_FALSE(unit_toml_write_sprite(&b, &reg, 99, &layout, &sheet,
        NULL, NULL));
    buffer_free(&b);
}

UTEST(unit_toml, a_list_written_as_an_empty_text_is_an_empty_list)
{
    /* A unit that has its parent's attack lists, but states its own as
     * empty texts: its attack has columns and no frames or ticks. A
     * text that is not empty in the place of a list is still refused. */
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    const int32_t frames[] = {0, 1, 2};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 3);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 3);
    make_reg_int(&m, "CenterX", 64);
    make_reg_int(&m, "CenterY", 78);
    make_reg_int_list(&m, "AttackAnimFrame", frames, 3);
    make_reg_int_list(&m, "AttackAnimTime", frames, 3);
    make_reg_section(&m, "Unit1");
    make_reg_int(&m, "ID", 2);
    make_reg_int(&m, "Parent", 3);
    make_reg_text(&m, "AttackAnimFrame", "");
    make_reg_text(&m, "AttackAnimTime", "");
    make_reg_section(&m, "Unit2");
    make_reg_int(&m, "ID", 5);
    make_reg_int(&m, "Parent", 3);
    make_reg_text(&m, "AttackAnimFrame", "x");
    make_reg_text(&m, "AttackAnimTime", "x");
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(unit_toml_write_sprite(&b, &reg, 2, &layout, &sheet, NULL,
        NULL));

    ASSERT_TRUE(holds(&b,
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "cell = [88, 72]\n"
        "anchor = [44, 50]\n"
        "\n"
        "[anim.move]\n"
        "column = 0\n"
        "columns = 4\n"
        "begin = 2\n"
        "\n"
        "[anim.attack]\n"
        "column = 4\n"
        "columns = 3\n"));
    b.size = 0;
    ASSERT_FALSE(unit_toml_write_sprite(&b, &reg, 5, &layout, &sheet, NULL,
        NULL));
    buffer_free(&b);
}

UTEST(unit_toml, write_takes_missing_values_from_the_parent)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 14, &layout, &sheet, NULL,
        NULL));

    /* Its own. */
    ASSERT_TRUE(has(&b, "id = 14\n"));
    ASSERT_TRUE(has(&b, "name = \"Human Archer\"\n"));
    /* The swordsman's. */
    ASSERT_TRUE(has(&b, "info_picture = \"SwordsMan\"\n"));
    ASSERT_TRUE(has(&b, "selection = [-16, -30, 16, 12]\n"));
    ASSERT_TRUE(has(&b, "ticks = [2, 1, 2]\n"));
    /* No word of the parent itself. */
    ASSERT_FALSE(has(&b, "parent"));
    buffer_free(&b);
}

UTEST(unit_toml, write_counts_shots_from_the_centre)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 14, &layout, &sheet, NULL,
        NULL));

    /* The pairs 61, 90 and 36, 80 and so on, less the centre 64, 78. */
    ASSERT_TRUE(has(&b, "delay = 4\nshoot_delay = 21\nprojectile = 1\n"
        "shoot_x = [-3, -28, -36, -24, 1, 21, 32, 23]\n"
        "shoot_y = [12, 2, -20, -39, -48, -38, -18, 3]\n"));
    buffer_free(&b);
}

UTEST(unit_toml, write_without_a_dying_sheet_has_no_dying_part)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 3, &layout, &sheet, NULL,
        NULL));

    ASSERT_FALSE(has(&b, "[dying]"));
    ASSERT_FALSE(has(&b, "dying.png"));
    buffer_free(&b);
}

UTEST(unit_toml, write_leaves_out_an_animation_with_no_columns)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    sheet.columns[UNIT_ANIM_ATTACK] = 0;
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 3, &layout, &sheet, NULL,
        NULL));

    ASSERT_TRUE(has(&b, "[anim.move]"));
    ASSERT_FALSE(has(&b, "[anim.attack]"));
    ASSERT_FALSE(has(&b, "delay"));
    ASSERT_FALSE(has(&b, "[anim.idle]"));
    buffer_free(&b);
}

UTEST(unit_toml, write_of_an_animation_without_lists_gives_its_columns)
{
    /* Idle columns on the sheet, and no IdleAnimFrame in the registry. */
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    sheet.columns[UNIT_ANIM_IDLE] = 5;
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 3, &layout, &sheet, NULL,
        NULL));

    ASSERT_TRUE(has(&b, "\n[anim.idle]\ncolumn = 7\ncolumns = 5\n"));
    buffer_free(&b);
}

UTEST(unit_toml, write_refuses_a_unit_without_a_centre)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 1);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 3);
    make_reg_int(&m, "CenterX", 64);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(unit_toml_write(&b, &reg, 3, &layout, &sheet, NULL,
        NULL));
    /* And a unit that is not there at all. */
    ASSERT_FALSE(unit_toml_write(&b, &reg, 4, &layout, &sheet, NULL,
        NULL));
    buffer_free(&b);
}

UTEST(unit_toml, write_refuses_frame_and_tick_lists_of_different_length)
{
    const int32_t frames[] = {0, 1, 2};
    const int32_t times[] = {2, 2};
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 1);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 3);
    make_reg_int(&m, "CenterX", 64);
    make_reg_int(&m, "CenterY", 78);
    make_reg_int_list(&m, "MoveAnimFrame", frames, 3);
    make_reg_int_list(&m, "MoveAnimTime", times, 2);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(unit_toml_write(&b, &reg, 3, &layout, &sheet, NULL,
        NULL));
    buffer_free(&b);
}

UTEST(unit_toml, write_refuses_shots_that_are_not_8_pairs)
{
    const int32_t shoot[] = {1, 2, 3, 4};
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 1);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 3);
    make_reg_int(&m, "CenterX", 64);
    make_reg_int(&m, "CenterY", 78);
    make_reg_int_list(&m, "ShootOffset", shoot, 4);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(unit_toml_write(&b, &reg, 3, &layout, &sheet, NULL,
        NULL));
    buffer_free(&b);
}

UTEST(unit_toml, write_refuses_a_list_longer_than_the_limit)
{
    static int32_t sounds[UNIT_MAX_PHASES + 1];
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "UnitCount", 1);
    make_reg_section(&m, "Unit0");
    make_reg_int(&m, "ID", 3);
    make_reg_int(&m, "CenterX", 64);
    make_reg_int(&m, "CenterY", 78);
    make_reg_int_list(&m, "Sound", sounds, UNIT_MAX_PHASES + 1);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(unit_toml_write(&b, &reg, 3, &layout, &sheet, NULL,
        NULL));
    buffer_free(&b);
}

UTEST(unit_toml, write_names_the_palettes_of_both_sheets)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    UnitSheet dying = make_dying_sheet();
    const UnitTomlPalettes palettes = {
        "palettes.png", 5, false, "dying_palettes.png", 2
    };
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 3, &layout, &sheet, &dying,
        &palettes));

    ASSERT_TRUE(has(&b, "anchor = [44, 50]\n"
        "palettes = \"palettes.png\"\npalette_rows = 5\n"
        "palette_by = \"face\"\nselection"));
    ASSERT_TRUE(has(&b, "anchor = [24, 46]\n"
        "palettes = \"dying_palettes.png\"\npalette_rows = 2\n"
        "dying = 4\n"));
    buffer_free(&b);
}

UTEST(unit_toml, write_says_when_the_player_picks_the_palette)
{
    static MakeReg m;
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    const UnitTomlPalettes palettes = {
        "palettes.png", 16, true, "palettes.png", 16
    };
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 3, &layout, &sheet, NULL,
        &palettes));

    ASSERT_TRUE(has(&b, "palette_rows = 16\npalette_by = \"player\"\n"));
    /* No dying sheet, so nothing about its palettes either. */
    ASSERT_FALSE(has(&b, "[dying]"));
    buffer_free(&b);
}

UTEST(unit_toml, write_names_the_edges_of_a_sheet_that_has_them)
{
    static MakeReg m;
    static uint8_t pixels[1];
    Reg reg;
    UnitLayout layout;
    UnitSheet sheet = make_main_sheet(&layout);
    UnitSheet dying = make_dying_sheet();
    sheet.edges.pixels = pixels;
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_two_units(&m), &reg));

    ASSERT_TRUE(unit_toml_write(&b, &reg, 3, &layout, &sheet, &dying,
        NULL));

    ASSERT_TRUE(has(&b, "sheet = \"sprites.png\"\nedges = \"edges.png\"\n"
        "cell = [88, 72]\n"));
    /* The dying sheet has none here. */
    ASSERT_TRUE(has(&b, "sheet = \"dying.png\"\ncell = [48, 64]\n"));
    buffer_free(&b);

    dying.edges.pixels = pixels;
    ASSERT_TRUE(unit_toml_write(&b, &reg, 3, &layout, &sheet, &dying,
        NULL));
    ASSERT_TRUE(has(&b, "sheet = \"dying.png\"\n"
        "edges = \"dying_edges.png\"\ncell = [48, 64]\n"));
    buffer_free(&b);
}

/* ---- unit_toml_write_kinds ---- */

/* A data.bin with three monster kinds, two of them goblins (unit 64) and
 * one empty row between them, and two human kinds of unit 3. */
static void make_kinds(ByteBuffer *b)
{
    const char *const monster_columns[] = {
        "body", "healthMax", "typeID", "face", "not used", "serverID"
    };
    const char *const human_columns[] = {
        "Body", "typeID", "face", "gender ( is female? )", "serverID"
    };
    const int32_t goblin[] = {20, 40, 64, 1, -1, 600};
    const int32_t goblin2[] = {30, 80, 64, 2, -1, 601};
    const int32_t orc[] = {50, 90, 65, 1, -1, 610};
    const int32_t knight[] = {25, 3, 7, 0, 12};
    const int32_t lady[] = {15, 3, 9, 1, 13};

    put_empty_first_tables(b);
    put_header(b, monster_columns, 6);
    put_i32(b, 5);
    put_row(b, "Goblin_Pike", goblin, 6);
    put_text(b, "Bronze Pike");
    put_text(b, "");
    put_row(b, "", NULL, 0);
    put_text(b, "");
    put_text(b, "");
    put_row(b, "Orc", orc, 6);
    put_text(b, "");
    put_text(b, "");
    put_row(b, "Goblin_Pike.2", goblin2, 6);
    put_text(b, "Iron Pike");
    put_text(b, "Wooden Shield");

    put_header(b, human_columns, 5);
    put_i32(b, 3);
    put_row(b, "Knight", knight, 5);
    put_text(b, "Long Sword");
    for (int i = 1; i < 10; i++) {
        put_text(b, i == 4 ? "Helm" : "");
    }
    put_row(b, "Lady", lady, 5);
    for (int i = 0; i < 10; i++) {
        put_text(b, "");
    }
    put_empty_last_tables(b);
}

UTEST(unit_toml, write_kinds_gives_every_kind_of_the_unit)
{
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    uint32_t count = 99;
    make_kinds(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_TRUE(unit_toml_write_kinds(&b, &bin, 64, &count));

    ASSERT_EQ(2u, count);
    ASSERT_TRUE(holds(&b,
        "\n[[kind]]\n"
        "id = 600\n"
        "name = \"Goblin_Pike\"\n"
        "group = \"monster\"\n"
        "body = 20\n"
        "health = 40\n"
        "face = 1\n"
        "items = [\"Bronze Pike\"]\n"
        "\n[[kind]]\n"
        "id = 601\n"
        "name = \"Goblin_Pike.2\"\n"
        "group = \"monster\"\n"
        "body = 30\n"
        "health = 80\n"
        "face = 2\n"
        "items = [\"Iron Pike\", \"Wooden Shield\"]\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(unit_toml, write_kinds_gives_human_kinds_with_their_columns)
{
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    uint32_t count = 99;
    make_kinds(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_TRUE(unit_toml_write_kinds(&b, &bin, 3, &count));

    ASSERT_EQ(2u, count);
    ASSERT_TRUE(holds(&b,
        "\n[[kind]]\n"
        "id = 12\n"
        "name = \"Knight\"\n"
        "group = \"human\"\n"
        "body = 25\n"
        "face = 7\n"
        "female = 0\n"
        "items = [\"Long Sword\", \"Helm\"]\n"
        "\n[[kind]]\n"
        "id = 13\n"
        "name = \"Lady\"\n"
        "group = \"human\"\n"
        "body = 15\n"
        "face = 9\n"
        "female = 1\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(unit_toml, write_kinds_of_a_unit_without_kinds_adds_nothing)
{
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    uint32_t count = 99;
    make_kinds(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_TRUE(unit_toml_write_kinds(&b, &bin, 70, &count));
    ASSERT_EQ(0u, count);
    ASSERT_EQ(0u, b.size);
    /* The count may be left out. */
    ASSERT_TRUE(unit_toml_write_kinds(&b, &bin, 65, NULL));
    ASSERT_TRUE(has(&b, "id = 610\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(unit_toml, write_kinds_refuses_a_table_without_its_id_columns)
{
    /* Monsters with no "serverID" column. */
    const char *const columns[] = {"body", "typeID"};
    const char *const human_columns[] = {"typeID", "serverID"};
    const int32_t goblin[] = {20, 64};
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    uint32_t count = 99;
    put_empty_first_tables(&file);
    put_header(&file, columns, 2);
    put_i32(&file, 2);
    put_row(&file, "Goblin", goblin, 2);
    put_text(&file, "");
    put_text(&file, "");
    put_header(&file, human_columns, 2);
    put_i32(&file, 1);
    put_empty_last_tables(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_FALSE(unit_toml_write_kinds(&b, &bin, 64, &count));
    ASSERT_EQ(99u, count);
    buffer_free(&b);
    buffer_free(&file);
}
