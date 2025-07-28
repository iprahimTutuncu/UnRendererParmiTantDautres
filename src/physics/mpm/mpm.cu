#include "mpm.h"

#include "utils.cuh"

#include <cstdlib>
#include <cuda.h>
#include <iostream>

struct Grid {
    float density; // one over spacing
    int3 bounds;
};

struct GridNode {
    float mass;
    float3 force;
    float3 velocity;
    float3 velocity_star;
};

struct mat3 {
    float3 cols[3];
};

struct Particle {
    float3 position; // p
    float3 velocity; // v
    float mass; // m
    float volume; // V
    mat3 fe; // F_E
    mat3 fp; // F_P
};

struct MpmState {
    Grid grid;
    Particle* particles;
    GridNode* nodes;
    unsigned* global_to_active_map;

    dim3 particleDims;
    dim3 gridDims;
};

struct MpmSolverParams {
    float one_over_radius; // 1 / h
    float deltaT;

    float compression;// theta_c
    float stretch;// theta_s
    float hardening;// xi
    float alpha;
    float density; // ρ

    float mu;
    float lambda; // λ

    int num_particles;

    int3 grid_corner_1;
    int3 grid_corner_2;
    float3 gravity;
};

__constant__ MpmParams mpm_params;
// constexpr size_t L1_CACHE_LINE_SIZE = 128;

MPM_AppResult mpm_init(void** appstate, MpmParams const& params, ) {
    print_cuda_devices_info();

    auto state_ptr = std::malloc(sizeof(MpmState));
    if (!state_ptr) {
        std::cerr << "Failed to allocate host memory for MPM state.\n";
        return MPM_APP_FAILURE;
    }

    MpmState& state = *static_cast<MpmState*>(state_ptr);

    checkCuda(cudaMemcpyToSymbol(mpm_params, &params, sizeof(MpmParams), 0, cudaMemcpyHostToDevice));

    *appstate = state_ptr;
    return MPM_APP_CONTINUE;
}

MPM_AppResult mpm_iterate(void* appstate) {
    [[maybe_unused]] MpmState& state = *static_cast<MpmState*>(appstate);
    return MPM_APP_CONTINUE;
}

void mpm_quit(void* appstate) {
    if (appstate) {
        std::free(appstate);
    }
}
