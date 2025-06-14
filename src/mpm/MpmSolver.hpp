#include "MpmParticle.hpp"
#include "MpmGrid.hpp"
#include "libs/eigen.hpp"

#include <cmath>
#include <cstring>
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
    MpmSolver(vec3 grid_origin, vec3 grid_size, double grid_spacing, double particle_spacing, vec3 particle_velocity);

    void initialize(); 
    void iterate(double dt);

public:
    MpmGrid grid;
    std::vector<MpmParticle> particles;
    std::vector<vec3> positions;

    std::atomic<bool> is_ready;

private:
    double dt;

    const double particle_spacing;

    const double initial_density = 4.0E2;           // rho_0
    const double critical_compression = 2.5E-2;     // theta_c
    const double critical_stretch = 7.5E-3;         // theta_s
    const double hardening_coefficient = 10.0;      // xi
    const double initial_youngs_modulus = 1.4E5;    // E_0
    const double poisson_ratio = 0.2;               // nu
    const double world_floor = 0.0;

    const vec3 gravity{0.0, -9.81, 0.0};          // g

    // lame coefficients
    const double mu_0 = initial_youngs_modulus / (2.0 * (1.0 + poisson_ratio));
    const double lambda_0 = 
        (initial_youngs_modulus * poisson_ratio) / ((1.0 + poisson_ratio) * (1.0 - 2.0 * poisson_ratio));

    const vec3 v_co = vec3::Zero(); // collider velocity
    const vec3 n_co = vec3(0.0, 1.0, 0.0); // collider normal
    const double mu_surface = 0.5; // Coulomb friction coefficient

    const int max_iterations = 100;
    const double tolerance = 1E-5;

    // 0 for explicit, 1/2 for trapezoidal, 1 for backward euler
    const double beta_integration = 1.0;

    void calculate_Ar(mat3n& residuals, mat3n& Ar, mat3n& df, std::vector<int>& global_to_active_map);
    double N(const double x);
    double d_N(const double x);

    void create_particle_cube(vec3& c, vec3& size, vec3& initial_velocity);
    void create_particle_sphere(vec3& center, double radius, vec3& initial_velocity);

    void step1_rasterize_particles_to_grid();
    void step2_compute_volumes_and_densities();
    void step3_compute_grid_forces();
    void step4_update_grid_velocities();
    void step5_grid_based_collisions();
    void step6_solve_linear_system();
    void step7_update_deformation_gradient();
    void step8_update_particle_velocities();
    void step9_particle_based_collisions();
    void step10_update_particle_positions();
};
