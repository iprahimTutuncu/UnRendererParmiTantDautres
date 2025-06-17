#version 460 core

out vec4 gColor;

uniform mat4 uProjMatrix;
uniform mat4 uViewMatrix;

struct ParticleInstance {
    vec3 position;
    float _p1;
};

layout(std430, binding = 0) buffer particleInstances
{
    ParticleInstance instances[];
};

void main()
{
    ParticleInstance instance = instances[gl_VertexID];
    mat4 VPMatrix = uProjMatrix * uViewMatrix;
//    gPosition = instance.position;
    gl_PointSize = 8.0f;
    gl_Position = VPMatrix * vec4(instance.position, 1.0f);
    gColor = vec4(1.0f);
}
