#version 450

/* One triangle in the middle of the screen, with a colour at each
 * corner. The corners are worked out from the number of the vertex, so
 * nothing has to be handed in. */

layout(location = 0) out vec3 colour;

void main()
{
    const vec2 places[3] = vec2[3](
        vec2(0.0, 0.6), vec2(-0.6, -0.6), vec2(0.6, -0.6));
    const vec3 colours[3] = vec3[3](
        vec3(1.0, 0.2, 0.2), vec3(0.2, 1.0, 0.2), vec3(0.2, 0.4, 1.0));
    gl_Position = vec4(places[gl_VertexIndex], 0.0, 1.0);
    colour = colours[gl_VertexIndex];
}
