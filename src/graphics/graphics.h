#pragma once

#include "../state.h"

#include <SDL3/SDL_init.h>

union SDL_Event;

SDL_AppResult graphics_init(AppState& state, int argc, char** argv);
SDL_AppResult graphics_iterate(AppState& state);
SDL_AppResult graphics_event(AppState& state, SDL_Event& event);
void graphics_quit(AppState& state);

enum PipelineIndex {
    Pipeline,
    NumPipelines, // must be last
};

enum BufferIndex {
    VertexBuffer,
    NumBuffers, // must be last
};

enum TextureIndex {
    SceneColorTexture,
    SceneDepthTexture,
    NumTextures, // must be last
};

struct SDL_GPUGraphicsPipeline;
struct SDL_GPUBuffer;
struct SDL_GPUTexture;
struct GraphicState {
    SDL_GPUGraphicsPipeline* pipeline[NumPipelines];
    SDL_GPUBuffer* buffers[NumBuffers];
    SDL_GPUTexture* textures[NumTextures];
};
