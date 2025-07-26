#include "controls.h"

#include "../camera.h"
#include "../vmath.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

static void set_mouse_capture(AppState& state, bool capture) {
    state.controls->isCameraCaptured = capture;
    SDL_SetWindowMouseGrab(state.window, capture);
    SDL_SetWindowRelativeMouseMode(state.window, capture);
}

SDL_AppResult controls_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    state.controls = new ControlState {};
    ControlState& controls = *state.controls;
    controls.isCameraCaptured = false;
    controls.movement_speed = 2.5f;
    controls.mouse_sensitivity = 0.1f;
    controls.distanceFromTarget = state.camera->position.length();
    controls.yaw = -180.f;
    controls.pitch = -60.f;

    return SDL_APP_CONTINUE;
}

SDL_AppResult controls_iterate(AppState& state) {
    bool const* keystate = SDL_GetKeyboardState(nullptr);

    float velocity = state.controls->movement_speed * state.deltaTime;
    if (keystate[SDL_SCANCODE_LCTRL]) {
        velocity *= 10;
    }
    if (keystate[SDL_SCANCODE_A]) {
        state.camera->position -= velocity * state.camera->right;
    }
    if (keystate[SDL_SCANCODE_D]) {
        state.camera->position += velocity * state.camera->right;
    }
    if (keystate[SDL_SCANCODE_S]) {
        state.camera->position -= velocity * state.camera->front;
    }
    if (keystate[SDL_SCANCODE_W]) {
        state.camera->position += velocity * state.camera->front;
    }
    if (keystate[SDL_SCANCODE_SPACE]) {
        state.camera->position += velocity * state.camera->up;
    }
    if (keystate[SDL_SCANCODE_LSHIFT]) {
        state.camera->position -= velocity * state.camera->up;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult controls_event(AppState& state, SDL_Event const& event) {
    switch (event.type) {

    case SDL_EVENT_KEY_DOWN: {
        SDL_KeyboardEvent& evt = (SDL_KeyboardEvent&)event;
        switch (evt.key) {
        case SDLK_ESCAPE:
            return SDL_APP_SUCCESS;
        case SDLK_E:
            set_mouse_capture(state, !state.controls->isCameraCaptured);
            return SDL_APP_CONTINUE;

        default:
            return SDL_APP_CONTINUE;
        }

    } break;
    // Handle Right Mouse Button Click to toggle camera lock
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        SDL_MouseButtonEvent& evt = (SDL_MouseButtonEvent&)event;
        if (evt.button == SDL_BUTTON_RIGHT) {
            set_mouse_capture(state, !state.controls->isCameraCaptured);
            return SDL_APP_CONTINUE;
        }
    } break;
    case SDL_EVENT_MOUSE_MOTION: {
        if (!state.controls->isCameraCaptured) {
            return SDL_APP_CONTINUE;
        }

        // if (state.controls->isCameraLocked) break; // Ignore camera movement if locked
        SDL_MouseMotionEvent const& evt = (SDL_MouseMotionEvent&)event;

        float& yaw = state.controls->yaw;
        float& pitch = state.controls->pitch;

        yaw += evt.xrel * state.controls->mouse_sensitivity;
        pitch -= evt.yrel * state.controls->mouse_sensitivity;

        if (pitch > 89.f) {
            pitch = 89.f;
        } else if (pitch < -89.f) {
            pitch = -89.f;
        }

        if (yaw > 180.f){
            yaw -= 360.f;
        } else if (yaw < -180.f) {
            yaw += 360.f;
        }

        vec3& front = state.camera->front;
        vec3& right = state.camera->right;
        vec3& up = state.camera->up;

        front.x = std::cos(radians(yaw)) * std::cos(radians(pitch));
        front.y = std::sin(radians(pitch));
        front.z = std::sin(radians(yaw)) * std::cos(radians(pitch));

        front = normalize(front);
        right = normalize(cross(front, vec3 { 0.f, 1.f, 0.f }));
        up = normalize(cross(right, front));

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
