#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 0) out vec3 fPosition;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 uProjMatrix;
    mat4 uViewMatrix;
    mat4 uModelMatrix;
} ubo;

void main()
{
    fPosition = (ubo.uModelMatrix * vec4(vPosition, 1.0)).xyz;
    gl_Position = ubo.uProjMatrix * ubo.uViewMatrix * ubo.uModelMatrix * vec4(vPosition, 1.0);
}
