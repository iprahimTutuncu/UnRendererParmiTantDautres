/*
 * This is an implementation MPM/PIC, as defined in [Stomakhin 2013]
 * It's not optimized, there are redundancies, and a lot of the steps can be combined...
 */

// The Material Point Method for Simulating Continuum Materials
// https://www.math.ucla.edu/~cffjiang/research/mpmcourse/mpmcourse.pdf

// A material point method for snow simulation
// https://media.disneyanimation.com/uploads/production/publication_asset/94/asset/SSCTS13_2.pdf

// Material point method, an almost complete walkthrough
// https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf

// Analysis of a Material Point Method for Snow
// https://studenttheses.uu.nl/bitstream/handle/20.500.12932/25872/ICA-4037324.pdf

#include "mpm.hpp"

#include <chrono>
#include <iostream>

#define USE_APIC 1

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

void resize(MpmParticlesState& state, size_t size) {
    state.p_position.resize(size);
    state.p_velocity.resize(size);
    state.p_mass.resize(size);
    state.p_volume_0.resize(size);
    state.p_deform_elastic.resize(size);
    state.p_deform_plastic.resize(size);
    state.p_deform_affine.resize(size);
}

void clear(MpmParticlesState& state) {
    state.p_position.clear();
    state.p_velocity.clear();
    state.p_mass.clear();
    state.p_volume_0.clear();
    state.p_deform_elastic.clear();
    state.p_deform_plastic.clear();
    state.p_deform_affine.clear();
}

void create_particle_state(MpmParticlesState& state, const vec3& position, const vec3& initial_velocity, const double& mass) {
    state.p_position.emplace_back(position);
    state.p_velocity.emplace_back(initial_velocity);
    state.p_mass.push_back(mass);
    state.p_volume_0.push_back(0.0);
    state.p_deform_elastic.emplace_back(mat3::Identity());
    state.p_deform_plastic.emplace_back(mat3::Identity());
    state.p_deform_affine.emplace_back(mat3::Zero());
}

void reset_nodes(MpmGrid& grid) {
    grid.nodes.assign(grid.nodes.size(), MpmGridNode());
    grid.active_nodes.clear();
}

inline vec3i get_local_pos_from_index(MpmGrid const& grid, size_t index) {
    return {
        static_cast<int>(index % grid.width),
        static_cast<int>((index / grid.width) % grid.height),
        static_cast<int>(index / (grid.width * grid.height)),
    };
}

inline size_t get_node_id_from_local(MpmGrid const& grid, vec3i pos) {
    return static_cast<size_t>(pos.x()) + static_cast<size_t>(pos.y()) * grid.width + static_cast<size_t>(pos.z()) * grid.width * grid.height;
}

inline size_t get_node_id_from_local(MpmGrid const& grid, int x, int y, int z) {
    return static_cast<size_t>(x + y * grid.width + z * grid.width * grid.height);
}

MpmGridNode* get_node_from_local(MpmGrid& grid, int x, int y, int z) {
    if (x < 0 || x >= grid.width || y < 0 || y >= grid.height || z < 0 || z >= grid.depth) [[unlikely]] {
        return nullptr;
    }

    return &grid.nodes[get_node_id_from_local(grid, x, y, z)];
}

MpmGridNode const* get_node_from_local(MpmGrid const& grid, int x, int y, int z) {
    if (x < 0 || x >= grid.width || y < 0 || y >= grid.height || z < 0 || z >= grid.depth) [[unlikely]] {
        return nullptr;
    }

    return &grid.nodes[get_node_id_from_local(grid, x, y, z)];
}

vec3 get_node_world_coords(MpmGrid const& grid, vec3i local_pos) {
    return vec3(
        grid.origin.x() + local_pos.x() * grid.spacing,
        grid.origin.y() + local_pos.y() * grid.spacing,
        grid.origin.z() + local_pos.z() * grid.spacing);
}
vec3 get_node_world_coords(MpmGrid const& grid, int x, int y, int z) {
    return vec3(
        grid.origin.x() + x * grid.spacing,
        grid.origin.y() + y * grid.spacing,
        grid.origin.z() + z * grid.spacing);
}

vec3 get_node_world_coords_from_index(MpmGrid const& grid, size_t index) {
    return vec3(
        grid.origin.x() + static_cast<double>(index % grid.width) * grid.spacing,
        grid.origin.y() + static_cast<double>((index / grid.width) % grid.height) * grid.spacing,
        grid.origin.z() + static_cast<double>(index / (grid.width * grid.height)) * grid.spacing);
}

