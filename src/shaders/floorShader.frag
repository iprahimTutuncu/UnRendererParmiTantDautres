#version 460 core

out vec4 FragColor;

in vec3 fPosition;

vec3 color1 = vec3(0.6, 0.6, 0.6);
vec3 color2 = vec3(0.3, 0.3, 0.3);

float tile_scale = 2.0;

void main()
{
    vec2 pos = fPosition.xz * tile_scale;
    float checker = mod(floor(pos.x) + floor(pos.y), 2.0);

    FragColor = vec4(color1, 1.0);

    if (checker < 0.5) {
        FragColor = vec4(color1, 1.0);
    }
    else {
        FragColor = vec4(color2, 1.0);
    }
}
