#pragma once

#include "libs/eigen.hpp"


struct MpmParticlesState {
    std::vector<vec3> p_position;               // p
    std::vector<vec3> p_velocity;               // v
    std::vector<double> p_mass;                 // m
    std::vector<double> p_volume_0;             // V
    std::vector<mat3> p_deform_elastic;         // F_E
    std::vector<mat3> p_deform_plastic;         // F_P
    std::vector<mat3> p_deform_affine;          // B

    void resize(size_t size) {
        p_position.resize(size);
        p_velocity.resize(size);
        p_mass.resize(size);
        p_volume_0.resize(size);
        p_deform_elastic.resize(size);
        p_deform_plastic.resize(size);
        p_deform_affine.resize(size);
    }

    void clear() {
        p_position.clear();
        p_velocity.clear();
        p_mass.clear();
        p_volume_0.clear();
        p_deform_elastic.clear();
        p_deform_plastic.clear();
        p_deform_affine.clear();
    }

    void create_particle(const vec3& position, const vec3& initial_velocity, const double& mass) {
        p_position.emplace_back(position);
        p_velocity.emplace_back(initial_velocity);
        p_mass.push_back(mass);
        p_volume_0.push_back(0.0);
        p_deform_elastic.emplace_back(mat3::Identity());
        p_deform_plastic.emplace_back(mat3::Identity());
        p_deform_affine.emplace_back(mat3::Zero());
    }
};
