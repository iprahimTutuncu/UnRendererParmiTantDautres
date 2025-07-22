#pragma once

#include <Eigen/Dense>
#include <Eigen/SVD>

using vec3i = Eigen::Vector3i;

using mat3n = Eigen::Matrix<double, 3, Eigen::Dynamic, Eigen::ColMajor>;
using vec3 = Eigen::Vector3d;
using mat3 = Eigen::Matrix3d;

#include <mutex>
#include <vector>

const double EPSILON = 1E-12;

// Explicit:            dt ~= 10e-5
// Semi-implicit:       dt ~= 0.5e-3
static inline constexpr double simulation_dt = 0.5e-3;

static inline constexpr double DEFAULT_COMPRESSION = 2.5e-2;
static inline constexpr double DEFAULT_STRETCH = 7.5e-3;
static inline constexpr double DEFAULT_HARDENING = 10.0;
static inline constexpr double DEFAULT_DENSITY = 4.0e2;
static inline constexpr double DEFAULT_YOUNGS_MODULUS = 1.4e5;
static inline constexpr double DEFAULT_POISSON_RATIO = 0.2;

struct MpmGridNode {
    double mass { 0.0 }; // m
    vec3 velocity_star = vec3::Zero(); // v
    vec3 velocity = vec3::Zero(); // v*
    vec3 momentum = vec3::Zero(); // v*
    vec3 force = vec3::Zero(); // F (stress)
};

struct MpmParticlesState {
    std::vector<vec3> p_position; // p
    std::vector<vec3> p_velocity; // v
    std::vector<double> p_mass; // m
    std::vector<double> p_volume_0; // V
    std::vector<mat3> p_deform_elastic; // F_E
    std::vector<mat3> p_deform_plastic; // F_P
    std::vector<mat3> p_deform_affine; // B
};

struct MpmGrid {
    double spacing; // h
    int width;
    int height;
    int depth;
    vec3 origin; // world space origin of the grid
    std::vector<MpmGridNode> nodes;
    std::vector<std::uint32_t> active_nodes;

    MpmGrid() = default;

    MpmGrid(vec3 origin, double size_x, double size_y, double size_z,
        double spacing)
        : origin { origin }
        , spacing { spacing }
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
    unsigned int particles_per_cell;
    double particle_spacing;
    double grid_spacing;
    float grid_origin[3];
    float grid_size[3];

    double critical_compression; // theta_c
    double critical_stretch; // theta_s
    double hardening_coefficient; // xi
    double initial_density; // rho_0
    double initial_youngs_modulus; // E_0
    double poisson_ratio; // nu
    float gravity[3]; // g

    double world_floor;
    float v_co[3]; // collider velocity
    float n_co[3]; // collider normal
    double mu_surface; // Coulomb friction coefficient

    size_t max_iterations_solver;
    double tolerance_solver;

    int max_iterations_newton;
    int max_iterations_line_search;
    double tolerance_newton;
    double line_search_constant; // armijo constant
    double line_search_shrink; // alpha shrink

    double beta_integration; // 0 for explicit, 1/2 for trapezoidal, 1 for
                             // backward euler
    double alpha_blend; // PIC/FLIP blend
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

struct MpmSolver {
    MpmGrid grid;
    std::vector<int> global_to_active_map;

    // Particles
    MpmParticlesState p_current_state;
    std::mutex p_state_mutex;

    std::vector<std::array<double, 64>> p_weights;
    std::vector<std::array<vec3, 64>> p_weights_gradient;

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

    void create_particle(vec3 position, vec3 velocity);

    double mu_0;
    double lambda_0;

    void calculate_Ar(mat3n& residuals, const mat3n& Ar, mat3n& df) const;

    void compute_preconditioner(mat3n& M_inv) const;

    void step1_rasterize_particles_to_grid();
    void step2_compute_volumes_and_densities();
    void step3_compute_grid_forces();
    void step4_update_grid_velocities();
    void step5_grid_based_collisions();
    void step6_solve_linear_system();
    void step6_solve_linear_system_preconditioned();
    void step7_update_deformation_gradient();
    void step8_update_particle_velocities();
    void step9_particle_based_collisions();
    void step10_update_particle_positions();
};