MpmSolver::MpmSolver()
    : params {} {
}

void MpmSolver::initialize() {
    this->dt = 4.0E-4;

    grid = MpmGrid(
        params.grid_origin,
        params.grid_size.x(), params.grid_size.y(), params.grid_size.z(),
        params.grid_spacing);

    const size_t nb_particles = p_current_state.p_position.size();
    p_weights.resize(nb_particles);
    p_weights_gradient.resize(nb_particles);

    update_lame_params();

    reset_nodes(grid);
    step1_rasterize_particles_to_grid();
    step2_compute_volumes_and_densities();
}

std::vector<vec3> MpmSolver::get_positions() {
    std::vector<vec3> positions;

    {
        std::lock_guard<std::mutex> lock(p_state_mutex);
        positions = p_current_state.p_position;
    }

    return positions;
}

void MpmSolver::update_lame_params() {
    mu_0 = params.initial_youngs_modulus
        / (2.0 * (1.0 + params.poisson_ratio));
    lambda_0 = (params.initial_youngs_modulus * params.poisson_ratio)
        / ((1.0 + params.poisson_ratio) * (1.0 - 2.0 * params.poisson_ratio));
}

void MpmSolver::create_particle(vec3 position, vec3 velocity) {
    const double mass = params.initial_density * params.grid_spacing * params.grid_spacing * params.grid_spacing / params.particles_per_cell;
    create_particle_state(p_current_state, position, velocity, mass);
}

// grid basis function to get weights
// dyadic products of one-dimensional cubic B-splines
// x parameter is the position of the particle relative to a given node within the eulerian grid
static inline constexpr double N(double x) {
    double a = ((static_cast<double>(2) - std::abs(x)) * (static_cast<double>(2) - std::abs(x)) * (static_cast<double>(2) - std::abs(x))) / static_cast<double>(6);
    if (std::abs(x) < static_cast<double>(1))
        a = (std::abs(x) * std::abs(x) * std::abs(x)) / static_cast<double>(2) - std::abs(x) * std::abs(x) + static_cast<double>(2) / static_cast<double>(3);

    return (std::abs(x) < static_cast<double>(2)) * a;
}

// derivative of the grid basis function
// see [Zhuo Lu 2019] at https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf
static inline constexpr double d_N(double x) {
    double sign = (x < static_cast<double>(0)) ? static_cast<double>(-1) : static_cast<double>(1);
    double a = -sign * (static_cast<double>(0.5) * std::abs(x) * std::abs(x) - std::abs(x) - std::abs(x) + static_cast<double>(2.0));
    if (std::abs(x) < static_cast<double>(1))
        a = sign * (static_cast<double>(1.5) * std::abs(x) * std::abs(x) - std::abs(x) - std::abs(x));

    return (std::abs(x) < static_cast<double>(2)) * a;
}

// double MpmSolver::d_N(const double x) {

// Transfer from particles to grid:
// Transfer mass using the weighing function
// Transfer velocity using normalized weights
void MpmSolver::step1_rasterize_particles_to_grid() {
    const double inv_h = 1.0 / grid.spacing;
    const double D_inv = 3.0 * inv_h * inv_h;

#pragma omp parallel
#pragma omp for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        // find the closest bottom-left node to the current cell
        vec3 p_position_rel = (p_current_state.p_position[i] - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        p_weights[i].fill(0.0);
        p_weights_gradient[i].fill(vec3::Zero());

        // look at the neighbor 4x4 grid
        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    // calculate particle offset
                    size_t node_index = get_node_id_from_local(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z);
                    MpmGridNode& node = grid.nodes[node_index];

                    const vec3 p_off = p_position_rel - vec3 {
                        static_cast<double>(base_position.x() + x),
                        static_cast<double>(base_position.y() + y),
                        static_cast<double>(base_position.z() + z),
                    };

                    double Ni_x = N(p_off.x());
                    double Ni_y = N(p_off.y());
                    double Ni_z = N(p_off.z());

                    double dNi_x = d_N(p_off.x());
                    double dNi_y = d_N(p_off.y());
                    double dNi_z = d_N(p_off.z());

                    auto weight_id = x + y * 4 + z * 4 * 4;
                    double w_ip = p_weights[i][weight_id] = Ni_x * Ni_y * Ni_z;
                    p_weights_gradient[i][weight_id] = inv_h * vec3(dNi_x * Ni_y * Ni_z, Ni_x * dNi_y * Ni_z, Ni_x * Ni_y * dNi_z);

                    // m_i = sum( m_p * w_ip )
                    // where w_ip = N_i(x_p)
                    double m_i = p_current_state.p_mass[i] * w_ip;

#if USE_APIC
                    vec3 node_pos = get_node_world_coords(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z) - p_current_state.p_position[i];
                    vec3 apic = (p_current_state.p_velocity[i] + p_current_state.p_deform_affine[i] * D_inv * node_pos);
                    vec3 momentum = m_i * apic;
#else
                    vec3 momentum = m_i * p_current_state.p_velocity[i];
#endif

#pragma omp atomic
                    node.mass += m_i;
#pragma omp atomic
                    node.momentum.x() += momentum.x();
#pragma omp atomic
                    node.momentum.y() += momentum.y();
#pragma omp atomic
                    node.momentum.z() += momentum.z();
                }
            }
        }
    }

