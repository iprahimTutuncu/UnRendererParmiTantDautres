#pragma once
#include <cstdint>

enum VertexAttributeLocation {
    Position,
    Color,
    NumVertexAttributes, // must be last
};

struct alignas(16) PositionColorVertex {
    float x, y, z;
    std::uint8_t r, g, b, a;
};

struct alignas(16) TriangleVertexIndices {
    std::uint32_t a, b, c;
};
