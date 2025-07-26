#version 460 core

in vec4 gColor;

out vec4 Color;

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    Color = vec4(0.90f, 0.95f, 1.00f, 1.0f);
}
