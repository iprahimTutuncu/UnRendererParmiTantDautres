#pragma once
#include "../state.h"
#include "../vmath.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

struct CameraPerspective {
    quat rotation; // quaternion
    vec3 position; // vec3
    float aspect;
    float fov;
    float near;
    float far;

    constexpr inline vec3 right() const {
        return vec3 {
            1 - 2 * (rotation.y * rotation.y + rotation.z * rotation.z),
            2 * (rotation.x * rotation.y + rotation.w * rotation.z),
            2 * (rotation.x * rotation.z - rotation.w * rotation.y),
        };
    }

    mat4 view_matrix() const;
    mat4 projection_matrix() const;
};
