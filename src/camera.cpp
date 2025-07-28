#include "camera.h"

#include <cassert>
#include <cmath>

static inline vmath::mat4 lookAt(vmath::vec3 const& eye, vmath::vec3 const& center, vmath::vec3 const& up) {
    vmath::vec3 const f(normalize(center - eye));
    vmath::vec3 const s(normalize(cross(f, up)));
    vmath::vec3 const u(cross(s, f));

    vmath::mat4 m = vmath::mat4::identity();
    m[0].x = s.x;
    m[1].x = s.y;
    m[2].x = s.z;
    m[0].y = u.x;
    m[1].y = u.y;
    m[2].y = u.z;
    m[0].z = -f.x;
    m[1].z = -f.y;
    m[2].z = -f.z;
    m[3].x = -dot(s, eye);
    m[3].y = -dot(u, eye);
    m[3].z = dot(f, eye);

    return m;
}

vmath::mat4 CameraPerspective::view_matrix() const {
    return lookAt(position, position + front, up);
}

vmath::mat4 CameraPerspective::projection_matrix() const {
    assert(aspectRatio != 0.f);
    assert(far - near != 0.f);
    const float tanHalfFovy = std::tan(fov / 2.f);
    vmath::mat4 m = {};

    m[0].x = 1 / (aspectRatio * tanHalfFovy);
    m[1].y = -1 / tanHalfFovy;
    m[2].z = -(far + near) / (far - near);
    m[2].w = -1;
    m[3].z = -(2 * far * near) / (far - near);

    return m;
}
