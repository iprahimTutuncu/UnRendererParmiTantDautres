#pragma once

#include "../state.h"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>


// Initialization & cleanup
SDL_AppResult deferred_ssao_init(AppState& state);
SDL_AppResult deferred_ssao_create_pipeline(AppState& state); // <-- Add this line

// Rendering
void deferred_ssao_render(AppState& state, SDL_GPUCommandBuffer* cmdBuf);
