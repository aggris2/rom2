#include "import/projectiles.h"

#include <string.h>

#include "make_reg.h"
#include "utest.h"

/* A registry with one projectile, ID 10. A number of -1 leaves its key
 * out. */
static size_t make_projectile(MakeReg *m, int32_t phases, int32_t rotation,
    int32_t flip, int32_t a16, int32_t width, int32_t height)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "Count", 1);
    make_reg_section(m, "Projectile0");
    make_reg_int(m, "ID", 10);
    make_reg_text(m, "File", "FireBolt\\sprites");
    if (phases >= 0) {
        make_reg_int(m, "Phases", phases);
    }
    if (rotation >= 0) {
        make_reg_int(m, "RotationPhases", rotation);
    }
    if (flip >= 0) {
        make_reg_int(m, "Flip", flip);
    }
    if (a16 >= 0) {
        make_reg_int(m, "A16", a16);
    }
    if (width >= 0) {
        make_reg_int(m, "Width", width);
    }
    if (height >= 0) {
        make_reg_int(m, "Height", height);
    }
    return make_reg_finish(m);
}

/* ---- projectiles_sprite_path ---- */

UTEST(projectiles, sprite_path_ends_in_16a_for_a_projectile_that_says_so)
{
    static MakeReg m;
    Reg reg;
    char path[64] = "untouched";
    bool is_16a = false;
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 4, -1, 1, 1, -1, -1), &reg));

    ASSERT_TRUE(projectiles_sprite_path(&reg, 10, path, sizeof path,
        &is_16a));
    ASSERT_STREQ("projectiles/firebolt/sprites.16a", path);
    ASSERT_TRUE(is_16a);
}

UTEST(projectiles, sprite_path_ends_in_256_otherwise)
{
    static MakeReg m;
    Reg reg;
    char path[64] = "untouched";
    bool is_16a = true;
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 1, -1, -1, -1, -1, -1), &reg));

    ASSERT_TRUE(projectiles_sprite_path(&reg, 10, path, sizeof path,
        &is_16a));
    ASSERT_STREQ("projectiles/firebolt/sprites.256", path);
    ASSERT_FALSE(is_16a);

    /* "A16" of 0 is the same as none. */
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 1, -1, -1, 0, -1, -1), &reg));
    is_16a = true;
    ASSERT_TRUE(projectiles_sprite_path(&reg, 10, path, sizeof path,
        &is_16a));
    ASSERT_FALSE(is_16a);
}

UTEST(projectiles, sprite_path_refuses_what_it_cannot_build)
{
    static MakeReg m;
    Reg reg;
    char path[64] = "untouched";
    bool is_16a = true;
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 1, -1, -1, -1, -1, -1), &reg));

    /* No such projectile, and a path of 32 characters with room for 31. */
    ASSERT_FALSE(projectiles_sprite_path(&reg, 11, path, sizeof path,
        &is_16a));
    ASSERT_FALSE(projectiles_sprite_path(&reg, 10, path, 32, &is_16a));
    ASSERT_STREQ("untouched", path);
    ASSERT_TRUE(is_16a);
    ASSERT_TRUE(projectiles_sprite_path(&reg, 10, path, 33, &is_16a));
}

/* ---- projectiles_layout ---- */

UTEST(projectiles, layout_without_rotation_or_flip_has_16_directions)
{
    /* An arrow: one picture in each of 16 directions. */
    static MakeReg m;
    Reg reg;
    ProjectileLayout l;
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 1, -1, -1, -1, -1, -1), &reg));

    ASSERT_TRUE(projectiles_layout(&reg, 10, 16, &l));

    ASSERT_EQ(1u, l.phases);
    ASSERT_EQ(16u, l.stored_steps);
    ASSERT_EQ(16u, l.steps);
    ASSERT_EQ(64u, l.width);
    ASSERT_EQ(64u, l.height);
    ASSERT_EQ(0u, l.spare_frames);
}

UTEST(projectiles, layout_with_flip_stores_9_directions_and_shows_16)
{
    static MakeReg m;
    Reg reg;
    ProjectileLayout l;
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 4, -1, 1, 1, -1, -1), &reg));

    ASSERT_TRUE(projectiles_layout(&reg, 10, 36, &l));

    ASSERT_EQ(4u, l.phases);
    ASSERT_EQ(9u, l.stored_steps);
    ASSERT_EQ(16u, l.steps);
}

UTEST(projectiles, layout_with_one_rotation_phase_does_not_turn)
{
    static MakeReg m;
    Reg reg;
    ProjectileLayout l;
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 11, 1, -1, 1, 128, 96), &reg));

    ASSERT_TRUE(projectiles_layout(&reg, 10, 11, &l));

    ASSERT_EQ(11u, l.phases);
    ASSERT_EQ(1u, l.stored_steps);
    ASSERT_EQ(1u, l.steps);
    ASSERT_EQ(128u, l.width);
    ASSERT_EQ(96u, l.height);
}

