#version 460

layout(set = 0, location = 0) in vec3 vPosition;
layout(set = 0, location = 1) in vec4 vColor;

layout(set = 0, location = 0) out vec4 fColor;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    fColor = vColor;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(vPosition, 1.0);
}
