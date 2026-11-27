#include "view/camera.h"

#include "utest.h"

static Camera camera_of(int32_t width, int32_t height)
{
    Camera camera = {0, 0, width, height};
    return camera;
}

UTEST(camera, look_at_puts_the_point_in_the_middle_of_the_screen)
{
    Camera camera = camera_of(640, 480);
    camera_look_at(&camera, 1000, 2000);
    EXPECT_EQ(1000 - 320, camera.left);
    EXPECT_EQ(2000 - 240, camera.top);
    EXPECT_EQ(640, camera.width);
    EXPECT_EQ(480, camera.height);
}

UTEST(camera, a_camera_inside_the_map_stays_where_it_is)
{
    /* 80 squares: what is shown runs from pixel 256 to 2304. */
    Camera camera = camera_of(640, 480);
    camera.left = 256;
    camera.top = 1000;
    camera_keep_inside(&camera, 80, 80);
    EXPECT_EQ(256, camera.left);
    EXPECT_EQ(1000, camera.top);
    camera.left = 2304 - 640;
    camera.top = 2304 - 480;
    camera_keep_inside(&camera, 80, 80);
    EXPECT_EQ(2304 - 640, camera.left);
    EXPECT_EQ(2304 - 480, camera.top);
}

UTEST(camera, is_moved_back_from_the_edge_that_is_never_shown)
{
    Camera camera = camera_of(640, 480);
    camera.left = 255;
    camera.top = -5000;
    camera_keep_inside(&camera, 80, 80);
    EXPECT_EQ(256, camera.left);
    EXPECT_EQ(256, camera.top);
    camera.left = 2304 - 640 + 1;
    camera.top = 99999;
    camera_keep_inside(&camera, 80, 80);
    EXPECT_EQ(2304 - 640, camera.left);
    EXPECT_EQ(2304 - 480, camera.top);
}

UTEST(camera, each_side_is_kept_inside_by_its_own_size)
{
    /* A map of 80 by 144 squares. */
    Camera camera = camera_of(640, 480);
    camera.left = 99999;
    camera.top = 99999;
    camera_keep_inside(&camera, 80, 144);
    EXPECT_EQ((80 - 8) * 32 - 640, camera.left);
    EXPECT_EQ((144 - 8) * 32 - 480, camera.top);
}

UTEST(camera, a_map_smaller_than_the_screen_is_put_in_the_middle)
{
    /* 20 squares show 4 of them, 128 pixels, from pixel 256 on. On a
     * screen of 1000 that leaves 436 on each side. */
    Camera camera = camera_of(1000, 480);
    camera.left = 12345;
    camera.top = 400;
    camera_keep_inside(&camera, 20, 80);
    EXPECT_EQ(256 - 436, camera.left);
    EXPECT_EQ(400, camera.top);
    /* And a screen exactly as large as what is shown has one place. */
    Camera exact = camera_of(128, 128);
    exact.left = 7;
    exact.top = 9999;
    camera_keep_inside(&exact, 20, 20);
    EXPECT_EQ(256, exact.left);
    EXPECT_EQ(256, exact.top);
}
