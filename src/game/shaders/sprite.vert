#version 450

/* A corner of a sprite: where it goes on the screen, and which point of
 * its sheet is there. Both are in pixels. */

layout(set = 1, binding = 0) uniform Screen {
    vec2 size;      /* of the screen, in pixels */
} screen;

layout(location = 0) in vec2 place;     /* on the screen */
layout(location = 1) in vec2 point;     /* in the sheet */

layout(location = 0) out vec2 at;

void main()
{
    at = point;
    /* The screen runs from -1 to 1, with 1 at the top. */
    vec2 spot = place / screen.size * 2.0 - 1.0;
    gl_Position = vec4(spot.x, -spot.y, 0.0, 1.0);
}
