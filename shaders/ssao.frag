#version 450
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D gPosition;
layout(set = 2, binding = 1) uniform sampler2D gNormal;
layout(set = 2, binding = 2) uniform sampler2D textureNoise;

layout(set = 2, binding = 3) uniform KernelUBO {
    vec4 samples[64]; // padded for std140
};

layout(push_constant) uniform SSAOParams {
    mat4 projection;
    mat4 view;
    float radius;
    float bias;
    float screenWidth;
    float screenHeight;
    int kernelSize;
} ubo;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(ubo.screenWidth, ubo.screenHeight);
    vec3 fragPos = (ubo.view * vec4(texture(gPosition, uv).xyz, 1.0)).xyz;
    vec3 normal = normalize((transpose(inverse(ubo.view)) * vec4(texture(gNormal, uv).xyz, 0.0)).xyz);

    vec2 noiseScale = vec2(ubo.screenWidth / 4.0, ubo.screenHeight / 4.0);
    vec3 randomVec = texture(textureNoise, uv * noiseScale).xyz;

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < ubo.kernelSize; ++i) {
        vec3 sampleVec = TBN * samples[i].xyz;
        vec3 samplePos = fragPos + sampleVec * ubo.radius;

        vec4 offset = ubo.projection * vec4(samplePos, 1.0);
        offset.xy /= offset.w;
        vec2 sampleUV = offset.xy * 0.5 + 0.5;

        float sampleDepth = (ubo.view * vec4(texture(gPosition, sampleUV).xyz, 1.0)).z;
        float rangeCheck = smoothstep(0.0, 1.0, ubo.radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + ubo.bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(ubo.kernelSize));
    outColor = vec4(vec3(occlusion), 1.0);
}
