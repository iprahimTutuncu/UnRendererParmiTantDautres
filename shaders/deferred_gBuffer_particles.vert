#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 vPosition;   // world space position
layout(location = 1) out vec3 vNormal;     // world space normal
layout(location = 2) out vec2 vTexCoord;

struct Particle {
    vec3 position;
    float mass;
    vec3 velocity;
    float volume_0;
    mat3 deform_elastic;
    mat3 deform_plastic;
    mat3 deform_affine;
};

struct GridNode {
    vec3 force;
    float mass;
    vec3 momentum;
    vec3 velocity_star;
    vec3 velocity;
};

layout(std430, set = 0, binding = 0) buffer ReadWriteBuffers {
    GridNode grid[512];
    Particle particles[];
};

layout(set = 1, binding = 0) uniform UBO 
{
    mat4 proj;
    mat4 view;
    mat4 model;
};

void main()
{
    vec3 instancePos = particles[gl_InstanceIndex].position;

    vec4 worldPos = vec4(position*0.2 + instancePos, 1.0);
    vPosition = worldPos.xyz;
    vNormal = normal;
    vTexCoord = texCoord;

    gl_Position = proj * view * worldPos;
}
