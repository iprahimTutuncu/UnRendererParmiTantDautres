#include "../eigen.hpp"
#include "MpmGrid.hpp"
#include "MpmParticle.hpp"

#include <memory>
#include <mutex>
#include <vector>

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

class MpmSolver {
public:
    MpmSolver();

    void initialize();
    void iterate(double dt);
    void update_lame_params();

    void swap_buffers();
    std::vector<vec3> get_positions();

    void create_particle_sphere_seeded(vec3& c, double r, vec3& initial_velocity, size_t nb_points, unsigned int* seed);

    struct {
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
    } params;

    std::unique_ptr<MpmGrid> grid;
    std::vector<int> global_to_active_map;

    // Particles
    MpmParticlesState p_states[2];
    MpmParticlesState* p_current_state;
    MpmParticlesState* p_next_state;
    std::mutex p_state_mutex;

    std::vector<std::array<double, 64>> p_weights;
    std::vector<std::array<vec3, 64>> p_weights_gradient;

private:
    double dt;
    double mu_0;
    double lambda_0;

    void calculate_Ar(mat3n& residuals, const mat3n& Ar, mat3n& df) const;

    void compute_preconditioner(mat3n& M_inv) const;

    double N(const double x);
    double d_N(const double x);

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
