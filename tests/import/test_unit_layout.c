#include "import/unit_layout.h"

#include "make_reg.h"
#include "utest.h"

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
