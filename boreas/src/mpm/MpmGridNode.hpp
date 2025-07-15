#pragma once

#include "../eigen.hpp"

struct MpmGridNode {
    double mass; // m
    vec3 velocity_star; // v
    vec3 velocity; // v*
    vec3 momentum; // v*
    vec3 force; // F (stress)
    vec3i local_pos;
    int index;
    bool is_active;
};
