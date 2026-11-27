#include "import/projectile.h"

#include <string.h>

#include "base/buffer.h"
#include "expect.h"
#include "make_reg.h"
#include "make_sprite.h"
#include "utest.h"

/* ======== projectiles ======== */

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

/* ======== projectile_sheet ======== */

/* Where the pixel at `x`, `y` of the cell at `column`, `row` is in the
 * sheet's two blocks. */
static size_t at(const ProjectileSheet *s, uint32_t column, uint32_t row,
    uint32_t x, uint32_t y)
{
    size_t px = (size_t)column * s->cell_width + x;
    size_t py = (size_t)row * s->cell_height + y;
    return py * projectile_sheet_width(s) + px;
}

UTEST(projectile_sheet, build_16a_gives_colour_and_solidness)
{
    /* One direction, two pictures, in a box of 4 by 3. */
    ByteBuffer file = {0};
    add_16a_frame(&file, 10, 15, 20, 1);
    add_16a_frame(&file, 30, 8, 0, 15);
    Sprite256 sprite;
    ASSERT_TRUE(finish_16a(&file, 2, &sprite));
    ProjectileLayout layout = {2, 1, 1, 4, 3, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_16a(&layout, &sprite, &s));

    ASSERT_EQ(4u, s.cell_width);
    ASSERT_EQ(3u, s.cell_height);
    ASSERT_EQ(2u, s.columns);
    ASSERT_EQ(1u, s.rows);
    ASSERT_EQ(8u, projectile_sheet_width(&s));
    ASSERT_EQ(3u, projectile_sheet_height(&s));
    /* The frame is the middle of the box: at 1, 1. Around it nothing. */
    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 0, 1)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 1, 0)]);
    /* The file's 15 levels of solidness are spread up to 255. */
    ASSERT_EQ(10, s.pixels[at(&s, 0, 0, 1, 1)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1, 1)]);
    ASSERT_EQ(20, s.pixels[at(&s, 0, 0, 2, 1)]);
    ASSERT_EQ(17, s.solid[at(&s, 0, 0, 2, 1)]);
    ASSERT_EQ(30, s.pixels[at(&s, 1, 0, 1, 1)]);
    ASSERT_EQ(136, s.solid[at(&s, 1, 0, 1, 1)]);
    /* Colour 0 is a colour like any other in a .16a: the pixel is
     * there, and solid. */
    ASSERT_EQ(0, s.pixels[at(&s, 1, 0, 2, 1)]);
    ASSERT_EQ(255, s.solid[at(&s, 1, 0, 2, 1)]);
    projectile_sheet_free(&s);
    ASSERT_TRUE(s.pixels == NULL);
    ASSERT_TRUE(s.solid == NULL);
    buffer_free(&file);
}

UTEST(projectile_sheet, build_16a_writes_out_the_mirrored_directions)
{
    /* 9 directions of one picture. Frame n has colour n + 1 on the left
     * and n + 101 on the right. */
    ByteBuffer file = {0};
    for (uint32_t i = 0; i < 9; i++) {
        add_16a_frame(&file, i + 1, 15, i + 101, 15);
    }
    Sprite256 sprite;
    ASSERT_TRUE(finish_16a(&file, 9, &sprite));
    ProjectileLayout layout = {1, 9, 16, 2, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_16a(&layout, &sprite, &s));

    ASSERT_EQ(16u, s.rows);
    ASSERT_EQ(1u, s.columns);
    /* Direction 8 as stored. */
    ASSERT_EQ(9, s.pixels[at(&s, 0, 8, 0, 0)]);
    ASSERT_EQ(109, s.pixels[at(&s, 0, 8, 1, 0)]);
    /* Direction 9 is frame 7 mirrored: its right side is on the left. */
    ASSERT_EQ(108, s.pixels[at(&s, 0, 9, 0, 0)]);
    ASSERT_EQ(8, s.pixels[at(&s, 0, 9, 1, 0)]);
    /* Direction 15 is frame 1 mirrored. */
    ASSERT_EQ(102, s.pixels[at(&s, 0, 15, 0, 0)]);
    ASSERT_EQ(2, s.pixels[at(&s, 0, 15, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 15, 1, 0)]);
    projectile_sheet_free(&s);
    buffer_free(&file);
}

UTEST(projectile_sheet, build_16a_mirrors_about_the_middle_of_the_box)
{
    /* A frame 2 wide in a box 5 wide cannot be centred exactly: it sits
     * at 1, with 1 pixel free on its left and 2 on its right. Mirrored
     * it sits at 2. */
    ByteBuffer file = {0};
    for (uint32_t i = 0; i < 9; i++) {
        add_16a_frame(&file, i + 1, 15, i + 101, 15);
    }
    Sprite256 sprite;
    ASSERT_TRUE(finish_16a(&file, 9, &sprite));
    ProjectileLayout layout = {1, 9, 16, 5, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_16a(&layout, &sprite, &s));

    /* Direction 1 as stored. */
    ASSERT_EQ(0, s.solid[at(&s, 0, 1, 0, 0)]);
    ASSERT_EQ(2, s.pixels[at(&s, 0, 1, 1, 0)]);
    ASSERT_EQ(102, s.pixels[at(&s, 0, 1, 2, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 1, 3, 0)]);
    /* Direction 15, the same frame mirrored. */
    ASSERT_EQ(0, s.solid[at(&s, 0, 15, 1, 0)]);
    ASSERT_EQ(102, s.pixels[at(&s, 0, 15, 2, 0)]);
    ASSERT_EQ(2, s.pixels[at(&s, 0, 15, 3, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 15, 4, 0)]);
    projectile_sheet_free(&s);
    buffer_free(&file);
}

