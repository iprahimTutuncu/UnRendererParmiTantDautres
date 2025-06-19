#version 460

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 fPosition;

vec3 color1 = vec3(0.6, 0.6, 0.6);
vec3 color2 = vec3(0.3, 0.3, 0.3);

float tile_scale = 4.0;

void main()
{
    vec2 pos = fPosition.xz * tile_scale;
    float checker = mod(floor(pos.x) + floor(pos.y), 2.0);

    fragColor = vec4(color1, 1.0);

    if (checker < 0.5) {
        fragColor = vec4(color1, 1.0);
    }
    else {
        fragColor = vec4(color2, 1.0);
    }
}
