#pragma once

#include <cassert>
#include <cstddef>

#include <x86intrin.h>

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
    vec3 cols[3];
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
        __m128 a, b;
        a = _mm_load_ps(&v.x);
        b = _mm_load_ps(&cols[0].x);

        auto x = _mm_dp_ps(a, b, 0x71);
        b = _mm_load_ps(&cols[1].x);
        x = _mm_dp_ps(a, b, 0x72);
        b = _mm_load_ps(&cols[2].z);
        x = _mm_dp_ps(a, b, 0x74);

        return {
            x[0],
            x[1],
            x[2],
        };
#endif
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
    __m128 a = _mm_set1_ps(angle / 2);
    __m128 s, c;
    sincos(a, &s, &c);
    __m128 r = _mm_mul_ps(s, _mm_load_ps(&v.x));
    return {
        c[0],
        r[0],
        r[1],
        r[2],
    };
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
    return {
        {
            { 1 - 2 * (qyy + qzz), 2 * (qxy - qwz), 2 * (qxz + qwy) },
            { 2 * (qxy + qwz), 1 - 2 * (qxx + qzz), 2 * (qyz - qwx) },
            { 2 * (qxz - qwy), 2 * (qyz + qwx), 1 - 2 * (qxx + qyy) },
        }
    };
}
