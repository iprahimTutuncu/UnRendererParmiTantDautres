#include "camera.h"

#include <cassert>
#include <cmath>
#include <limits>

mat4 CameraPerspective::view_matrix() const {
    mat4 m;
    quat const& q = rotation;
    vec3 p = -position;

    m = mat3_cast(q);
    m.mm[3] = m.mm[0] * p.x + m.mm[1] * p.y + m.mm[2] * p.z;
    m[3].w = 1;
    return m;
}

mat4 CameraPerspective::projection_matrix() const {
    assert(std::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.f);
    assert(far - near != 0.f);
    const float tanHalfFovy = std::tan(fov / 2.f);
    mat4 m = {};

    m[0].x = 1 / (aspect * tanHalfFovy);
    m[1].y = 1 / tanHalfFovy;
    m[2].z = -(far + near) / (far - near);
    m[2].w = -1;
    m[3].z = -(2 * far * near) / (far - near);

    return m;
}
