#pragma once

#include "libs/eigen.hpp"


struct MpmGridNode {
    double mass{0.0};                           // m
    vec3 velocity_star = vec3::Zero();          // v
    vec3 velocity = vec3::Zero();               // v*
    vec3 momentum = vec3::Zero();               // v*
    vec3 force = vec3::Zero();                  // F (stress)
};
