#pragma once

#include "../state.h"

union SDL_Event;

void imgui_init(AppState& state);
void imgui_iterate(AppState& state);
void imgui_event(AppState& state, SDL_Event& event);
void imgui_quit(AppState& state);
