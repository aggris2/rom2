#version 450

/* A corner of the part of the screen a square of ground may cover.
 * Every square is one record, GroundSquare in view/frame.h, and its
 * four corners are made here: top left, top right, low left, low
 * right. Which pixels of that part are the square's is for ground.frag
 * to say. */

layout(set = 1, binding = 0) uniform Screen {
    vec2 size;      /* of the screen, in pixels */
    int square;     /* the side of a square of ground, in pixels */
} screen;

layout(location = 0) in ivec4 corner_y;  /* of its corners, on the screen */
layout(location = 1) in ivec2 left;      /* its left edge, and its
                                            mark */
layout(location = 2) in uvec2 tile;      /* its tile's corner in the sheet */
layout(location = 3) in uvec4 light;     /* the level at its corners */

layout(location = 0) flat out ivec4 corners;
layout(location = 1) flat out ivec3 from;
layout(location = 2) flat out ivec4 levels;
layout(location = 3) flat out int mark;

void main()
{
    corners = corner_y;
    from = ivec3(left.x, ivec2(tile));
    levels = ivec4(light);
    mark = left.y;
    /* From its highest top corner to its lowest low corner. */
    int right = gl_VertexIndex & 1;
    int low = gl_VertexIndex >> 1;
    vec2 place = vec2(left.x + right * screen.square, low == 0
        ? min(corner_y.x, corner_y.y) : max(corner_y.z, corner_y.w));
    /* The screen runs from -1 to 1, with 1 at the top. */
    vec2 spot = place / screen.size * 2.0 - 1.0;
    gl_Position = vec4(spot.x, -spot.y, 0.0, 1.0);
}
