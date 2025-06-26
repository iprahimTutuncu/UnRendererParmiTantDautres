#version 450

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D gPosition;
layout(set = 2, binding = 1) uniform sampler2D gNormal;
layout(set = 2, binding = 2) uniform sampler2D gAlbedo;

layout(set = 3, binding = 0) uniform Params
{
    int displayMode;
};

void main() 
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(gAlbedo, 0));
    
    vec3 albedo   = texture(gAlbedo,   uv).rgb;
    vec3 normal   = texture(gNormal,   uv).xyz; // maybe * 0.5 + 0.5; // [-1,1] -> [0,1]
    vec3 position = texture(gPosition, uv).xyz;
    
    vec3 color;

    if (displayMode == 3)
        color = albedo;
    else if (displayMode == 2)
        color = normal;
    else if (displayMode == 1)
        color = position * 0.05;
    else
        color = vec3(1.0, 0.5, 0.0); // pas valid bro

    outColor = vec4(color, 1.0);
}
