#pragma once

#include "../state.h"
#include "../vmath.h"

#include <SDL3/SDL_init.h>
#include <vector>

union SDL_Event;

SDL_AppResult graphics_init(AppState& state, int argc, char** argv);
SDL_AppResult graphics_iterate(AppState& state);
SDL_AppResult graphics_event(AppState& state, SDL_Event& event);
SDL_AppResult graphics_create_render_targets(AppState& state);
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
    MpmInit,
    MpmParticleToGrid,
    MpmUpdateGrid,
    MpmGridToParticle,
    MpmResetGrid,
    NumComputePipelines // must be last
};

enum BufferIndex {
    BoxVertexBuffer,
    BoxIndexBuffer,
    ParticlesBuffer,
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
    alignas(16) float min[3];
    alignas(16) float max[3];
};

struct Rect {
    float centerX;
    float centerY;
    float halfWidth;
    float halfHeight;
};

struct Vertex {
    alignas(16) float position[3];
    alignas(16) float normal[3];
    alignas(16) float texCoord[2];
};

struct Particle {
    vec3 position;
    float mass;
    vec3 velocity;
    float volume_0;
    alignas(16) mat3 deform_elastic;
    alignas(16) mat3 deform_plastic;
    alignas(16) mat3 deform_affine;
};

struct GridNode {
    vec3 force;
    float mass;
    alignas(16) vec3 momentum;
    alignas(16) vec3 velocity_star;
    alignas(16) vec3 velocity;
};

struct SDL_GPUGraphicsPipeline;
struct SDL_GPUComputePipeline;
struct SDL_GPUBuffer;
struct SDL_GPUTexture;
struct SDL_GPUSampler;

// c'est le uniform buffer pour le compute

struct ParticleUpdateUniform {
    vec3 u_grid_origin;
    float u_grid_spacing; // h
    vec3 u_grid_dimension;
    float u_particles_per_cell;
    float u_initial_density;
    float u_mu_0;
    float u_lambda_0;
    float u_hardening_coefficient;
    float u_critical_compression;
    float u_critical_stretch;
    float u_poisson_ratio;
    float u_alpha_blend;

    vec3 u_gravity;
    float u_co_floor_y;
    vec3 u_co_normal;
    float u_co_mu;

    float u_D_inv; // 3.0 / h * h
    float time;
};

struct GeometryBufferUniform {
    mat4 proj;
    mat4 view;
    mat4 model;
};

struct GraphicState {
    SDL_GPUGraphicsPipeline* graphicPipeline[NumGraphicPipelines];
    SDL_GPUComputePipeline* computePipeline[NumComputePipelines];
    SDL_GPUBuffer* buffers[NumBuffers];
    SDL_GPUTexture* textures[NumTextures];
    SDL_GPUSampler* samplersPreset[NumSamplers];
    DisplayMode displayMode;
    RasterMode rasterMode;
    std::uint32_t numSphereIndices;
    std::uint32_t numBoxIndices;

    ParticleUpdateUniform particleUniformBuffer;
    GeometryBufferUniform geometryBufferUniform;

    std::vector<Box> boxes;
};
