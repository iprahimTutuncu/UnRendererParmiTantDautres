#include "camera.h"

#include <cassert>
#include <cmath>

mat4 CameraPerspective::view_matrix() const {
    return lookat(position, position + front, up);
}

mat4 CameraPerspective::projection_matrix() const {
    assert(std::abs(aspectRatio - std::numeric_limits<float>::epsilon()) > 0.f);
    assert(far - near != 0.f);
    const float tanHalfFovy = std::tan(fov / 2.f);
    mat4 m = {};

    m[0].x = 1 / (aspectRatio * tanHalfFovy);
    m[1].y = 1 / tanHalfFovy;
    m[2].z = -(far + near) / (far - near);
    m[2].w = -1;
    m[3].z = -(2 * far * near) / (far - near);

    return m;
}