UTEST(projectiles, layout_counts_the_frames_the_numbers_do_not_use)
{
    /* The healing sparkle: 7 pictures in a file of 8. */
    static MakeReg m;
    Reg reg;
    ProjectileLayout l;
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 7, 1, -1, 1, 16, 16), &reg));

    ASSERT_TRUE(projectiles_layout(&reg, 10, 8, &l));
    ASSERT_EQ(1u, l.spare_frames);
}

UTEST(projectiles, layout_refuses_a_file_with_too_few_frames)
{
    static MakeReg m;
    Reg reg;
    ProjectileLayout l;
    memset(&l, 0x55, sizeof l);
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 4, -1, 1, 1, -1, -1), &reg));

    ASSERT_FALSE(projectiles_layout(&reg, 10, 35, &l));
    ASSERT_FALSE(projectiles_layout(&reg, 10, 0, &l));
    ASSERT_EQ(0x55555555u, l.phases);
    /* And a projectile that is not there. */
    ASSERT_FALSE(projectiles_layout(&reg, 11, 36, &l));
}

UTEST(projectiles, layout_refuses_numbers_that_are_not_usable)
{
    static MakeReg m;
    Reg reg;
    ProjectileLayout l;
    /* No "Phases", none, and too many. */
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, -1, 1, -1, -1, -1, -1), &reg));
    ASSERT_FALSE(projectiles_layout(&reg, 10, 100, &l));
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 0, 1, -1, -1, -1, -1), &reg));
    ASSERT_FALSE(projectiles_layout(&reg, 10, 100, &l));
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 257, 1, -1, -1, -1, -1), &reg));
    ASSERT_FALSE(projectiles_layout(&reg, 10, 1000, &l));
    /* A number of directions that is not 1, 9 or 16. */
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 1, 8, -1, -1, -1, -1), &reg));
    ASSERT_FALSE(projectiles_layout(&reg, 10, 100, &l));
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 1, 0, -1, -1, -1, -1), &reg));
    ASSERT_FALSE(projectiles_layout(&reg, 10, 100, &l));
    /* A box of no size. */
    ASSERT_TRUE(reg_open(m.bytes,
        make_projectile(&m, 1, 1, -1, -1, 0, 16), &reg));
    ASSERT_FALSE(projectiles_layout(&reg, 10, 100, &l));
}

/* ---- projectiles_frame ---- */

UTEST(projectiles, frame_is_direction_times_pictures_plus_picture)
{
    ProjectileLayout l = {4, 16, 16, 64, 64, 0};
    uint32_t frame = 99;
    bool mirror = true;

    ASSERT_TRUE(projectiles_frame(&l, 0, 0, &frame, &mirror));
    ASSERT_EQ(0u, frame);
    ASSERT_FALSE(mirror);
    ASSERT_TRUE(projectiles_frame(&l, 3, 2, &frame, &mirror));
    ASSERT_EQ(14u, frame);
    ASSERT_TRUE(projectiles_frame(&l, 15, 3, &frame, &mirror));
    ASSERT_EQ(63u, frame);
    ASSERT_FALSE(mirror);
}

UTEST(projectiles, frame_past_the_stored_directions_is_a_mirror_image)
{
    ProjectileLayout l = {4, 9, 16, 64, 64, 0};
    uint32_t frame = 99;
    bool mirror = true;

    /* Direction 8 is the last stored one. */
    ASSERT_TRUE(projectiles_frame(&l, 8, 1, &frame, &mirror));
    ASSERT_EQ(33u, frame);
    ASSERT_FALSE(mirror);
    /* 9 is 7 mirrored, and 15 is 1 mirrored. */
    ASSERT_TRUE(projectiles_frame(&l, 9, 1, &frame, &mirror));
    ASSERT_EQ(29u, frame);
    ASSERT_TRUE(mirror);
    ASSERT_TRUE(projectiles_frame(&l, 15, 0, &frame, &mirror));
    ASSERT_EQ(4u, frame);
    ASSERT_TRUE(mirror);
}

UTEST(projectiles, frame_refuses_what_the_layout_does_not_have)
{
    ProjectileLayout turning = {4, 9, 16, 64, 64, 0};
    ProjectileLayout still = {5, 1, 1, 16, 16, 0};
    uint32_t frame = 99;
    bool mirror = true;

    ASSERT_FALSE(projectiles_frame(&turning, 16, 0, &frame, &mirror));
    ASSERT_FALSE(projectiles_frame(&turning, 0, 4, &frame, &mirror));
    ASSERT_FALSE(projectiles_frame(&still, 1, 0, &frame, &mirror));
    ASSERT_EQ(99u, frame);
    ASSERT_TRUE(projectiles_frame(&still, 0, 4, &frame, &mirror));
    ASSERT_EQ(4u, frame);
    ASSERT_FALSE(mirror);
}
