#pragma once

#include "vmath.h"

struct CameraPerspective {
    vec3 front;
    vec3 right;
    vec3 up;
    vec3 position;
    float aspectRatio;
    float fov;
    float near;
    float far;

    mat4 view_matrix() const;
    mat4 projection_matrix() const;
};
