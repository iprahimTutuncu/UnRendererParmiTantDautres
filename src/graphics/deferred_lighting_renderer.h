#pragma once

#include "../state.h"
#include "graphics.h"

#include <SDL3/SDL_gpu.h>

SDL_AppResult deferred_lighting_init(AppState& state);
void deferred_lighting_render_to_texture(
    AppState& state,
    SDL_GPURenderPass* renderPass,
    SDL_GPUCommandBuffer* cmdBuf,
    DisplayMode mode);
