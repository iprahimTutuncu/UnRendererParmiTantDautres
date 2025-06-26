#version 450

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D gPosition;
layout(set = 2, binding = 1) uniform sampler2D gNormal;
layout(set = 2, binding = 2) uniform sampler2D gAlbedo;

void main() 
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(gAlbedo, 0));
    
    vec3 albedo   = texture(gAlbedo,   uv).rgb;
    vec3 normal   = texture(gNormal,   uv).xyz * 0.5 + 0.5; // [-1,1] -> [0,1]
    vec3 position = texture(gPosition, uv).xyz;
    
    vec3 color;

    vec3 lightPos = vec3(0.0, 5.0, 0.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 surfaceColor = albedo;

    vec3 N = normalize(normal * 2.0 - 1.0);
    vec3 L = normalize(lightPos - position);

    float diff = max(dot(N, L), 0.0);
    color = surfaceColor * lightColor * diff;

    outColor = vec4(color, 1.0);
}
