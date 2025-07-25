#pragma once

#include <Eigen/Dense>
#include <Eigen/SVD>

using vec3i = Eigen::Vector3i;

using mat3n = Eigen::Matrix<float, 3, Eigen::Dynamic, Eigen::ColMajor>;
using vec3 = Eigen::aligned_allocator<Eigen::Vector3f>::value_type;
using mat3 = Eigen::Matrix3f;

#include <mutex>
#include <vector>

#define STEP6_PRECONDITIONED 0

const float EPSILON = 1E-12;

// Explicit:            dt ~= 10e-5
// Semi-implicit:       dt ~= 0.5e-3
static inline constexpr float simulation_dt = 0.5e-3f;

static inline constexpr float DEFAULT_COMPRESSION = 2.5e-2f;
static inline constexpr float DEFAULT_STRETCH = 7.5e-3f;
static inline constexpr float DEFAULT_HARDENING = 10.0f;
static inline constexpr float DEFAULT_DENSITY = 4.0e2f;
static inline constexpr float DEFAULT_YOUNGS_MODULUS = 1.4e5f;
static inline constexpr float DEFAULT_POISSON_RATIO = 0.2f;

#define HARDWARE_DESTRUCTIVE_INTERFERENCE_SIZE 64
struct alignas(HARDWARE_DESTRUCTIVE_INTERFERENCE_SIZE) MpmGridNode {
    float mass { 0.0f }; // m
    vec3 momentum = vec3::Zero(); // v*
    alignas(16) vec3 velocity_star = vec3::Zero(); // v
    alignas(16) vec3 velocity = vec3::Zero(); // v*
    alignas(16) vec3 force = vec3::Zero(); // F (stress)
};

struct MpmParticlesState {
    std::vector<vec3> p_position; // p
    std::vector<vec3> p_velocity; // v
    std::vector<float> p_mass; // m
    std::vector<float> p_volume_0; // V
    std::vector<mat3> p_deform_elastic; // F_E
    std::vector<mat3> p_deform_plastic; // F_P
    std::vector<mat3> p_deform_affine; // B

    void ensure_capacity(size_t new_size) {
        p_position.reserve(new_size);
        p_velocity.reserve(new_size);
        p_mass.reserve(new_size);
        p_volume_0.reserve(new_size);
        p_deform_elastic.reserve(new_size);
        p_deform_plastic.reserve(new_size);
        p_deform_affine.reserve(new_size);
    }
};

struct MpmGrid {
    const float spacing; // h
    const float one_over_h; // 1 / h
    int width;
    int height;
    int depth;
    vec3 origin; // world space origin of the grid
    std::vector<MpmGridNode> nodes;
    std::vector<std::uint32_t> active_nodes;

    MpmGrid(vec3 origin, float size_x, float size_y, float size_z,
        float spacing)
        : origin { origin }
        , spacing { spacing }
        , one_over_h { 1 / spacing }
        , width { static_cast<int>(std::ceil(size_x / spacing)) + 1 }
        , height { static_cast<int>(std::ceil(size_y / spacing)) + 1 }
        , depth { static_cast<int>(std::ceil(size_z / spacing)) + 1 } {
        size_t nb_nodes = width * height * depth;
        nodes.resize(nb_nodes, MpmGridNode());
    }
};

/*
 To simulate different types of snow, we found the following intu-
ition useful. θc and θs determine when the material starts breaking
(larger = chunky, smaller = powdery). The hardening coefficient
determines how fast the material breaks once it is plastic
(larger = brittle, smaller = ductile). Dry and powdery snow has smaller criti-
cal compression and stretch constants, while the opposite is true for
wet and chunky snow. Icy snow has a higher hardening coefficient
and Young’s modulus, with the opposite producing muddy snow.
*/

struct MpmSolverParams {
    const unsigned int particles_per_cell;
    const float particle_spacing;
    const float grid_spacing;
    const float grid_origin[3];
    const float grid_size[3];

    const float critical_compression; // theta_c
    const float critical_stretch; // theta_s
    const float hardening_coefficient; // xi
    const float initial_density; // rho_0
    const float initial_youngs_modulus; // E_0
    const float poisson_ratio; // nu
    const float gravity[3]; // g

    const float world_floor;
    const float v_co[3]; // collider velocity
    const float n_co[3]; // collider normal
    const float mu_surface; // Coulomb friction coefficient

    const size_t max_iterations_solver;
    const float tolerance_solver;

    const int max_iterations_newton;
    const int max_iterations_line_search;
    const float tolerance_newton;
    const float line_search_constant; // armijo constant
    const float line_search_shrink; // alpha shrink

    const float beta_integration; // 0 for explicit, 1/2 for trapezoidal, 1 for
                                  // backward euler
    const float alpha_blend; // PIC/FLIP blend
};

static constexpr MpmSolverParams params {
    .particles_per_cell = 32,
    .grid_spacing = 0.080,
    .grid_origin = { -2.5, 0.0, -2.5 },
    .grid_size = { 5.0, 3.0, 5.0 },

    .critical_compression = DEFAULT_COMPRESSION,
    .critical_stretch = DEFAULT_STRETCH,
    .hardening_coefficient = DEFAULT_HARDENING * 1.0,
    .initial_density = DEFAULT_DENSITY,
    .initial_youngs_modulus = DEFAULT_YOUNGS_MODULUS * 1.0,
    .poisson_ratio = DEFAULT_POISSON_RATIO * 1.0,
    .gravity = { 0.0, -20.0, 0.0 },

    .world_floor = 0.0,
    .v_co = { 0, 0, 0 },
    .n_co = { 0.0, 1.0, 0.0 },
    .mu_surface = 0.5,

    .max_iterations_solver = 20,
    .tolerance_solver = 1E-5,

    .max_iterations_newton = 20,
    .max_iterations_line_search = 8,
    .tolerance_newton = 1E-4,
    .line_search_constant = 1E-4, // armijo constant
    .line_search_shrink = 0.5, // alpha shrink

    .beta_integration = 1.0,
    .alpha_blend = 0.95,
};

static constexpr float mu_0 = params.initial_youngs_modulus / (static_cast<float>(2) * (static_cast<float>(1) + params.poisson_ratio));
static constexpr float lambda_0 = (params.initial_youngs_modulus * params.poisson_ratio) / ((1.0 + params.poisson_ratio) * (static_cast<float>(1) - static_cast<float>(2) * params.poisson_ratio));

struct MpmSolver {
    MpmGrid grid = MpmGrid(vec3(params.grid_origin[0], params.grid_origin[1],
                               params.grid_origin[2]),
        params.grid_size[0], params.grid_size[1],
        params.grid_size[2], params.grid_spacing);
    std::vector<int> global_to_active_map;

    // Particles
    MpmParticlesState p_current_state;
    std::mutex p_state_mutex;

    std::vector<std::array<float, 64>> p_weights;
    std::vector<std::array<vec3, 64>> p_weights_gradient;

    void create_particle(vec3 position, vec3 velocity);
    void initialize();
    void iterate();

    inline std::vector<vec3> get_positions() {
        std::vector<vec3> positions(p_current_state.p_position.size());

        {
            std::lock_guard<std::mutex> lock(p_state_mutex);
            std::memcpy(positions.data(), p_current_state.p_position.data(),
                p_current_state.p_position.size() * sizeof(decltype(positions[0])));
        }

        return positions;
    }
};
