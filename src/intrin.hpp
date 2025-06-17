#pragma once

// SSE4.1 instructions
#include <smmintrin.h>

#ifdef _MSC_VER

constexpr __m128 operator*(__m128 const& a, __m128 const& b) {
    return _mm_mul_ps(a, b);
}

constexpr __m128 operator+(__m128 const& a, __m128 const& b) {
    return _mm_add_ps(a, b);
}

constexpr __m128 operator+(float a, __m128 const& b) {
    return _mm_add_ps(_mm_set1_ps(a), b);
}

constexpr __m128 operator*(float a, __m128 const& b) {
    return _mm_mul_ps(_mm_set1_ps(a), b);
}

constexpr __m128 operator+(__m128 const& b, float a) {
    return _mm_add_ps(_mm_set1_ps(a), b);
}

constexpr __m128 operator*(__m128 const& b, float a) {
    return _mm_mul_ps(_mm_set1_ps(a), b);
}

#endif
