#pragma once

#include "../state.h"

#include <SDL3/SDL_init.h>

union SDL_Event;

SDL_AppResult controls_init(AppState& state, int argc, char** argv);
SDL_AppResult controls_iterate(AppState& state);
SDL_AppResult controls_event(AppState& state, SDL_Event const& event);
void controls_quit(AppState& state);

struct ControlState {
    bool isCameraCaptured;
    float movement_speed;
    float mouse_sensitivity;
    float distanceFromTarget;
    float yaw;
    float pitch;
};
