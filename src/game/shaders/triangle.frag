#version 450

/* Each pixel gets the colour mixed from the three corners. */

layout(location = 0) in vec3 colour;
layout(location = 0) out vec4 pixel;

void main()
{
    pixel = vec4(colour, 1.0);
}
