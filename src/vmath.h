#pragma once

#include <cassert>
#include <cstddef>

#include "intrin.hpp"

struct alignas(16) quat {
    float w;
    float x;
    float y;
    float z;

    constexpr inline quat operator*(quat const& q) const {
        quat const& p = *this;
        return {
            p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z,
            p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y,
            p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z,
            p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x,
        };
    }

    constexpr inline quat& operator*=(quat const& q) {
        quat& p = *this;
        this->w = p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z;
        this->x = p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y;
        this->y = p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z;
        this->z = p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x;

        return *this;
    }
};

struct alignas(16) vec3 {
    float x;
    float y;
    float z;

    constexpr inline vec3 operator-() const {
        return { -x, -y, -z };
    }

    constexpr inline vec3& operator=(vec3 const& v) {
        this->x = v.x;
        this->y = v.y;
        this->z = v.z;
        return *this;
    }

    constexpr inline vec3& operator+=(vec3 const& v) {
        this->x += v.x;
        this->y += v.y;
        this->z += v.z;
        return *this;
    }

    constexpr inline vec3& operator-=(vec3 const& v) {
        this->x -= v.x;
        this->y -= v.y;
        this->z -= v.z;
        return *this;
    }
};

constexpr inline vec3 operator*(float const& f, vec3 const& v) {
    return { v.x * f, v.y * f, v.z * f };
}

constexpr inline vec3 operator*(vec3 const& v, float const& f) {
    return { v.x * f, v.y * f, v.z * f };
}

constexpr inline vec3 operator*(int const& i, vec3 const& v) {
    return { i * v.x, i * v.y, i * v.z };
}

struct mat3 {
    union {
        __m128 mm[3];
        vec3 cols[3];
    };
    constexpr inline vec3& operator[](std::size_t index) {
        assert(index < 3);
        return cols[index];
    }

    const inline vec3& operator[](std::size_t index) const {
        assert(index < 3);
        return cols[index];
    }

    constexpr inline vec3 operator*(vec3 const& v) const {
#ifndef __SSE4_1__
        return {
            cols[0].x * v.x + cols[0].y * v.y + cols[0].z * v.z,
            cols[1].x * v.x + cols[1].y * v.y + cols[1].z * v.z,
            cols[2].x * v.x + cols[2].y * v.y + cols[2].z * v.z,
        };
#else
        __m128 r0 = _mm_load_ps(&v.x);
        auto r1 = _mm_dp_ps(r0, mm[0], 0x71);
        auto r2 = _mm_dp_ps(r0, mm[1], 0x72);
        auto r3 = _mm_dp_ps(r0, mm[2], 0x74);
#ifndef _MSC_VER
        return { r1[0], r2[1], r3[2] };
#else
        return { r1.m128_f32[0], r2.m128_f32[1], r3.m128_f32[2] };
#endif
#endif // __SSE4_1__
    }
};

struct alignas(16) vec4 {
    // clang-format off
    union { float x, r, s; };
    union { float y, g, t; };
    union { float z, b, p; };
    union { float w, a, q; };
    // clang-format on

    constexpr inline float& operator[](std::size_t index) {
        assert(index < 4);
        switch (index) {
        default:
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        case 3:
            return w;
        }
    }

    constexpr const inline float& operator[](std::size_t index) const {
        assert(index < 4);
        switch (index) {
        default:
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        case 3:
            return w;
        }
    }
};

struct mat4 {
    __m128 cols[4];

    constexpr inline __m128& operator[](std::size_t index) {
        assert(index < 4);
        return cols[index];
    }

    const inline __m128& operator[](std::size_t index) const {
        assert(index < 4);
        return cols[index];
    }
};

template <typename T>
constexpr T radians(T degrees) {
    return degrees * static_cast<T>(0.01745329251994329576923690768489);
}

constexpr inline void sincos(__m128 x, __m128* sin_out, __m128* cos_out) {
    const __m128 c3 = _mm_set1_ps(-1.0f / 6.0f);
    const __m128 c2 = _mm_set1_ps(-1.0f / 2.0f);
    const __m128 c4 = _mm_set1_ps(1.0f / 24.0f);
    const __m128 c5 = _mm_set1_ps(1.0f / 120.0f);
    const __m128 c6 = _mm_set1_ps(-1.0f / 720.0f);
    const __m128 c7 = _mm_set1_ps(-1.0f / 5040.0f);

    __m128 x2 = x * x;
    *sin_out = x + (x * x2 * (c3 + x2 * (c5 + x2 * c7)));
    *cos_out = 1.0f + x2 * (c2 + x2 * (c4 + x2 * c6));
}

constexpr inline quat angleAxis(float const& angle, vec3 const& v) {
    __m128 r0 = _mm_set1_ps(angle / 2);
    __m128 r1_sin, r2_cos, r3;
    sincos(r0, &r1_sin, &r2_cos);
    r3 = _mm_mul_ps(r1_sin, _mm_load_ps(&v.x));

#ifdef _MSV_VER
    return { c.m128_f32[0], r.m128_f32[0], r.m128_f32[1], r.m128_f32[2] };
#else
    return { r2_cos[0], r3[0], r3[1], r3[2] };
#endif
}

constexpr inline mat3 mat3_cast(quat const& q) {
    float qxx(q.x * q.x);
    float qyy(q.y * q.y);
    float qzz(q.z * q.z);
    float qxz(q.x * q.z);
    float qxy(q.x * q.y);
    float qyz(q.y * q.z);
    float qwx(q.w * q.x);
    float qwy(q.w * q.y);
    float qwz(q.w * q.z);

    __m128 r0 = _mm_set_ps(qyy + qzz, qxy - qwz, qxz + qwy, 0.f);
    __m128 r1 = _mm_set_ps(qxy + qwz, qxx + qzz, qyz - qwx, 0.f);
    __m128 r2 = _mm_set_ps(qxz - qwy, qyz + qwx, qxx + qyy, 0.f);

    r0 = r0 + r0;
    r1 = r1 + r1;
    r2 = r2 + r2;

    return {
        .cols {
            { 1 - r0[0], r0[1], r0[2] },
            { r1[0], 1 - r1[1], r1[2] },
            { r2[0], r2[1], 1 - r2[2] },
        }
    };
}
