#include "controls.h"

#include "../graphics/camera.h"
#include "../vmath.h"

#include <SDL3/SDL_keyboard.h>

SDL_AppResult controls_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    state.controls = new ControlState {};
    ControlState& controls = *state.controls;
    controls.mouse.movement_speed = 2.5f;
    controls.mouse.mouse_sensitivity = 1.f;

    return SDL_APP_CONTINUE;
}

SDL_AppResult controls_iterate(AppState& state) {
    float velocity = state.controls->mouse.movement_speed * state.delta_time;
    bool const* keystate = SDL_GetKeyboardState(nullptr);

    if (keystate[SDL_SCANCODE_LCTRL]) {
        velocity *= 10;
    }
    if (keystate[SDL_SCANCODE_A]) {
        state.camera->position -= velocity * state.delta_time * mat3_cast(state.camera->rotation)[0];
    }
    if (keystate[SDL_SCANCODE_D]) {
        state.camera->position += velocity * state.delta_time * mat3_cast(state.camera->rotation)[0];
    }
    if (keystate[SDL_SCANCODE_S]) {
        state.camera->position -= velocity * state.delta_time * mat3_cast(state.camera->rotation)[2];
    }
    if (keystate[SDL_SCANCODE_W]) {
        state.camera->position += velocity * state.delta_time * mat3_cast(state.camera->rotation)[2];
    }
    if (keystate[SDL_SCANCODE_SPACE]) {
        state.camera->position += velocity * state.delta_time * mat3_cast(state.camera->rotation)[1];
    }
    if (keystate[SDL_SCANCODE_LSHIFT]) {
        state.camera->position -= velocity * state.delta_time * mat3_cast(state.camera->rotation)[1];
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult controls_event(AppState& state, SDL_Event const& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
        SDL_KeyboardEvent& evt = (SDL_KeyboardEvent&)event;
        if (evt.key == SDLK_ESCAPE) [[unlikely]]
            return SDL_APP_SUCCESS;
    } break;
    case SDL_EVENT_MOUSE_MOTION: {
        SDL_MouseMotionEvent const& evt = (SDL_MouseMotionEvent&)event;

        float angle_h = -radians(evt.xrel);
        float angle_v = -radians(evt.yrel);
        quat q_h = angleAxis(angle_h, vec3 { 0, 1, 0 });
        quat q_v = angleAxis(angle_v, state.camera->right());

        state.camera->rotation = q_h * q_v * state.camera->rotation;
    } break;
    case SDL_EVENT_MOUSE_WHEEL: {
        SDL_MouseWheelEvent const& evt = (SDL_MouseWheelEvent&)event;

        float& fov = state.camera->fov;
        fov -= radians(evt.y * 1.25f);

        if (fov < radians(1.0f)) {
            fov = radians(1.0f);
        } else if (fov > radians(120.f)) {
            fov = radians(120.f);
        }
    } break;

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

void controls_quit(AppState& state) {
    if (state.controls)
        delete state.controls;
}
