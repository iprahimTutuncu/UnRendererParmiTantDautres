#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) out vec4 gColor;

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

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 uProjMatrix;
    mat4 uViewMatrix;
};

layout(std430, set = 1, binding = 0) buffer ReadWriteBuffers {
    GridNode grid[512];
    Particle particles[];
};

void main()
{
    Particle instance = particles[gl_InstanceIndex];
    mat4 VPMatrix = uProjMatrix * uViewMatrix;

    gl_PointSize = 8.0;  // Requires enabling widePoints in pipeline config
    gl_Position = VPMatrix * vec4(instance.position, 1.0);
    gColor = vec4(1.0);
}
