#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 vPosition;   // world space position
layout(location = 1) out vec3 vEyeSpace;   // screen space position todo
layout(location = 2) out vec3 vNormal;     // world space normal
layout(location = 3) out vec2 vTexCoord;
layout(location = 4) out int vID;

layout(std140, set = 0, binding = 0) readonly buffer ParticlesBuffer
{
    vec4 positions[];
};

layout(set = 1, binding = 0) uniform gBufferParticlesUniform 
{
    mat4 proj;
    mat4 view;
    mat4 model;
    float radius;
    float near;
    float far;
    int id;
    vec4 color;
};

void main()
{
    vec3 instancePos = positions[gl_InstanceIndex].xyz;

    vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp    = vec3(view[0][1], view[1][1], view[2][1]);

    float scale = 1.f;
    vec3 billboardPos = instancePos
                      + camRight * position.x * scale
                      + camUp    * position.y * scale;

    vPosition = billboardPos;
    vEyeSpace = (view * vec4(billboardPos, 1.0)).xyz;
    vNormal = vec3(0.0, 0.0, 1.0);
    vTexCoord = texCoord;
    vID = id;

    gl_Position = proj * view * vec4(billboardPos, 1.0);
}