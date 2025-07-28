#pragma once

#include <vmath/vmath.h>

struct CameraPerspective {
    vmath::vec3 front;
    vmath::vec3 right;
    vmath::vec3 up;
    vmath::vec3 position;
    float aspectRatio;
    float fov;
    float near;
    float far;

    vmath::mat4 view_matrix() const;
    vmath::mat4 projection_matrix() const;
};
