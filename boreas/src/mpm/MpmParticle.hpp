#pragma once

#include "libs/eigen.hpp"


struct MpmParticle {
    vec3 position = vec3::Zero();               // p
    vec3 velocity = vec3::Zero();               // v
    double mass{0};                             // m
    double volume_0{0};                           // V
    mat3 deform_elastic = mat3::Identity();     // F_E
    mat3 deform_plastic = mat3::Identity();     // F_P

    std::array<double, 64> weights;
    std::array<vec3, 64> weights_gradient;
};
