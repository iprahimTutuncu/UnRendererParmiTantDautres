#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 vPosition;   // world space position
layout(location = 1) out vec3 vNormal;     // world space normal
layout(location = 2) out vec2 vTexCoord;

layout(set = 1, binding = 0) uniform UBO 
{
    mat4 proj;
    mat4 view;
    mat4 model;
};

void main()
{
    vec4 worldPos = model * vec4(position, 1.0);
    vPosition = worldPos.xyz;

    // Normal matrix: inverse transpose of model matrix (no scale skew)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vNormal = normalize(normalMatrix * normal);

    vTexCoord = texCoord;

    gl_Position = proj * view * worldPos;
}