UTEST(projectile_sheet, build_16a_refuses_what_does_not_fit)
{
    ByteBuffer file = {0};
    add_16a_frame(&file, 1, 15, 2, 15);
    Sprite256 sprite;
    ASSERT_TRUE(finish_16a(&file, 1, &sprite));
    ProjectileSheet s;
    memset(&s, 0, sizeof s);
    s.columns = 12345;

    /* Two pictures wanted and one in the file. */
    ProjectileLayout two = {2, 1, 1, 4, 3, 0};
    ASSERT_FALSE(projectile_sheet_build_16a(&two, &sprite, &s));
    /* A box narrower than the frame, of no size, and too large. */
    ProjectileLayout narrow = {1, 1, 1, 1, 3, 0};
    ASSERT_FALSE(projectile_sheet_build_16a(&narrow, &sprite, &s));
    ProjectileLayout none = {1, 1, 1, 0, 3, 0};
    ASSERT_FALSE(projectile_sheet_build_16a(&none, &sprite, &s));
    ProjectileLayout huge = {1, 1, 1, 1025, 3, 0};
    ASSERT_FALSE(projectile_sheet_build_16a(&huge, &sprite, &s));
    ASSERT_EQ(12345u, s.columns);
    ASSERT_TRUE(s.pixels == NULL);
    /* A box of exactly the frame's size is fine. */
    ProjectileLayout exact = {1, 1, 1, 2, 1, 0};
    ASSERT_TRUE(projectile_sheet_build_16a(&exact, &sprite, &s));
    projectile_sheet_free(&s);
    buffer_free(&file);
}

/* ---- .256 projectiles ---- */

UTEST(projectile_sheet, build_256_puts_the_figure_solid_and_its_edge_soft)
{
    /* One picture, not turning. The figure is 1 by 1 of colour 7 and its
     * edge 3 by 1 of colour 9, in a box of 3 by 1. */
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    make_sprite_solid(&m, 1, 1, 7);
    make_sprite_solid(&e, 3, 1, 9);
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    ProjectileLayout layout = {1, 1, 1, 3, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_256(&layout, &sprite, &edges, &s));

    ASSERT_EQ(9, s.pixels[at(&s, 0, 0, 0, 0)]);
    ASSERT_EQ(PROJECTILE_SHEET_EDGE, s.solid[at(&s, 0, 0, 0, 0)]);
    /* Where both have a pixel, the figure is what shows. */
    ASSERT_EQ(7, s.pixels[at(&s, 0, 0, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1, 0)]);
    ASSERT_EQ(9, s.pixels[at(&s, 0, 0, 2, 0)]);
    ASSERT_EQ(PROJECTILE_SHEET_EDGE, s.solid[at(&s, 0, 0, 2, 0)]);
    projectile_sheet_free(&s);

    /* Without edges only the figure is there. */
    ASSERT_TRUE(projectile_sheet_build_256(&layout, &sprite, NULL, &s));
    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 0, 0)]);
    ASSERT_EQ(7, s.pixels[at(&s, 0, 0, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1, 0)]);
    projectile_sheet_free(&s);
}

UTEST(projectile_sheet, build_256_leaves_index_0_empty)
{
    static MakeSprite m;
    const uint8_t pixels[2] = {0, 5};
    make_sprite_start(&m);
    make_sprite_frame(&m, 2, 1, pixels);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ProjectileLayout layout = {1, 1, 1, 2, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_256(&layout, &sprite, NULL, &s));

    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 0, 0)]);
    ASSERT_EQ(5, s.pixels[at(&s, 0, 0, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1, 0)]);
    projectile_sheet_free(&s);
}

UTEST(projectile_sheet, build_256_lays_directions_in_rows)
{
    /* An arrow: 16 directions of one picture, each of its own colour. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 16; i++) {
        make_sprite_solid(&m, 1, 1, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ProjectileLayout layout = {1, 16, 16, 1, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_256(&layout, &sprite, NULL, &s));

    ASSERT_EQ(16u, s.rows);
    for (uint32_t step = 0; step < 16; step++) {
        ASSERT_EQ(step + 1, s.pixels[at(&s, 0, step, 0, 0)]);
    }
    projectile_sheet_free(&s);
}

UTEST(projectile_sheet, build_256_refuses_what_does_not_fit)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    make_sprite_solid(&m, 2, 2, 7);
    make_sprite_solid(&e, 2, 2, 9);
    make_sprite_solid(&e, 2, 2, 9);
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    ProjectileSheet s;
    memset(&s, 0, sizeof s);
    s.columns = 12345;

    /* Edges with another number of frames, too few frames, and a box
     * smaller than the frame. */
    ProjectileLayout fine = {1, 1, 1, 2, 2, 0};
    ASSERT_FALSE(projectile_sheet_build_256(&fine, &sprite, &edges, &s));
    ProjectileLayout two = {2, 1, 1, 2, 2, 0};
    ASSERT_FALSE(projectile_sheet_build_256(&two, &sprite, NULL, &s));
    ProjectileLayout low = {1, 1, 1, 2, 1, 0};
    ASSERT_FALSE(projectile_sheet_build_256(&low, &sprite, NULL, &s));
    ASSERT_EQ(12345u, s.columns);
    ASSERT_TRUE(s.pixels == NULL);
    ASSERT_TRUE(projectile_sheet_build_256(&fine, &sprite, NULL, &s));
    projectile_sheet_free(&s);
}

/* ======== projectile_toml ======== */

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
