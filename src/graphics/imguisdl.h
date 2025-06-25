#pragma once

#include "../state.h"

union SDL_Event;
struct SDL_GPURenderPass;
struct SDL_GPUCommandBuffer;

void imgui_init(AppState& state);
void imgui_iterate(AppState& state, SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmdbuf);
void imgui_event(AppState& state, SDL_Event& event);
void imgui_quit(AppState& state);