#pragma omp single
    for (size_t i = 0; i < grid.nodes.size(); ++i) {
        // v_i = sum( v_p * m_p * w_ip / m_i )
        // p = mv -> v = p/m
        MpmGridNode& node = grid.nodes[i];
        if (node.mass > EPSILON) [[unlikely]] {
            node.velocity = node.momentum / node.mass;
            grid.active_nodes.push_back(i);
        }
    }
}

// First time step only - initial configuration
void MpmSolver::step2_compute_volumes_and_densities() {
    double inv_h = 1.0 / grid.spacing;
    double inv_h3 = inv_h * inv_h * inv_h;

#pragma omp parallel for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state.p_position[i] - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        double rho_p = 0.0;

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= grid.depth) [[unlikely]]
                        continue;
                    MpmGridNode* node = get_node_from_local(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z);
                    [[assume(node != nullptr)]];

                    // rho_p = sum(w_ip * (m_i * / h^3))
                    double w_ip = p_weights[i][x + y * 4 + z * 4 * 4];
                    rho_p += w_ip * node->mass * inv_h3;
                }
            }
        }

        // V_p = m_p / rho_p
        if (rho_p > 0.0) {
            p_current_state.p_volume_0[i] = p_current_state.p_mass[i] / rho_p;
        }
    }
}

// x^_i = x_i
// or... f_i(x^) = -sum(V_p * sigma_p * gradient_w_ip)
// where sigma_p is the Cauchy stress tensor (equation 6 of Stomakhin's paper)
// the Cauchy stress tensor defines the state of stress at a point inside the material in its deformed state
// gradient_w_ip is the gradient of N_i(xp), the derivative of the cubic b-spline N() above
// See p.5 https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf for more on the equations
void MpmSolver::step3_compute_grid_forces() {
    double inv_h = 1.0 / grid.spacing;

#pragma omp parallel for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state.p_position[i] - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        const mat3& Fe = p_current_state.p_deform_elastic[i];
        const mat3& Fp = p_current_state.p_deform_plastic[i];

        const mat3 R = fast_polar_decompose_R(Fe, 2);

        double Jp = Fp.determinant();
        double mu = mu_0 * std::exp(params.hardening_coefficient * (1.0 - Jp));
        double lambda = lambda_0 * std::exp(params.hardening_coefficient * (1.0 - Jp));

        const mat3 Fe_invT = Fe.inverse().transpose();

        double Je = Fe.determinant();
        mat3 dPsi = 2.0 * mu * (Fe - R) + lambda * (Je - 1.0) * Je * Fe_invT;
        mat3 stress_force = p_current_state.p_volume_0[i] * (dPsi * Fe.transpose());

        // add force to nodes
        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= grid.depth) [[unlikely]]
                        continue;
                    MpmGridNode* node = get_node_from_local(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z);
                    [[assume(node != nullptr)]];

                    vec3 w_ip_grad = p_weights_gradient[i][x + y * 4 + z * 4 * 4];

                    vec3 force = stress_force * w_ip_grad;
#pragma omp atomic
                    node->force.x() -= force.x();
#pragma omp atomic
                    node->force.y() -= force.y();
#pragma omp atomic
                    node->force.z() -= force.z();
                }
            }
        }
    }
}

