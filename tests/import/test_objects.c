#include "import/objects.h"

#include <string.h>

#include "make_reg.h"
#include "utest.h"

/* Four objects over two sprite files:
 *
 *     Object0   ID 0    a pine: file 0, 7 frames from frame 0
 *     Object1   ID 24   a rock: file 1, 1 frame at frame 3
 *     Object2   ID 2    a dead pine, which says 7 frames of a file that
 *                       will have 1
 *     Object3   ID 30   a child of the rock that states nothing more
 */
static size_t make_objects(MakeReg *m)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "ObjectCount", 4);
    make_reg_section(m, "Files");
    make_reg_text(m, "File0", "pine1\\sprites");
    make_reg_text(m, "File1", "stones\\sprites");

    make_reg_section(m, "Object0");
    make_reg_int(m, "ID", 0);
    make_reg_int(m, "File", 0);
    make_reg_int(m, "Index", 0);
    make_reg_int(m, "Phases", 7);

    make_reg_section(m, "Object1");
    make_reg_int(m, "ID", 24);
    make_reg_int(m, "File", 1);
    make_reg_int(m, "Index", 3);
    make_reg_int(m, "Phases", 1);

    make_reg_section(m, "Object2");
    make_reg_int(m, "ID", 2);
    make_reg_int(m, "File", 0);
    make_reg_int(m, "Phases", 7);

    make_reg_section(m, "Object3");
    make_reg_int(m, "ID", 30);
    make_reg_int(m, "Parent", 24);
    return make_reg_finish(m);
}

/* ---- objects_sprite_path ---- */

UTEST(objects, sprite_path_is_the_file_text_under_objects)
{
    static MakeReg m;
    Reg reg;
    char path[64];
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

    ASSERT_TRUE(objects_sprite_path(&reg, 0, path, sizeof path));
    ASSERT_STREQ("objects/pine1/sprites.256", path);
    ASSERT_TRUE(objects_sprite_path(&reg, 24, path, sizeof path));
    ASSERT_STREQ("objects/stones/sprites.256", path);
    /* The child uses its parent's file. */
    ASSERT_TRUE(objects_sprite_path(&reg, 30, path, sizeof path));
    ASSERT_STREQ("objects/stones/sprites.256", path);
}

UTEST(objects, sprite_path_refuses_what_it_cannot_build)
{
    static MakeReg m;
    Reg reg;
    char path[64] = "untouched";
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "ObjectCount", 2);
    make_reg_section(&m, "Files");
    make_reg_text(&m, "File0", "pine1\\sprites");
    make_reg_section(&m, "Object0");
    make_reg_int(&m, "ID", 0);
    make_reg_int(&m, "File", 5);
    make_reg_section(&m, "Object1");
    make_reg_int(&m, "ID", 1);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    /* A file number the registry has no text for, no file at all, and no
     * such object. */
    ASSERT_FALSE(objects_sprite_path(&reg, 0, path, sizeof path));
    ASSERT_FALSE(objects_sprite_path(&reg, 1, path, sizeof path));
    ASSERT_FALSE(objects_sprite_path(&reg, 9, path, sizeof path));
    ASSERT_STREQ("untouched", path);
}

UTEST(objects, sprite_path_refuses_a_path_that_does_not_fit)
{
    static MakeReg m;
    Reg reg;
    char path[32];
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));
    memset(path, 'x', sizeof path);

    /* "objects/pine1/sprites.256" is 25 characters: 26 bytes fit it. */
    ASSERT_FALSE(objects_sprite_path(&reg, 0, path, 25));
    ASSERT_EQ('x', path[0]);
    ASSERT_TRUE(objects_sprite_path(&reg, 0, path, 26));
    ASSERT_EQ('x', path[26]);
}

/* ---- objects_frame_range ---- */

UTEST(objects, frame_range_is_index_and_phases)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

    ASSERT_TRUE(objects_frame_range(&reg, 0, 7, &first, &count));
    ASSERT_EQ(0u, first);
    ASSERT_EQ(7u, count);
    ASSERT_TRUE(objects_frame_range(&reg, 24, 10, &first, &count));
    ASSERT_EQ(3u, first);
    ASSERT_EQ(1u, count);
}

UTEST(objects, frame_range_without_index_or_phases_is_one_frame_at_0)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "ObjectCount", 1);
    make_reg_section(&m, "Object0");
    make_reg_int(&m, "ID", 4);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(objects_frame_range(&reg, 4, 5, &first, &count));
    ASSERT_EQ(0u, first);
    ASSERT_EQ(1u, count);
}

UTEST(objects, frame_range_gives_no_more_frames_than_the_file_has)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

    /* The dead pine says 7 and its file has 1. */
    ASSERT_TRUE(objects_frame_range(&reg, 2, 1, &first, &count));
    ASSERT_EQ(0u, first);
    ASSERT_EQ(1u, count);
    /* The rock at frame 3 of a file of 4 keeps its one frame. */
    ASSERT_TRUE(objects_frame_range(&reg, 24, 4, &first, &count));
    ASSERT_EQ(3u, first);
    ASSERT_EQ(1u, count);
}

UTEST(objects, frame_range_refuses_a_first_frame_outside_the_file)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

    ASSERT_FALSE(objects_frame_range(&reg, 24, 3, &first, &count));
    ASSERT_FALSE(objects_frame_range(&reg, 0, 0, &first, &count));
    /* And an object that is not there. */
    ASSERT_FALSE(objects_frame_range(&reg, 77, 10, &first, &count));
    ASSERT_EQ(99u, first);
    ASSERT_EQ(99u, count);
}

UTEST(objects, frame_range_refuses_negative_numbers)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "ObjectCount", 2);
    make_reg_section(&m, "Object0");
    make_reg_int(&m, "ID", 1);
    make_reg_int(&m, "Index", -1);
    make_reg_section(&m, "Object1");
    make_reg_int(&m, "ID", 2);
    make_reg_int(&m, "Phases", -3);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(objects_frame_range(&reg, 1, 10, &first, &count));
    ASSERT_FALSE(objects_frame_range(&reg, 2, 10, &first, &count));
    ASSERT_EQ(99u, first);
}
