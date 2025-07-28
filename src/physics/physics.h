#pragma once

#include "../state.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_thread.h>

#include <atomic>

union SDL_Event;

SDL_AppResult physics_init(AppState& state, int argc, char** argv);
SDL_AppResult physics_iterate(AppState& state);
SDL_AppResult physics_event(AppState& state, SDL_Event& event);
void physics_quit(AppState& state);

struct PhysicState {
    SDL_Thread* thread;
    void* mpm_data; // an opaque pointer to the MPM data structure
    std::atomic_bool running;
};
