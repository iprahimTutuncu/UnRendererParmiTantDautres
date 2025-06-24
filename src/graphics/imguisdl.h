#pragma once

#include "../state.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>


union SDL_Event;

SDL_AppResult imgui_init(AppState& state, int argc, char** argv);
void imgui_iterate(AppState& state, SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmdbuf);
SDL_AppResult imgui_event(AppState& state, SDL_Event& event);
void imgui_quit(AppState& state);