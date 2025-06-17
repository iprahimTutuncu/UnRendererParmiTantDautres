#include "api.h"

#include "../graphics/camera.h"
#include "../vmath.h"
#include "mouse_control.h"

SDL_AppResult controls_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    state.mouseControl = new MouseControl {};
    MouseControl& controls = *state.mouseControl;
    controls.mouvement_speed = 2.5f;

    return SDL_APP_CONTINUE;
}

SDL_AppResult controls_iterate(AppState& state) {
    (void)state;

    return SDL_APP_CONTINUE;
}

SDL_AppResult controls_event(AppState& state, SDL_Event& event) {
    switch (event.type) {
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
    case SDL_EVENT_MOUSE_MOTION: {
        SDL_MouseMotionEvent const& evt = (SDL_MouseMotionEvent&)event;

        float angle_h = -radians(evt.xrel);
        float angle_v = -radians(evt.yrel);
        quat q_h = angleAxis(angle_h, vec3 { 0, 1, 0 });
        quat q_v = angleAxis(angle_v, state.camera->right());

        state.camera->rotation = q_h * q_v * state.camera->rotation;
    } break;
    case SDL_EVENT_KEY_DOWN: {
        SDL_KeyboardEvent const& evt = (SDL_KeyboardEvent&)event;
        float velocity = state.mouseControl->mouvement_speed * state.delta_time;
        switch (evt.key) {
        case SDLK_LCTRL:
            velocity *= 10;
            break;
        case SDLK_W: {

        } break;
        case SDLK_S:
            break;
        case SDLK_A: {
            quat const& q = state.camera->rotation;
            float qyy(q.y * q.y);
            float qzz(q.z * q.z);
            float qxz(q.x * q.z);
            float qxy(q.x * q.y);
            float qwy(q.w * q.y);
            float qwz(q.w * q.z);

            // mat3_cast(q) * vec3(1,0,0)
            state.camera->position -= velocity * state.delta_time * vec3 { 1 - 2 * (qyy + qzz), 2 * (qxy + qwz), 2 * (qxz - qwy) };
        } break;
        case SDLK_D:
            break;
        case SDLK_LSHIFT:
            break;
        case SDLK_SPACE:
            break;
        case SDLK_PAGEUP:
            break;
        case SDLK_PAGEDOWN:
            break;
        default:
            break;
        }

    } break;

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

void controls_quit(AppState& state) {
    if (state.mouseControl)
        delete state.mouseControl;
}