// update velocities using explicit Euler integration
// vi{*} = vi{n} + delta_t * forces_i / mass_i
// forces include internal and external (gravity)
// this will then be used in step 6 in euler semi-implicite integration as the right side of the linear system
void MpmSolver::step4_update_grid_velocities() {
#pragma omp parallel for
    for (size_t i = 0; i < grid.active_nodes.size(); ++i) {
        const auto index = grid.active_nodes[i];
        MpmGridNode& node = grid.nodes[index];

        if (node.mass > EPSILON) {
            vec3 f_ext = node.mass * params.gravity;
            vec3 f_i = node.force + f_ext;
            node.velocity_star = node.velocity + dt * f_i / node.mass;
        }
    }
}

// collisions are inelastic
// collisions are processed twice each time step, once here, and again before updating positions
// see section 8 of Stomakhin
void MpmSolver::step5_grid_based_collisions() {
#pragma omp parallel for
    for (size_t i = 0; i < grid.active_nodes.size(); ++i) {
        const auto index = grid.active_nodes[i];
        vec3 node_position_world = get_node_world_coords_from_index(grid, index);

        if (node_position_world.y() > params.world_floor) {
            continue;
        }

        // velocity relative to collider (ground)
        MpmGridNode& node = grid.nodes[index];
        vec3 v_rel = node.velocity_star - params.v_co;
        double v_n = v_rel.dot(params.n_co);

        // if moving towards collider
        if (v_n < 0.0) {
            vec3 v_t = v_rel - (v_n * params.n_co);
            double v_t_norm = v_t.norm();

            if (v_t_norm <= (-params.mu_surface * v_n)) {
                v_rel = vec3::Zero();
            } else {
                v_rel = v_t + params.mu_surface * v_n * (v_t / v_t_norm);
            }

            node.velocity_star = v_rel + params.v_co;
        }
    }
}

// see https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf
void MpmSolver::calculate_Ar(mat3n& Av_next, const mat3n& v_next, mat3n& df) const {
    df.setZero();
    double inv_h = 1.0 / grid.spacing;

    // calculate Ar
#pragma omp parallel
#pragma omp for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {

        // 3.23 - velocity gradient
        mat3 velocities_grad = mat3::Zero();

        vec3 p_position_rel = (p_current_state.p_position[i] - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= grid.depth) [[unlikely]]
                        continue;

                    size_t index = get_node_id_from_local(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z);
                    int active_id = global_to_active_map[index];
                    if (active_id < 0) continue;

                    vec3 w_ip_grad = p_weights_gradient[i][x + y * 4 + z * 4 * 4];
                    velocities_grad += v_next.col(active_id) * w_ip_grad.transpose();
                }
            }
        }

        // 3.24 - dFEp
        mat3 dFEp = dt * velocities_grad * p_current_state.p_deform_elastic[i];

        // 3.30 - RTdR
        Eigen::JacobiSVD<mat3> svd { p_current_state.p_deform_elastic[i], Eigen::ComputeFullU | Eigen::ComputeFullV };
        mat3 const& U = svd.matrixU();
        mat3 const& V = svd.matrixV();
        mat3 R = U * V.transpose();
        if (R.determinant() < 0.0) [[unlikely]] {
            mat3 b = U;
            b.col(2) *= -1.0; // flip the last column to ensure R is a rotation matrix
            R = b * V.transpose();
        }
        mat3 S = V * svd.singularValues().asDiagonal() * V.transpose();

        mat3 RTdF = R.transpose() * dFEp - dFEp.transpose() * R;

        const double& b_x = RTdF(1, 0);
        const double& b_y = RTdF(2, 0);
        const double& b_z = RTdF(2, 1);

        const double a00 = S(0, 0) + S(1, 1);
        const double a11 = S(0, 0) + S(2, 2);
        const double a22 = S(1, 1) + S(2, 2);
        const double a01 = S(1, 2);
        const double a02 = -S(2, 0);
        const double a12 = S(1, 0);

        const double det = 1.0 / (a00 * a11 * a22 + 2.0 * a01 * a02 * a12 - a00 * a12 * a12 - a11 * a02 * a02 - a22 * a01 * a01);
        const double c00 = a11 * a22 - a12 * a12;
        const double c01 = a02 * a12 - a01 * a22;
        const double c02 = a01 * a12 - a02 * a11;
        const double c11 = a00 * a22 - a02 * a02;
        const double c12 = a01 * a02 - a00 * a12;
        const double c22 = a00 * a11 - a01 * a01;

        // vec3 xyz = A.inverse() * b;
        const double xyz_x = (c00 * b_x + c01 * b_y + c02 * b_z) * det;
        const double xyz_y = (c01 * b_x + c11 * b_y + c12 * b_z) * det;
        const double xyz_z = (c02 * b_x + c12 * b_y + c22 * b_z) * det;

        mat3 RTdR;
        RTdR << 0.0, xyz_x, xyz_y,
            -xyz_x, 0.0, xyz_z,
            -xyz_y, -xyz_z, 0.0;

        // 3.31 - dR
        mat3 dR = R * RTdR;

        const mat3& Fe = p_current_state.p_deform_elastic[i];
        const mat3& Fp = p_current_state.p_deform_plastic[i];
        double Je = Fe.determinant();
        double Jp = Fp.determinant();

        // JFinvT
        mat3 Finv = Fe.inverse();
        mat3 FinvT = Finv.transpose();
        mat3 JFinvT = Je * FinvT;

        // Frobenius inner product
        double JFinvT_dF = (JFinvT.array() * dFEp.array()).sum();

        // using Jacobi's formula for the derivative of the inverse and determinant
        double tr_Finv_dF = (Finv * dFEp).trace();
        mat3 dFinvT = -FinvT * dFEp.transpose() * FinvT;
        mat3 dJFinvT = tr_Finv_dF * JFinvT + Je * dFinvT;

        // 3.26 - Ap
        double mu = mu_0 * expf(params.hardening_coefficient * (1.0 - Jp));
        double lambda = lambda_0 * expf(params.hardening_coefficient * (1.0 - Jp));

        mat3 Ap = p_current_state.p_volume_0[i] * (2.0 * mu * (dFEp - dR) + lambda * JFinvT * JFinvT_dF + lambda * (Je - 1.0) * dJFinvT) * Fe.transpose();

        // 3.25 - df
        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const size_t index = get_node_id_from_local(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z);
                    if (index >= global_to_active_map.size()) [[unlikely]]
                        continue;

                    int active_id = global_to_active_map[index];
                    if (active_id < 0) continue;

                    vec3 w_ip_grad = p_weights_gradient[i][x + y * 4 + z * 4 * 4];

                    vec3 Ap_w = Ap * w_ip_grad;
