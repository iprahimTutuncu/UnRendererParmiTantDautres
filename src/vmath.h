#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <utility>

#ifdef _MSC_VER
#include "x86intrin.hpp"
#else
#include <x86intrin.h>
#endif

struct alignas(16) quat {
    float w;
    float x;
    float y;
    float z;

    constexpr quat operator*(quat const& q) const {
        quat const& p = *this;
        return {
            p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z,
            p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y,
            p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z,
            p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x,
        };
    }

    constexpr quat& operator*=(quat const& q) {
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

    inline vec3 operator-() const {
        return { -x, -y, -z };
    }

    inline vec3& operator+=(vec3 const& v) {
        this->x += v.x;
        this->y += v.y;
        this->z += v.z;
        return *this;
    }

    inline vec3 operator*(vec3 const& v) {
        return { x * v.x, y * v.y, z * v.z };
    }

    inline vec3& operator-=(vec3 const& v) {
        this->x -= v.x;
        this->y -= v.y;
        this->z -= v.z;
        return *this;
    }
};

constexpr vec3 operator*(float f, vec3 const& v) {
    return { v.x * f, v.y * f, v.z * f };
}

constexpr vec3 operator*(vec3 const& v, float f) {
    return { v.x * f, v.y * f, v.z * f };
}

constexpr vec3 operator*(int i, vec3 const& v) {
    const float j = static_cast<float>(i);
    return { j * v.x, j * v.y, j * v.z };
}

struct mat3 {
    union {
        __m128 mm[3];
        vec3 cols[3];
    };

    inline vec3& operator[](std::size_t index) {
        assert(index < 3);
        return cols[index];
    }

    inline vec3 const& operator[](std::size_t index) const {
        assert(index < 3);
        return cols[index];
    }

