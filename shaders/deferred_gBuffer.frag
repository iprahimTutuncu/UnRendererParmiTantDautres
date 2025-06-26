#version 450

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

layout(location = 0) out vec4 gPosition;   // xyz = world position, w = unused
layout(location = 1) out vec4 gNormal;     // xyz = normal,         w = unused
layout(location = 2) out vec4 gAlbedo;     // rgb = albedo color,   a = unused

// Sampler for albedo texture
layout(set = 2, binding = 0) uniform sampler2D tex_sampler;

void main()
{
    vec3 albedo = texture(tex_sampler, vTexCoord).rgb;

    gPosition = vec4(vPosition, 1.0);
    gNormal = vec4(normalize(vNormal), 1.0);
    gAlbedo = vec4(albedo, 1.0);
}
