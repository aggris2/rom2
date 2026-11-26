#include "import/unit_toml.h"

#include <string.h>

#include "make_reg.h"
#include "utest.h"

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

/* True if the buffer holds exactly the text `expected`. */
static bool holds(const ByteBuffer *b, const char *expected)
{
    size_t length = strlen(expected);
    return b->size == length && memcmp(b->data, expected, length) == 0;
}

/* True if the text `part` is somewhere in the buffer. */
static bool has(ByteBuffer *b, const char *part)
{
    if (!buffer_push(b, 0)) {
        return false;
    }
    b->size--;
    return strstr((const char *)b->data, part) != NULL;
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
