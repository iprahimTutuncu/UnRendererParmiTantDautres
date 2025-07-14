#version 460 core

layout (location = 0) in vec3 vPosition;

out vec3 fPosition;

uniform mat4 uProjMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uModelMatrix;

void main()
{
    fPosition = (uModelMatrix * vec4(vPosition, 1.0)).xyz;
    gl_Position = uProjMatrix * uViewMatrix * uModelMatrix * vec4(vPosition, 1.0f);
}
