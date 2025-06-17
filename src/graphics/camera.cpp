#include "camera.h"

#include <cmath>

mat4 CameraPerspective::view_matrix() const {
    mat4 m;
    quat const& q = rotation;
    vec3 p = -position;

    float qxx(q.x * q.x);
    float qyy(q.y * q.y);
    float qzz(q.z * q.z);
    float qxz(q.x * q.z);
    float qxy(q.x * q.y);
    float qyz(q.y * q.z);
    float qwx(q.w * q.x);
    float qwy(q.w * q.y);
    float qwz(q.w * q.z);

    m[0] = { 1 - 2 * (qyy + qzz), 2 * (qxy - qwz), 2 * (qxz + qwy), 0 };
    m[1] = { 2 * (qxy + qwz), 1 - 2 * (qxx + qzz), 2 * (qyz - qwx), 0 };
    m[2] = { 2 * (qxz - qwy), 2 * (qyz + qwx), 1 - 2 * (qxx + qyy), 0 };
    m[3] = m[0] * p.x + m[1] * p.y + m[2] * p.z;
    m[3][3] = 1;

    return m;
}

mat4 CameraPerspective::projection_matrix() const {
    assert(std::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.f);
    assert(far - near != 0.f);
    const float tanHalfFovy = std::tanf(fov / 2.f);
    mat4 m = {};
    m[0][0] = 1 / (aspect * tanHalfFovy);
    m[1][1] = 1 / tanHalfFovy;
    m[2][2] = -(far + near) / (far - near);
    m[2][3] = -1;
    m[3][4] = -(2 * far * near) / (far - near);
    return m;
}
