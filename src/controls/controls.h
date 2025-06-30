#pragma once

#include "../state.h"
#include "../vmath.h"

#include <SDL3/SDL_init.h>

union SDL_Event;

SDL_AppResult controls_init(AppState& state, int argc, char** argv);
SDL_AppResult controls_iterate(AppState& state);
SDL_AppResult controls_event(AppState& state, SDL_Event const& event);
void controls_quit(AppState& state);

struct MouseCtrl {
    vec3 target; // Center where camera is looking at
    float movement_speed;
    float mouse_sensitivity;
    float distanceFromTarget; // Distance from camera to targe
    bool locked; // Lock camera movement when true
};

struct KeyboardCtrl {
};

struct ControlState {
    MouseCtrl mouse;
    KeyboardCtrl kbCtrl;
};
