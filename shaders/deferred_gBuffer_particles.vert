#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 vPosition;   // world space position
layout(location = 1) out vec3 vNormal;     // world space normal
layout(location = 2) out vec2 vTexCoord;

layout(std140, set = 0, binding = 0) readonly buffer ParticlesBuffer
{
    vec4 positions[];
};

layout(set = 1, binding = 0) uniform UBO 
{
    mat4 proj;
    mat4 view;
    mat4 model;
};

void main()
{
    vec3 instancePos = positions[gl_InstanceIndex].xyz;

    vec4 worldPos = vec4(position*0.2 + instancePos, 1.0);
    vPosition = worldPos.xyz;
    vNormal = normal;
    vTexCoord = texCoord;

    gl_Position = proj * view * worldPos;
}