#pragma once
#include "state.h"
#include "vmath.h"

struct CameraPerspective {
    quat rotation; // quaternion
    vec3 position; // vec3
    vec3 target = { 0.0f, 0.0f, 0.0f }; // Center where camera is looking at
    float aspectRatio;
    float distance = 100.0f; // Distance from camera to target
    float fov;
    float near;
    float far;

    inline constexpr vec3 right() const {
        return vec3 {
            1 - 2 * (rotation.y * rotation.y + rotation.z * rotation.z),
            2 * (rotation.x * rotation.y + rotation.w * rotation.z),
            2 * (rotation.x * rotation.z - rotation.w * rotation.y),
        };
    }

    mat4 view_matrix() const;
    mat4 projection_matrix() const;
};
