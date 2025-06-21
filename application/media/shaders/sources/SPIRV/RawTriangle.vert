#version 450

layout(location = 0) out vec4 outColor;

// Uniform block for transformation matrices
layout(set = 1, binding = 0) uniform UBO 
{
    mat4 proj;
    mat4 view;
    mat4 model;
};

void main()
{
    vec4 pos;
    uint vertexIndex = gl_VertexIndex;

    if (vertexIndex == 0) 
    {
        pos = vec4(-1.0, -1.0, 0.0, 1.0);
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    } 
    
    else if (vertexIndex == 1)
    {
        pos = vec4(1.0, -1.0, 0.0, 1.0);
        outColor = vec4(0.0, 1.0, 0.0, 1.0);
    } 
    
    else if (vertexIndex == 2) 
    {
        pos = vec4(0.0, 1.0, 0.0, 1.0);
        outColor = vec4(0.0, 0.0, 1.0, 1.0);
    }

    gl_Position = proj * view * model * pos;
}
