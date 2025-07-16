#pragma once

#include "../state.h"
#include "../vmath.h"

#include <SDL3/SDL_init.h>
#include <vector>

union SDL_Event;

SDL_AppResult graphics_init(AppState& state, int argc, char** argv);
SDL_AppResult graphics_iterate(AppState& state);
SDL_AppResult graphics_event(AppState& state, SDL_Event& event);
void graphics_quit(AppState& state);

enum GraphicPipelineIndex {
    GeometryBufferFillPipeline,
    GeometryBufferLinePipeline,
    GeometryBufferParticleFillPipeline,
    GeometryBufferParticleLinePipeline,
    DeferredLightingPipeline,
    DeferredDebugPipeline,
    NumGraphicPipelines // must be last
};

enum ComputePipelineIndex {
    ParticleUpdate,
    ParticleBilateralBlur,
    NumComputePipelines // must be last
};

enum BufferIndex {
    BoxVertexBuffer,
    BoxIndexBuffer,
    ParticlePositionBuffer, // TODO: this has to be the MPM and be renamed
    ParticlesVertexBuffer,
    ParticlesIndexBuffer,
    SphereVertexBuffer,
    SphereIndexBuffer,
    NumBuffers // must be last
};

enum TextureIndex {
    GeometryPosition,
    GeometryNormal,
    GeometryAlbedo,
    GeometryDepth,
    GeometryDepthModified,
    DefaultWhite,
    NumTextures // must be last
};

enum SamplerPreset {
    PointClamp,
    PointWrap,
    LinearClamp,
    LinearWrap,
    AnisotropicClamp,
    AnisotropicWrap,
    NumSamplers // must be last
};

enum DisplayMode {
    Final,
    Position,
    Normal,
    Albedo,
    Depth
};

enum class ViewType {
    Particles,
    Mesh
};

enum RasterMode {
    RasterMode_Fill,
    RasterMode_Line
};

struct Box {
    alignas(16) float min[3];
    alignas(16) float max[3];
};

struct Rect {
    float centerX;
    float centerY;
    float halfWidth;
    float halfHeight;
};

struct Particle {
    alignas(16) vec4 position;
    alignas(16) vec4 color;
};

struct Vertex {
    alignas(16) float position[3];
    alignas(16) float normal[3];
    alignas(16) float texCoord[2];
};

struct SDL_GPUGraphicsPipeline;
struct SDL_GPUComputePipeline;
struct SDL_GPUBuffer;
struct SDL_GPUTexture;
struct SDL_GPUSampler;

// c'est le uniform buffer pour le compute

struct ParticleUpdateUniform {
    float time;
};

struct GeometryBufferUniform {
    mat4 proj;
    mat4 view;
    mat4 model;
    int id;
    int p0, p1, p2;
};

struct BilateralBlurBufferUniform {
    int filterRadius;
    float blurScale; 
    float blurDepthFalloff;
    int p0, p1, p2;
};

struct GeometryBufferParticlesUniform {
    mat4 proj;
    mat4 view;
    mat4 model;
    float radius;
    int id;
    int p0, p1;
    vec4 color;
};


struct GraphicState {
    SDL_GPUGraphicsPipeline* graphicPipeline[NumGraphicPipelines];
    SDL_GPUComputePipeline* computePipeline[NumComputePipelines];
    SDL_GPUBuffer* buffers[NumBuffers];
    SDL_GPUTexture* textures[NumTextures];
    SDL_GPUSampler* samplersPreset[NumSamplers];
    DisplayMode displayMode;
    ViewType viewType;
    RasterMode rasterMode;
    std::uint32_t numSphereIndices;
    std::uint32_t numBoxIndices;

    ParticleUpdateUniform particleUniformBuffer;
    GeometryBufferUniform geometryBufferUniform;
    GeometryBufferParticlesUniform geometryBufferParticlesUniform;
    BilateralBlurBufferUniform bilateralBlurBufferUniform;

    std::vector<Box> boxes;
    std::vector<Particle> particles;
};
