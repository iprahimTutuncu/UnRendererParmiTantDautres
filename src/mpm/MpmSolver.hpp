#include "MpmParticle.hpp"
#include "MpmGrid.hpp"
#include "libs/eigen.hpp"

#include <cmath>
#include <cstring>
#include <memory>
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

    void create_particle_cube(vec3& c, vec3& size, vec3& initial_velocity);
    void create_particle_sphere(vec3& center, double radius, vec3& initial_velocity);
    void create_particle_sphere_seeded(vec3& c, double r, vec3& initial_velocity, int nb_points, unsigned int* seed);

    struct {
        double particle_spacing;
        double grid_spacing;
        vec3 grid_origin;
        vec3 grid_size;

        double critical_compression;    // theta_c
        double critical_stretch;        // theta_s
        double hardening_coefficient;   // xi
        double initial_density;         // rho_0
        double initial_youngs_modulus;  // E_0
        double poisson_ratio;           // nu
        vec3 gravity;                   // g

        double world_floor;
        vec3 v_co;                      // collider velocity
        vec3 n_co;                      // collider normal
        double mu_surface;              // Coulomb friction coefficient

        int max_iterations;
        double tolerance;
        double beta_integration;        // 0 for explicit, 1/2 for trapezoidal, 1 for backward euler

    } params;

    std::unique_ptr<MpmGrid> grid;
    std::vector<int> global_to_active_map;

    std::vector<MpmParticle> particles;
    std::vector<vec3> positions;

    std::atomic<bool> is_ready;

private:
    double dt;
    double mu_0;
    double lambda_0;

    void calculate_Ar(mat3n& residuals, const mat3n& Ar, mat3n& df) const;

    double N(const double x);
    double d_N(const double x);

    void create_particle_clumpy_sphere(
            vec3& center, double radius,
            vec3& initial_velocity,
            int num_clumps, double clump_radius_factor, 
            unsigned int* seed);

    void step1_rasterize_particles_to_grid();
    void step2_compute_volumes_and_densities();
    void step3_compute_grid_forces();
    void step4_update_grid_velocities();
    void step5_grid_based_collisions();

    template<class Solver>
    void step6_solve_linear_system();
    void step7_update_deformation_gradient();
    void step8_update_particle_velocities();
    void step9_particle_based_collisions();
    void step10_update_particle_positions();
};