#pragma omp atomic
                    df.col(active_id).x() -= Ap_w.x();
#pragma omp atomic
                    df.col(active_id).y() -= Ap_w.y();
#pragma omp atomic
                    df.col(active_id).z() -= Ap_w.z();
                }
            }
        }
    }

#pragma omp for
    for (size_t i = 0; i < grid.active_nodes.size(); ++i) {
        Av_next.col(i) = v_next.col(i);
        const auto index = grid.active_nodes[i];
        double node_mass = grid.nodes[i].mass;
        if (node_mass > 0.0) {
            vec3 df_res = params.beta_integration * dt * df.col(i) / node_mass;
#pragma omp atomic
            Av_next.col(i).x() -= df_res.x();
#pragma omp atomic
            Av_next.col(i).y() -= df_res.y();
#pragma omp atomic
            Av_next.col(i).z() -= df_res.z();
        }
    }
}

template <class Solver>
void MpmSolver::step6_solve_linear_system() {
    if (params.beta_integration == 0.0 || grid.active_nodes.empty()) [[unlikely]] {
        return;
    }

    size_t nb_active_nodes = grid.active_nodes.size();
    mat3n b(3, nb_active_nodes);
    global_to_active_map.assign(grid.nodes.size(), -1);
    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto index = grid.active_nodes[i];
        global_to_active_map[index] = i;
        b.col(i) = grid.nodes[index].velocity_star;
    }

    mat3n df(3, nb_active_nodes);
    auto A = [&](const mat3n& v) {
        mat3n Av(3, v.cols());
        calculate_Ar(Av, v, df);
        return Av;
    };

    mat3n x = b;
    Solver::solve(A, x, b, params.max_iterations_solver, params.tolerance_solver);

    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto& index = grid.active_nodes[i];
        grid.nodes[index].velocity_star = x.col(i);
    }
}

