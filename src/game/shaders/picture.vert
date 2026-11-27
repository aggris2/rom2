#version 450

/* A picture as a rectangle, at its own size, with its top left corner
 * in the top left corner of the screen. The four corners are worked out
 * from the number of the vertex. */

layout(set = 1, binding = 0) uniform Place {
    vec2 screen;    /* the size of the screen, in pixels */
    vec2 size;      /* the size of the picture, in pixels */
} place;

/* Where in the picture this is, in pixels. */
layout(location = 0) out vec2 at;

void main()
{
    vec2 corner = vec2(gl_VertexIndex & 1, gl_VertexIndex >> 1);
    at = corner * place.size;
    /* The screen runs from -1 to 1, with 1 at the top. */
    vec2 spot = at / place.screen * 2.0 - 1.0;
    gl_Position = vec4(spot.x, -spot.y, 0.0, 1.0);
}