    inline vec3 operator*(vec3 const& v) const {
#ifndef __SSE4_1__
        return {
            cols[0].x * v.x + cols[0].y * v.y + cols[0].z * v.z,
            cols[1].x * v.x + cols[1].y * v.y + cols[1].z * v.z,
            cols[2].x * v.x + cols[2].y * v.y + cols[2].z * v.z,
        };
#else
        __m128 r0 = _mm_load_ps(&v.x);
        __m128 r1 = _mm_dp_ps(r0, mm[0], 0x71);
        __m128 r2 = _mm_dp_ps(r0, mm[1], 0x72);
        __m128 r3 = _mm_dp_ps(r0, mm[2], 0x74);
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

    constexpr float& operator[](std::size_t index) {
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

    constexpr float const& operator[](std::size_t index) const {
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

    inline vec4 operator*(float f) {
        return { { x * f }, { y * f }, { z * f }, { w * f } };
    }

    inline vec4 operator+(float f) {
        return { { x + f }, { y + f }, { z + f }, { w + f } };
    }

    inline vec4 operator+(vec4 const& v) {
        return { { x + v.x }, { y + v.y }, { z + v.z }, { w + v.w } };
    }
};

struct mat4 {
    union {
        __m128 mm[4];
        vec4 v4[4];
    };

    constexpr vec4& operator[](std::size_t index) {
        assert(index < 4);
        return v4[index];
    }

    constexpr vec4 const& operator[](std::size_t index) const {
        assert(index < 4);
        return v4[index];
    }

    inline mat4& operator=(mat3 const& m) {
        std::memcpy(&v4[0].x, &m[0].x, sizeof(m));
        return *this;
    }

    static inline mat4 identity() {
        mat4 m = {};
        for (size_t i = 0; i < 4; i++) {
            m.v4[i][i] = 1.f;
        }
        return m;
    }
};

template <typename T>
static constexpr T radians(T degrees) {
    return degrees * static_cast<T>(0.01745329251994329576923690768489);
}

static inline void sincos(__m128 x, __m128* sin_out, __m128* cos_out) {
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

static inline quat angleAxis(float angle, vec3 const& v) {
    __m128 r0 = _mm_set1_ps(angle / 2);
    __m128 r1, sin, cos;
    sincos(r0, &sin, &cos);
    r1 = _mm_mul_ps(sin, _mm_load_ps(&v.x));

#ifdef _MSC_VER
    return { cos.m128_f32[0], r1.m128_f32[0], r1.m128_f32[1], r1.m128_f32[2] };
#else
    return { cos[0], r1[0], r1[1], r1[2] };
#endif
}

static inline mat3 mat3_cast(quat q) {
    mat3 m;
    float qxx(q.x * q.x);
    float qyy(q.y * q.y);
    float qzz(q.z * q.z);
    float qxz(q.x * q.z);
    float qxy(q.x * q.y);
    float qyz(q.y * q.z);
    float qwx(q.w * q.x);
    float qwy(q.w * q.y);
    float qwz(q.w * q.z);

    __m128 r0 = { qyy + qzz, qxy - qwz, qxz + qwy, 0.f };
    __m128 r1 = { qxy + qwz, qxx + qzz, qyz - qwx, 0.f };
    __m128 r2 = { qxz - qwy, qyz + qwx, qxx + qyy, 0.f };

    m.mm[0] = r0 + r0;
    m.mm[1] = r1 + r1;
    m.mm[2] = r2 + r2;
    m[0].x = 1 - m[0].x;
    m[1].y = 1 - m[1].y;
    m[2].z = 1 - m[2].z;
    return m;
}

static inline float dot(vec3 const& a, vec3 const& b) {
#ifndef __SSE4_1__
    return a.x * b.x + a.y * b.y + a.z * b.z;
#else
    __m128 r0 = _mm_load_ps(&a.x);
    __m128 r1 = _mm_load_ps(&b.x);
    return _mm_cvtss_f32(_mm_dp_ps(r0, r1, 0x71));
#endif
}

static inline float inversesqrt(float f) {
    return 1.f / std::sqrt(f);
}

static inline vec3 normalize(vec3 const& v) {
    __m128 r0 = _mm_load_ps(&v.x);
    __m128 r1 = _mm_dp_ps(r0, r0, 0x7F);
    __m128 r2 = _mm_rsqrt_ps(r1);

    r0 = _mm_mul_ps(r0, r2);

    vec3 r;
    _mm_store_ps(&r.x, r0);
    return r;
}

static inline vec4 normalize(vec4 const& v) {
    __m128 r0 = _mm_load_ps(&v.x);
    __m128 r1 = _mm_dp_ps(r0, r0, 0xFF);
    __m128 r2 = _mm_rsqrt_ps(r1);

    vec4 r;
    _mm_store_ps(&r.x, _mm_mul_ps(r0, r2));
    return r;
}

static inline __m128 normalize3(__m128 const& v) {
    __m128 r0 = _mm_dp_ps(v, v, 0x77);
    __m128 r2 = _mm_rsqrt_ps(r0);

    return _mm_mul_ps(r0, r2);
}

static inline __m128 normalize(__m128 const& v) {
    __m128 r0 = _mm_dp_ps(v, v, 0xFF);
    __m128 r2 = _mm_rsqrt_ps(r0);

    return _mm_mul_ps(r0, r2);
}

static inline vec3 cross(vec3 const& x, vec3 const& y) {
    __m128 vec0 = _mm_load_ps(&x.x);
    __m128 vec1 = _mm_load_ps(&y.x);
    __m128 tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 tmp2 = _mm_mul_ps(tmp0, vec1);
    __m128 tmp4 = _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
    vec3 v;
    _mm_store_ps(&v.x, _mm_fmsub_ps(tmp0, tmp1, tmp4));
    return v;
}

static inline vec4 cross(vec4 const& x, vec4 const& y) {
    __m128 vec0 = _mm_load_ps(&x.x);
    __m128 vec1 = _mm_load_ps(&y.x);
    __m128 tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 tmp2 = _mm_mul_ps(tmp0, vec1);
    __m128 tmp4 = _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
    vec4 v;
    _mm_store_ps(&v.x, _mm_fmsub_ps(tmp0, tmp1, tmp4));
    return v;
}

static inline __m128 cross(__m128 const& vec0, __m128 const& vec1) {
    __m128 tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 tmp2 = _mm_mul_ps(tmp0, vec1);
    __m128 tmp4 = _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
    return _mm_fmsub_ps(tmp0, tmp1, tmp4);
}

static inline quat quat_cast(mat3 const& m) {
    float fourXSquaredMinus1 = m[0].x - m[1].y - m[2].z;
    float fourYSquaredMinus1 = m[1].y - m[0].x - m[2].z;
    float fourZSquaredMinus1 = m[2].z - m[0].x - m[1].y;
    float fourWSquaredMinus1 = m[0].x + m[1].y + m[2].z;

    int biggestIndex = 0;
    float fourBiggestSquaredMinus1 = fourWSquaredMinus1;
    if (fourXSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourXSquaredMinus1;
        biggestIndex = 1;
    }
    if (fourYSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourYSquaredMinus1;
        biggestIndex = 2;
    }
    if (fourZSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourZSquaredMinus1;
        biggestIndex = 3;
    }

    float biggestVal = std::sqrt(fourBiggestSquaredMinus1 + 1.f) / 2.f;
    float mult = 0.25f / biggestVal;

    switch (biggestIndex) {
    case 0:
        return quat { biggestVal, (m[1].z - m[2].y) * mult, (m[2].x - m[0].z) * mult, (m[0].y - m[1].x) * mult };
    case 1:
        return quat { (m[1].z - m[2].y) * mult, biggestVal, (m[0].y + m[1].x) * mult, (m[2].x + m[0].z) * mult };
    case 2:
        return quat { (m[2].x - m[0].z) * mult, (m[0].y + m[1].x) * mult, biggestVal, (m[1].z + m[2].y) * mult };
    case 3:
        return quat { (m[0].y - m[1].x) * mult, (m[2].x + m[0].z) * mult, (m[1].z + m[2].y) * mult, biggestVal };
    default:
        std::unreachable();
    }
}
