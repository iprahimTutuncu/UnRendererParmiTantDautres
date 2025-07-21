#pragma once

#include <Eigen/Dense>
#include <Eigen/SVD>

using vec3i = Eigen::Vector3i;

using mat3n = Eigen::Matrix<double, 3, Eigen::Dynamic, Eigen::ColMajor>;
using vec3 = Eigen::Vector3d;
using mat3 = Eigen::Matrix3d;

#include <cmath>
#include <mutex>
#include <vector>

const double EPSILON = 1E-12;

// https://csmbrannon.net/2013/02/14/illustration-of-polar-decomposition/
template <typename T>
inline T fast_polar_decompose_R(const T& A, const int k) {
    double alpha = (A.transpose() * A).trace();
    T X = A / std::sqrt(alpha);

    for (int i = 0; i < k; ++i) {
        X = 0.5 * (X + X.inverse().transpose());
    }

    return X;
}

struct SolverCG {
    template <class Vec, class CalculateA>
    static void solve(CalculateA A, Vec& x, const Vec& b, int max_iterations, double tolerance) {
        Vec r = b - A(x);
        Vec p = r;

        double rs_old = r.squaredNorm();
        const double b_norm = b.norm();
        const double b_sn = b_norm < EPSILON ? 1.0 : b_norm * b_norm;
        const double t_sq = tolerance * tolerance;

        for (int k = 0; k < max_iterations; ++k) {
            if (rs_old / b_sn < t_sq) {
                break;
            }

            Vec Ap = A(p);
            double alpha = rs_old / p.cwiseProduct(Ap).sum();

            x += alpha * p;
            r -= alpha * Ap;

            double rs_new = r.squaredNorm();

            if (rs_new / b_sn < t_sq) {
                break;
            }

            double beta = rs_new / rs_old;
            p = r + beta * p;
            rs_old = rs_new;
        }
    }
};

struct SolverCR {
    template <class Vec, class CalculateA>
    static void solve(CalculateA A, Vec& x, const Vec& b, int max_iterations, double tolerance) {
        Vec r = b - A(x);
        Vec p = r;
        Vec Ap = A(p);

        double rAr_old = r.cwiseProduct(Ap).sum();
        const double b_norm = b.norm();
        const double b_sn = b_norm < EPSILON ? 1.0 : b_norm * b_norm;
        const double t_sq = tolerance * tolerance;

        for (int k = 0; k < max_iterations; ++k) {
            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            double alpha = rAr_old / Ap.squaredNorm();

            x += alpha * p;
            r -= alpha * Ap;

            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            Vec Ar = A(r);
            double rAr_new = (r.cwiseProduct(Ar)).sum();
            double beta = rAr_new / rAr_old;

            p = r + beta * p;
            Ap = Ar + beta * Ap;
            rAr_old = rAr_new;
        }
    }
};

struct SolverPCR {
    template <class Vec, class CalculateA>
    static void solve(CalculateA A, Vec& x, const Vec& b, const Vec& M_inv, int max_iterations, double tolerance) {
        Vec r = b - A(x);
        Vec z = r.cwiseProduct(M_inv);
        Vec p = z;

        double rz_old = r.cwiseProduct(z).sum();
        const double b_norm = b.norm();
        const double b_sn = b_norm < EPSILON ? 1.0 : b_norm * b_norm;
        const double t_sq = tolerance * tolerance;

        for (int k = 0; k < max_iterations; ++k) {
            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            Vec Ap = A(p);
            double alpha = rz_old / Ap.squaredNorm();

            x += alpha * p;
            r -= alpha * Ap;

            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            z = r.cwiseProduct(M_inv);

            double rz_new = (r.cwiseProduct(z)).sum();
            double beta = rz_new / rz_old;

            p = z + beta * p;
            rz_old = rz_new;
        }
    }
};

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

    void reset_nodes() {
        nodes.assign(nodes.size(), MpmGridNode());
        active_nodes.clear();
    }

    inline vec3i get_local_pos_from_index(size_t index) const {
        return {
            static_cast<int>(index % width),
            static_cast<int>((index / width) % height),
            static_cast<int>(index / (width * height)),
        };
    }

    inline size_t get_node_id_from_local(vec3i pos) const {
        return static_cast<size_t>(pos.x()) + static_cast<size_t>(pos.y()) * width + static_cast<size_t>(pos.z()) * width * height;
    }

    inline size_t get_node_id_from_local(int x, int y, int z) const {
        return static_cast<size_t>(x + y * width + z * width * height);
    }

    MpmGridNode* get_node_from_local(int x, int y, int z) {
        if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= depth) [[unlikely]] {
            return nullptr;
        }

        return &nodes[get_node_id_from_local(x, y, z)];
    }

    MpmGridNode const* get_node_from_local(int x, int y, int z) const {
        if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= depth) [[unlikely]] {
            return nullptr;
        }

        return &nodes[get_node_id_from_local(x, y, z)];
    }

    vec3 get_node_world_coords(vec3i local_pos) const {
        return vec3(
            origin.x() + local_pos.x() * spacing,
            origin.y() + local_pos.y() * spacing,
            origin.z() + local_pos.z() * spacing);
    }
    vec3 get_node_world_coords(int x, int y, int z) const {
        return vec3(
            origin.x() + x * spacing,
            origin.y() + y * spacing,
            origin.z() + z * spacing);
    }

    vec3 get_node_world_coords_from_index(size_t index) const {
        return vec3(
            origin.x() + static_cast<double>(index % width) * spacing,
            origin.y() + static_cast<double>((index / width) % height) * spacing,
            origin.z() + static_cast<double>(index / (width * height)) * spacing);
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
    MpmSolver();

    void initialize();
    void iterate(double dt);
    void update_lame_params();

    std::vector<vec3> get_positions();

    void create_particle(vec3 position, vec3 velocity);

    MpmGrid grid;
    MpmSolverParams params;
    std::vector<int> global_to_active_map;

    // Particles
    MpmParticlesState p_current_state;
    std::mutex p_state_mutex;

    std::vector<std::array<double, 64>> p_weights;
    std::vector<std::array<vec3, 64>> p_weights_gradient;

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
