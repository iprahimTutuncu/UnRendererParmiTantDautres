#version 450

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D gPosition;
layout(set = 2, binding = 1) uniform sampler2D gNormal;
layout(set = 2, binding = 2) uniform sampler2D gAlbedo;

layout(set = 3, binding = 0) uniform UBO 
{
    mat4 proj;
    mat4 view;
    mat4 model;
    int id;
};

void generateRay(out vec3 ro, out vec3 rd)
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(gPosition, 0));
    vec2 ndc = uv * 2.0 - 1.0;

    //todo, precompute proj and view

    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewPos = inverse(proj) * clip;
    viewPos /= viewPos.w;
    vec4 worldPos = inverse(view) * viewPos;

    vec3 cameraPos = vec3(inverse(view)[3]);
    ro = cameraPos;
    rd = normalize(worldPos.xyz - cameraPos);
}

// plane degined by p (p.xyz must be normalized)
float plaIntersect( in vec3 ro, in vec3 rd, in vec4 p )
{
    return -(dot(ro,p.xyz)+p.w)/dot(rd,p.xyz);
}

vec3 renderChecker(vec3 hitPos)
{
    float scale = 1.0;
    vec2 uv = hitPos.xz * scale;
    float checker = mod(floor(uv.x) + floor(uv.y), 2.0);
    vec3 col = mix(vec3(0.4, 0.8, 0.9), vec3(0.4, 0.4, 0.1), checker);
    return col;
}

void main() 
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(gPosition, 0));
    vec3 albedo = texture(gAlbedo, uv).rgb;
    vec4 position = texture(gPosition, uv);

    vec3 color;
    bool isEmpty = length(position.xyz) < 1e-5;

    if (isEmpty)
    {
        vec3 ro, rd;
        generateRay(ro, rd);

        // y=0 plane: normal = vec3(0,1,0), distance = 0 => p = vec4(0,1,0,0)
        vec4 groundPlane = vec4(0.0, -1.0, 0.0, 0.0);
        float t = plaIntersect(ro, rd, groundPlane);

        if (t > 0.0)
        {
            vec3 hitPos = ro + t * rd;
            color = clamp((50.0-t), 0.0, 1.0) * renderChecker(hitPos);
        }
        else
        {
            color = vec3(0.0); // no hit, background
        }
    }
    else
    {
        color = albedo;
    }

    outColor = vec4(color, 1.0);
}
