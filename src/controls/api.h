#pragma once

#include "../state.h"

#include <SDL3/SDL_init.h>

union SDL_Event;

SDL_AppResult controls_init(AppState& state, int argc, char** argv);
SDL_AppResult controls_iterate(AppState& state);
SDL_AppResult controls_event(AppState& state, SDL_Event& event);
void controls_quit(AppState& state);

// NOTE: DUMMY IMPLEMENTATION, DELETE ONCE IMPLEMENTED IN A REAL FILE

inline SDL_AppResult controls_init(AppState& state, int argc, char** argv) {
    (void)state;
    (void)argc;
    (void)argv;

    return SDL_APP_CONTINUE;
}

inline SDL_AppResult controls_iterate(AppState& state) {
    (void)state;

    return SDL_APP_CONTINUE;
}

inline SDL_AppResult controls_event(AppState& state, SDL_Event& event) {
    (void)state;
    (void)event;

    return SDL_APP_CONTINUE;
}

inline void controls_quit(AppState& state) {
    (void)state;
}