template <class Solver>
void MpmSolver::step6_solve_linear_system_preconditioned() {
    if (params.beta_integration == 0.0 || grid.active_nodes.empty()) [[unlikely]] {
        return;
    }

    size_t nb_active_nodes = grid.active_nodes.size();
    global_to_active_map.assign(grid.nodes.size(), -1);
    for (size_t i = 0; i < nb_active_nodes; ++i) {
        global_to_active_map[grid.active_nodes[i]] = i;
    }

    mat3n M_inv;
    compute_preconditioner(M_inv);

    mat3n b(3, nb_active_nodes);
    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto index = grid.active_nodes[i];
        b.col(i) = grid.nodes[index].velocity_star;
    }

    mat3n df(3, nb_active_nodes);
    auto A = [&](const mat3n& v) {
        mat3n Av(3, v.cols());
        calculate_Ar(Av, v, df);
        return Av;
    };

    mat3n x = b;
    Solver::solve(A, x, b, M_inv, params.max_iterations_solver, params.tolerance_solver);

    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto index = grid.active_nodes[i];
        grid.nodes[index].velocity_star = x.col(i);
    }
}

void MpmSolver::compute_preconditioner(mat3n& M_inv) const {
    const size_t nb_active_nodes = grid.active_nodes.size();
    M_inv.resize(3, nb_active_nodes);

    mat3n P(3, nb_active_nodes);
    P.setZero();

    const double h = grid.spacing;

#pragma omp parallel for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state.p_position[i] - grid.origin) / h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        double Jp = p_current_state.p_deform_plastic[i].determinant();
        double mu = mu_0 * std::exp(params.hardening_coefficient * (1.0 - Jp));
        double lambda = lambda_0 * std::exp(params.hardening_coefficient * (1.0 - Jp));

        double particle_stiffness = p_current_state.p_volume_0[i] * (2.0 * mu + lambda);

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const size_t index = get_node_id_from_local(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z);
                    if (index >= global_to_active_map.size()) [[unlikely]]
                        continue;

                    int active_id = global_to_active_map[index];
                    if (active_id < 0) continue;

                    const vec3& w_ip_grad = p_weights_gradient[i][x + y * 4 + z * 4 * 4];
                    double diag_contrib = particle_stiffness * w_ip_grad.squaredNorm();

#pragma omp atomic
                    P(0, active_id) += diag_contrib;
#pragma omp atomic
                    P(1, active_id) += diag_contrib;
#pragma omp atomic
                    P(2, active_id) += diag_contrib;
                }
            }
        }
    }

    double factor = params.beta_integration * dt * dt;
    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto index = grid.active_nodes[i];
        double const& node_mass = grid.nodes[i].mass;
        if (node_mass > EPSILON) {
            vec3 A_diag = vec3::Ones() + (factor / node_mass) * P.col(i);
            M_inv.col(i) = A_diag.cwiseInverse();
        } else {
            M_inv.col(i).setOnes();
        }
    }
}

void MpmSolver::step7_update_deformation_gradient() {
    double inv_h = 1.0 / grid.spacing;

#pragma omp parallel for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state.p_position[i] - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        // 3.23 - velolity gradient
        mat3 velocities_grad = mat3::Zero();

        // look at the neighbor 4x4 grid
        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    MpmGridNode* node = get_node_from_local(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z);
                    if (!node) continue;

                    vec3 w_ip_grad = p_weights_gradient[i][x + y * 4 + z * 4 * 4];
                    velocities_grad += node->velocity_star * w_ip_grad.transpose();
                }
            }
        }

        mat3 tmp_FE = (mat3::Identity() + dt * velocities_grad) * p_current_state.p_deform_elastic[i];
        mat3 const& tmp_FP = p_current_state.p_deform_plastic[i];

        Eigen::JacobiSVD<mat3> svd { tmp_FE, Eigen::ComputeFullU | Eigen::ComputeFullV };
        mat3 const& V = svd.matrixV();
        mat3 const& U = svd.matrixU();

        vec3 sigma = svd.singularValues().cwiseMin(1.f + params.critical_stretch).cwiseMax(1.f - params.critical_compression);

        p_current_state.p_deform_plastic[i] = V * sigma.cwiseInverse().asDiagonal() * U.transpose() * (tmp_FE * tmp_FP);
        p_current_state.p_deform_elastic[i] = U * sigma.asDiagonal() * V.transpose();
    }
}

