#pragma once

#include "../state.h"

#include <SDL3/SDL_init.h>

union SDL_Event;

SDL_AppResult graphics_init(AppState& state, int argc, char** argv);
SDL_AppResult graphics_iterate(AppState& state);
SDL_AppResult graphics_event(AppState& state, SDL_Event& event);
void graphics_quit(AppState& state);
