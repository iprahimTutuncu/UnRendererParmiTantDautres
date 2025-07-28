#include "physics.h"

#include "mpm/mpm.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>
#include <UTL/profiler.hpp>

static int run_thread(void* data) {

    auto& state = *static_cast<PhysicState*>(data);

    UTL_PROFILER("Physics Thread")
    while (state.running.load(std::memory_order::acquire)) {
        MPM_AppResult result;
        UTL_PROFILER("mpm_iterate")
        result = mpm_iterate(state.mpm_data);

        if (result != MPM_APP_CONTINUE) [[unlikely]] {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MPM iteration failed.");
            state.running.store(false, std::memory_order::release);
            return 1; // Exit thread with error
        }
    }

    return 0;
}

SDL_AppResult physics_init(AppState& state, int argc, char** argv) {
    (void)argc; // Unused
    (void)argv; // Unused
    auto pstate = new PhysicState();

    MpmParams params {
        .deltaT = 0.0005f,
        .radius = 0.017f,

        .compression = 0.019f,
        .stretch = 0.0075f,
        .hardening = 15.0f,
        .young = 140000.f,
        .poisson = 0.2f,
        .alpha = 0.95f,
        .density = 250.f,
        .num_particles = 10000,
        .grid_size = 150,
        .gravity = { 0.0f, -9.81f, 0.0f },
    };

    MPM_AppResult mpm_result;
    UTL_PROFILER("mpm_init")
    mpm_result = mpm_init(&pstate->mpm_data, params);

    if (mpm_result != MPM_APP_CONTINUE) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize MPM: %d", mpm_result);
        delete pstate;
        return SDL_APP_FAILURE;
    }

    auto thread = SDL_CreateThread(run_thread, "Physics Thread", static_cast<void*>(pstate));
    if (!thread) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create physics thread: %s", SDL_GetError());

        mpm_quit(pstate->mpm_data);

        delete pstate;
        return SDL_APP_FAILURE;
    }

    state.physics = pstate;
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
