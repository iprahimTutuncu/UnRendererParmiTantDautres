#pragma once

// SSE4.1 instructions
#include <smmintrin.h>

#ifdef _MSC_VER

constexpr inline __m128 operator*(__m128 const& a, __m128 const& b) {
    return _mm_mul_ps(a, b);
}

constexpr inline __m128 operator+(__m128 const& a, __m128 const& b) {
    return _mm_add_ps(a, b);
}

constexpr inline __m128 operator+(float const& a, __m128 const& b) {
    return _mm_add_ps(_mm_set1_ps(a), b);
}

constexpr inline __m128 operator*(float const& a, __m128 const& b) {
    return _mm_mul_ps(_mm_set1_ps(a), b);
}

#endif
