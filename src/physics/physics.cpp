#include "physics.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>
#include <UTL/profiler.hpp>

static int run_thread(void* data) {
    // This function is called by the SDL thread system
    // It will run the physics simulation in a separate thread

    auto& state = *static_cast<PhysicState*>(data);

    size_t i = 0;
    UTL_PROFILER("Physics Thread")
    while (state.running.load(std::memory_order::acquire)) {

        SDL_Log("Physics thread is running: %zu", i++);
    }

    return 0;
}

SDL_AppResult physics_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    auto pstate = new PhysicState();
    state.physics = pstate;

    auto thread = SDL_CreateThread(run_thread, "Physics Thread", static_cast<void*>(pstate));
    if (!thread) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create physics thread: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    pstate->thread = thread;
    pstate->running = true;

    return SDL_APP_CONTINUE;
}

SDL_AppResult physics_iterate(AppState& state) {
    (void)state;

    return SDL_APP_CONTINUE;
}

SDL_AppResult physics_event(AppState& state, SDL_Event& event) {
    (void)state;
    (void)event;

    return SDL_APP_CONTINUE;
}

void physics_quit(AppState& state) {

    PhysicState& physic_state = *state.physics;

    physic_state.running.store(false, std::memory_order::release);
    if (physic_state.thread) {
        SDL_WaitThread(physic_state.thread, nullptr);
        physic_state.thread = nullptr;
    }

    delete state.physics;
    state.physics = nullptr;
}
