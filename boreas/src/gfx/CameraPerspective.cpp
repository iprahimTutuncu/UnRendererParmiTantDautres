#include "CameraPerspective.hpp"

#include <glad/glad.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

CameraPerspective::CameraPerspective(int width, int height, glm::vec3 position, glm::vec3 at)
    : CameraPerspective(width, height, position, at, DEFAULT_YAW, DEFAULT_PITCH) { }

CameraPerspective::CameraPerspective(int width, int height, glm::vec3 position, glm::vec3 at, float yaw, float pitch)
    : _aspect_ratio { (float)width / (float)height }
    , position { position }
    , yaw { yaw }
    , pitch { pitch } {
    update();
}

const float CameraPerspective::get_aspect_ratio() const {
    return _aspect_ratio;
}

void CameraPerspective::set_aspect_ratio(int width, int height) {
    _aspect_ratio = (float)width / (float)height;
}

glm::mat4 CameraPerspective::get_view_matrix() {
    // eye, target, up
    return glm::lookAt(position, position + front, up);
}

glm::mat4 CameraPerspective::get_proj_matrix() {
    return glm::perspective(glm::radians(zoom), _aspect_ratio, _near, _far);
}

glm::vec3 CameraPerspective::get_position() {
    return position;
}

void CameraPerspective::process_input(double delta_time) {
    float velocity = movement_speed * delta_time;
    const Uint8* keystate = SDL_GetKeyboardState(NULL);

    if (keystate[SDL_SCANCODE_LCTRL]) {
        velocity *= 10;
    }
    if (keystate[SDL_SCANCODE_W]) {
        position += front * velocity;
    }
    if (keystate[SDL_SCANCODE_S]) {
        position -= front * velocity;
    }
    if (keystate[SDL_SCANCODE_A]) {
        position -= right * velocity;
    }
    if (keystate[SDL_SCANCODE_D]) {
        position += right * velocity;
    }
    if (keystate[SDL_SCANCODE_LSHIFT]) {
        position -= world_up * velocity;
    }
    if (keystate[SDL_SCANCODE_SPACE]) {
        position += world_up * velocity;
    }
    if (keystate[SDL_SCANCODE_PAGEUP]) {
        zoom -= 25.0f * velocity;
        if (zoom < DEFAULT_ZOOM_MIN)
            zoom = DEFAULT_ZOOM_MIN;
    }
    if (keystate[SDL_SCANCODE_PAGEDOWN]) {
        zoom += 25.0f * velocity;
        if (zoom > DEFAULT_ZOOM_MAX)
            zoom = DEFAULT_ZOOM_MAX;
    }
}

void CameraPerspective::process_mouse(float x, float y) {
    if (_first_mouse) {
        SDL_CaptureMouse(SDL_TRUE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
        _first_mouse = false;
        return;
    }

    const float sensitivity = 0.1f;
    yaw += x * sensitivity;
    pitch += -(y * sensitivity);

    if (pitch > 89.f) {
        pitch = 89.f;
    }
    if (pitch < -89.f) {
        pitch = -89.f;
    }

    update();
}

void CameraPerspective::process_wheel(float y) {
    zoom -= (float)y * 1.25;

    if (zoom < DEFAULT_ZOOM_MIN)
        zoom = DEFAULT_ZOOM_MIN;
    if (zoom > DEFAULT_ZOOM_MAX)
        zoom = DEFAULT_ZOOM_MAX;
}

void CameraPerspective::update() {
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    front = glm::normalize(front);
    right = glm::normalize(glm::cross(front, world_up));
    up = glm::normalize(glm::cross(right, front));
}
