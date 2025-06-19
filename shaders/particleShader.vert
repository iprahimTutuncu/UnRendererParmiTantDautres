#version 460

layout(location=0) out vec4 gColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 uProjMatrix;
    mat4 uViewMatrix;
};

struct ParticleInstance {
    vec3 position;
    float _p1;
};

layout(std430, binding = 1) buffer particleInstances
{
    ParticleInstance instances[];
};

void main()
{
    ParticleInstance instance = instances[gl_InstanceIndex];
    mat4 VPMatrix = uProjMatrix * uViewMatrix;

    gl_PointSize = 8.0;  // Requires enabling widePoints in pipeline config
    gl_Position = VPMatrix * vec4(instance.position, 1.0);
    gColor = vec4(1.0);
}
