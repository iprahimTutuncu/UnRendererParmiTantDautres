#pragma once
#include <cstdint>

struct alignas(16) PositionVertex {
    float x, y, z;
};

struct alignas(16) PositionColorVertex {
    float x, y, z;
    std::uint8_t r, g, b, a;
};

struct alignas(16) IndexesVertex {
    std::uint32_t a, b, c;
};
