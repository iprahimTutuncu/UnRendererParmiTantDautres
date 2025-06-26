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
    NumGraphicPipelines // must be last
};

enum ComputePipelineIndex {
    ParticleUpdate,
    NumComputePipelines // must be last
};

enum BufferIndex {
    BoxVertexBuffer,
    BoxIndexBuffer,
    ParticlePositionBuffer, // TODO: this has to be the struct for MPM and be renamed
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

enum RasterMode {
    RasterMode_Fill,
    RasterMode_Line
};

struct Box {
    float min[3];
    float max[3];
};

struct Rect {
    float centerX;
    float centerY;
    float halfWidth;
    float halfHeight;
};

struct Particle {
    float position[4];
    float color[4];
};

struct Vertex {
    float position[3];
    float _pad1;

    float normal[3];
    float _pad2;

    float texCoord[2];
    float _pad3[2];
};

struct SDL_GPUGraphicsPipeline;
struct SDL_GPUComputePipeline;
struct SDL_GPUBuffer;
struct SDL_GPUTexture;
struct SDL_GPUSampler;

// c'est le uniform buffer pour le compute

struct ParticleUpdateUniform
{
    float time;
};

struct GeometryBufferUniform {
    mat4 model;
    mat4 view;
    mat4 proj;
};

struct GraphicState
{
    SDL_GPUGraphicsPipeline* graphicPipeline[NumGraphicPipelines];
    SDL_GPUComputePipeline* computePipeline[NumComputePipelines];
    SDL_GPUBuffer* buffers[NumBuffers];
    SDL_GPUTexture* textures[NumTextures];
    SDL_GPUSampler* samplersPreset[NumSamplers];
    DisplayMode displayMode;
    RasterMode rasterMode;
    int numSphereIndices { 0 };
    int particleCount { 100 };

    ParticleUpdateUniform particleUniformBuffer {};
    GeometryBufferUniform geometryBufferUniform {};

    std::vector<Box> boxes;
    std::vector<Particle> particles;
};
