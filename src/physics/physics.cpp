#include "physics.h"

SDL_AppResult physics_init(AppState& state, int argc, char** argv) {
    (void)state;
    (void)argc;
    (void)argv;

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
    (void)state;
}
