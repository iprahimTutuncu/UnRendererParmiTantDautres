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

    MpmGrid(vec3 origin, double size_x, double size_y, double size_z, double spacing)
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
    vec3 grid_origin;
    vec3 grid_size;

    double critical_compression; // theta_c
    double critical_stretch; // theta_s
    double hardening_coefficient; // xi
    double initial_density; // rho_0
    double initial_youngs_modulus; // E_0
    double poisson_ratio; // nu
    vec3 gravity; // g

    double world_floor;
    vec3 v_co; // collider velocity
    vec3 n_co; // collider normal
    double mu_surface; // Coulomb friction coefficient

    int max_iterations_solver;
    double tolerance_solver;

    int max_iterations_newton;
    int max_iterations_line_search;
    double tolerance_newton;
    double line_search_constant; // armijo constant
    double line_search_shrink; // alpha shrink

    double beta_integration; // 0 for explicit, 1/2 for trapezoidal, 1 for backward euler
    double alpha_blend; // PIC/FLIP blend
};

class MpmSolver {
public:
    MpmGrid grid;
    MpmSolverParams params;
    std::vector<int> global_to_active_map;

    // Particles
    MpmParticlesState p_current_state;
    std::mutex p_state_mutex;

    std::vector<std::array<double, 64>> p_weights;
    std::vector<std::array<vec3, 64>> p_weights_gradient;
    MpmSolver();

    void initialize();
    void iterate(double dt);
    void update_lame_params();

    std::vector<vec3> get_positions();

    void create_particle(vec3 position, vec3 velocity);

private:
    double dt;
    double mu_0;
    double lambda_0;

    void calculate_Ar(mat3n& residuals, const mat3n& Ar, mat3n& df) const;

    void compute_preconditioner(mat3n& M_inv) const;

    void step1_rasterize_particles_to_grid();
    void step2_compute_volumes_and_densities();
    void step3_compute_grid_forces();
    void step4_update_grid_velocities();
    void step5_grid_based_collisions();

    template <class Solver>
    void step6_solve_linear_system();

    template <class Solver>
    void step6_solve_linear_system_preconditioned();
    void step7_update_deformation_gradient();
    void step8_update_particle_velocities();
    void step9_particle_based_collisions();
    void step10_update_particle_positions();
};
