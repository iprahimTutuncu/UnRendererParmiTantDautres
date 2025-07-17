#version 450

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vEyeSpace;
layout(location = 2) in vec3 vNormal;
layout(location = 3) in vec2 vTexCoord;
layout(location = 4) flat in int vID;

layout(location = 0) out vec4 gPosition;   // xyz = world position, w = unused
layout(location = 1) out vec4 gNormal;     // xyz = normal,         w = unused
layout(location = 2) out vec4 gAlbedo;     // rgb = albedo color,   a = unused

// Sampler for albedo texture
layout(set = 2, binding = 0) uniform sampler2D tex_sampler;

layout(set = 3, binding = 0) uniform gBufferParticlesUniform 
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
    vec3 N;
    N.xy = vTexCoord*2.0-1.0;
    float r2 = dot(N.xy, N.xy);
    if(r2 > 1.0) discard;
    N.z = sqrt(1.0 - r2);

    //calcualte the depth
    vec3 eyePos = vEyeSpace + N * radius;
    vec4 clipPos = proj * vec4(eyePos, 1.0);

    gl_FragDepth = clipPos.z / clipPos.w;

    gPosition = vec4(vPosition, float(vID));
    gNormal = vec4(N, 1.0);
    gAlbedo = vec4(color.rgb, 1.0);
}