void MpmSolver::step8_update_particle_velocities() {
    double inv_h = 1.0 / grid.spacing;

#pragma omp parallel for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state.p_position[i] - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        vec3 v_pic = vec3::Zero();
        vec3 v_flip = vec3::Zero();

#if USE_APIC
        // APIC
        p_current_state.p_deform_affine[i] = mat3::Zero();
#endif

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= grid.depth) [[unlikely]]
                        continue;

                    MpmGridNode const* node = get_node_from_local(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z);
                    [[assume(node != nullptr)]]; // assume node is not null, as we check bounds above

                    vec3 node_pos = get_node_world_coords(grid, base_position.x() + x, base_position.y() + y, base_position.z() + z) - p_current_state.p_position[i];
                    double w_ip = p_weights[i][x + y * 4 + z * 4 * 4];
                    v_pic += node->velocity_star * w_ip;
                    v_flip += (node->velocity_star - node->velocity) * w_ip;

#if USE_APIC
                    p_current_state.p_deform_affine[i] += w_ip * node->velocity_star * node_pos.transpose();
#endif
                }
            }
        }

        v_flip += p_current_state.p_velocity[i];
        p_current_state.p_velocity[i] = (1.0 - params.alpha_blend) * v_pic + params.alpha_blend * v_flip;
    }
}

void MpmSolver::step9_particle_based_collisions() {
#pragma omp parallel for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        if (p_current_state.p_position[i].y() + dt * p_current_state.p_velocity[i].y() > params.world_floor) [[unlikely]] {
            continue;
        }

        // velocity relative to collider (ground)
        vec3 v_rel = p_current_state.p_velocity[i] - params.v_co;
        double v_n = v_rel.dot(params.n_co);

        // if moving towards collider
        if (v_n < 0.0) {
            vec3 v_t = v_rel - (v_n * params.n_co);
            double v_t_norm = v_t.norm();

            if (v_t_norm <= (-params.mu_surface * v_n)) {
                v_rel = vec3::Zero();
            } else {
                v_rel = v_t + params.mu_surface * v_n * (v_t / v_t_norm);
            }

            p_current_state.p_velocity[i] = v_rel + params.v_co;
        }
    }
}

void MpmSolver::step10_update_particle_positions() {
    std::lock_guard<std::mutex> lock(p_state_mutex);
#pragma omp parallel for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        p_current_state.p_position[i] += dt * p_current_state.p_velocity[i];
    }
}

void MpmSolver::iterate(double dt) {
    this->dt = dt;

    auto t0 = std::chrono::high_resolution_clock::now();

    auto t1 = t0;
    auto t2 = t0;
    auto t3 = t0;
    auto t4 = t0;
    auto t5 = t0;
    auto t6 = t0;
    auto t7 = t0;
    auto t8 = t0;
    auto t9 = t0;
    auto t10 = t0;

    reset_nodes(grid);
    t1 = std::chrono::high_resolution_clock::now();

    step1_rasterize_particles_to_grid();
    t2 = std::chrono::high_resolution_clock::now();

    step3_compute_grid_forces();
    t3 = std::chrono::high_resolution_clock::now();

    step4_update_grid_velocities();
    t4 = std::chrono::high_resolution_clock::now();

    step5_grid_based_collisions();
    t5 = std::chrono::high_resolution_clock::now();

    step6_solve_linear_system<SolverCR>();
    t6 = std::chrono::high_resolution_clock::now();

    //    step6_solve_linear_system_preconditioned<SolverPCR>();
    step7_update_deformation_gradient();
    t7 = std::chrono::high_resolution_clock::now();

    step8_update_particle_velocities();
    t8 = std::chrono::high_resolution_clock::now();

    step9_particle_based_collisions();
    t9 = std::chrono::high_resolution_clock::now();

    step10_update_particle_positions();
    t10 = std::chrono::high_resolution_clock::now();

    const auto print_duration = [](const char* name, auto t_start, auto t_end) {
        std::cout << name << ": "
                  << std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count()
                  << " us\n";
    };

    print_duration("grid.reset_nodes", t0, t1);
    print_duration("step1_rasterize_particles_to_grid", t1, t2);
    print_duration("step3_compute_grid_forces", t2, t3);
    print_duration("step4_update_grid_velocities", t3, t4);
    print_duration("step5_grid_based_collisions", t4, t5);
    print_duration("step6_solve_linear_system", t5, t6);
    print_duration("step7_update_deformation_gradient", t6, t7);
    print_duration("step8_update_particle_velocities", t7, t8);
    print_duration("step9_particle_based_collisions", t8, t9);
    print_duration("step10_update_particle_positions", t9, t10);
}
