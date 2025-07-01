#include "controls.h"

#include "../camera.h"
#include "../vmath.h"

#include <SDL3/SDL_keyboard.h>

static inline vec3 rotate_vec3_by_quat(const vec3& v, const quat& q) {
    // q * v * q^-1
    quat vq { 0, v.x, v.y, v.z };
    quat q_inv { q.w, -q.x, -q.y, -q.z };
    quat result = q * vq * q_inv;
    return { result.x, result.y, result.z };
}

SDL_AppResult controls_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    state.controls = new ControlState {};
    ControlState& controls = *state.controls;
    controls.cameraTarget = { 0.f, 0.f, 0.f };
    controls.movement_speed = 2.5f;
    controls.mouse_sensitivity = 1.f;
    controls.distanceFromTarget = state.camera->position.length();
    controls.isCameraLocked = true;

    return SDL_APP_CONTINUE;
}

SDL_AppResult controls_iterate(AppState& state) {
    float velocity = state.controls->movement_speed * state.deltaTime;
    bool const* keystate = SDL_GetKeyboardState(nullptr);

    if (keystate[SDL_SCANCODE_LCTRL]) {
        velocity *= 10;
    }
    if (keystate[SDL_SCANCODE_A]) {
        state.camera->position -= velocity * state.deltaTime * mat3_cast(state.camera->rotation)[0];
    }
    if (keystate[SDL_SCANCODE_D]) {
        state.camera->position += velocity * state.deltaTime * mat3_cast(state.camera->rotation)[0];
    }
    if (keystate[SDL_SCANCODE_S]) {
        state.camera->position -= velocity * state.deltaTime * mat3_cast(state.camera->rotation)[2];
    }
    if (keystate[SDL_SCANCODE_W]) {
        state.camera->position += velocity * state.deltaTime * mat3_cast(state.camera->rotation)[2];
    }
    if (keystate[SDL_SCANCODE_SPACE]) {
        state.camera->position += velocity * state.deltaTime * mat3_cast(state.camera->rotation)[1];
    }
    if (keystate[SDL_SCANCODE_LSHIFT]) {
        state.camera->position -= velocity * state.deltaTime * mat3_cast(state.camera->rotation)[1];
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
    // Handle Right Mouse Button Click to toggle camera lock
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        SDL_MouseButtonEvent const& evt = (SDL_MouseButtonEvent&)event;
        if (evt.button == SDL_BUTTON_RIGHT) {
            state.controls->isCameraLocked = !state.controls->isCameraLocked;
        }
    } break;
    case SDL_EVENT_MOUSE_MOTION: {
        if (state.controls->isCameraLocked) break; // Ignore camera movement if locked

        SDL_MouseMotionEvent const& evt = (SDL_MouseMotionEvent&)event;
        float mouse_x, mouse_y;
        Uint32 mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        // Check if left mouse button is held
        if (mouse_buttons & SDL_BUTTON_LMASK) {

            vec3 right = state.camera->right();
            vec3 up = { 0, 1, 0 };

            state.controls->cameraTarget -= right * evt.xrel;
            state.controls->cameraTarget += up * evt.yrel;

            vec3 offset = rotate_vec3_by_quat(vec3 { 0, 0, state.controls->distanceFromTarget }, state.camera->rotation);
            state.camera->position = vec3 {
                state.controls->cameraTarget.x + offset.x,
                state.controls->cameraTarget.y + offset.y,
                state.controls->cameraTarget.z + offset.z
            };
        } else {
            // Orbit as before
            float angle_h = -radians(evt.xrel);
            float angle_v = -radians(evt.yrel);
            quat q_h = angleAxis(angle_h, vec3 { 0, 1, 0 });
            quat q_v = angleAxis(angle_v, state.camera->right());

            state.camera->rotation = q_h * q_v * state.camera->rotation;

            vec3 offset = rotate_vec3_by_quat(vec3 { 0, 0, state.controls->distanceFromTarget }, state.camera->rotation);
            state.camera->position = vec3 {
                state.controls->cameraTarget.x + offset.x,
                state.controls->cameraTarget.y + offset.y,
                state.controls->cameraTarget.z + offset.z
            };
        }
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
    delete state.controls;
}
