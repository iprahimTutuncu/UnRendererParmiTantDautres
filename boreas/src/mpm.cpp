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
template <typename T, size_t k>
static inline T fast_polar_decompose_R(const T& A) {
    T X = A / std::sqrt((A.transpose() * A).trace());

    for (size_t i = 0; i < k; ++i) {
        X = (X + X.inverse().transpose()) / 2;
    }

    return X;
}

static inline size_t get_node_id_from_local(MpmGrid const& grid, int x, int y,
    int z) {
    return static_cast<size_t>(x + y * grid.width + z * grid.width * grid.height);
}

static inline vec3 get_node_world_coords(MpmGrid const& grid, int x, int y,
    int z) {
    return vec3(grid.origin.x() + x * grid.spacing,
        grid.origin.y() + y * grid.spacing,
        grid.origin.z() + z * grid.spacing);
}

namespace Solver {

    struct solveCR_params {
        int active_id;
        vec3 wip_grad;
    };
    struct calculate_ar_params {
        float svd_det_invt;
        float Fe_det;
        float mu_2x;
        float lambda;
        float volume;
        mat3 Fe_inverse;
        mat3 R;
        mat3 U;
        mat3 V;
        mat3 A_inverse;
        std::array<solveCR_params, 64> gradient;
    };

    // see
    // https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf
    void calculate_Ar(MpmSolver const& solver, mat3n& Av_next, const mat3n& v_next, const std::vector<calculate_ar_params>& _params) {
        const size_t actives_nodes_size = solver.grid.active_nodes.size();

#pragma omp parallel
        {
            vec3* df = new vec3[v_next.cols()]();


#pragma omp for
            for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {

                const auto& param = _params[i];

                // 3.23 - velocity gradient
                mat3 velocities_grad = mat3::Zero();
                for (const auto& d : param.gradient) {
                    velocities_grad += v_next.col(d.active_id) * d.wip_grad.transpose();
                }

                // 3.24 - dFEp
                mat3 dFEp = simulation_dt * velocities_grad * solver.p_current_state.p_deform_elastic[i];

                // 3.30 - RTdR

                mat3 RTdF = param.R.transpose() * dFEp - dFEp.transpose() * param.R;

                const float& b_x = RTdF(1, 0);
                const float& b_y = RTdF(2, 0);
                const float& b_z = RTdF(2, 1);

                // vec3 xyz = A.inverse() * b;
                vec3 xyz = param.A_inverse * vec3(b_x, b_y, b_z);

                const float& xyz_x = xyz.x();
                const float& xyz_y = xyz.y();
                const float& xyz_z = xyz.z();

                // 3.31 - dR

                const mat3& Fe = solver.p_current_state.p_deform_elastic[i];
                const mat3& Fp = solver.p_current_state.p_deform_plastic[i];
                float const& Je = param.Fe_det;

                // JFinvT
                mat3 const& Finv = param.Fe_inverse;
                auto FinvT = Finv.transpose();
                mat3 JFinvT = Je * FinvT;

                // Frobenius inner product
                float JFinvT_dF = (JFinvT.array() * dFEp.array()).sum();

                // using Jacobi's formula for the derivative of the inverse and
                // determinant
                float tr_Finv_dF = (Finv * dFEp).trace();
                mat3 dFinvT = -FinvT * dFEp.transpose() * FinvT;
                mat3 dJFinvT = tr_Finv_dF * JFinvT + Je * dFinvT;

                // 3.26 - Ap
                float const& mu = param.mu_2x;
                float const& lambda = param.lambda;

                mat3& RTdR = RTdF;
                // clang-format off
                RTdR <<    0,  xyz_x, xyz_y,
                      -xyz_x,      0, xyz_z,
                      -xyz_y, -xyz_z,     0;
                // clang-format on
                mat3 Ap = param.volume * (mu * (dFEp - param.R * RTdR) + lambda * JFinvT * JFinvT_dF + lambda * (Je - static_cast<float>(1)) * dJFinvT) * Fe.transpose();

                // 3.25 - df
                for (const auto& d : param.gradient) {
                    df[d.active_id] -= Ap * d.wip_grad;
                }
            }

            for (size_t i = 0; i < actives_nodes_size; ++i) {
                if (df[i] == vec3::Zero()) continue;

                const auto index = solver.grid.active_nodes[i];
                const float& node_mass = solver.grid.nodes[index].mass;
                if (node_mass > EPSILON) [[likely]] {
                    vec3 df_res = params.beta_integration * simulation_dt * df[i] / node_mass;
#pragma omp atomic
                    Av_next.col(i).x() -= df_res.x();
#pragma omp atomic
                    Av_next.col(i).y() -= df_res.y();
#pragma omp atomic
                    Av_next.col(i).z() -= df_res.z();
                }
            }

            delete[] df;
        }
    }

    template <class Vec, class CalculateA>
    static void solveCG(CalculateA A, Vec& x, const Vec& b, int max_iterations, float tolerance) {
        Vec r = b - A(x);
        Vec p = r;

        float rs_old = r.squaredNorm();
        const float b_norm = b.norm();
        const float b_sn = b_norm < EPSILON ? static_cast<float>(1) : b_norm * b_norm;
        const float t_sq = tolerance * tolerance;

        for (int k = 0; k < params.max_iterations_solver; ++k) {
            if (rs_old / b_sn < t_sq) {
                break;
            }

            Vec Ap = A(p);
            float alpha = rs_old / p.cwiseProduct(Ap).sum();

            x += alpha * p;
            r -= alpha * Ap;

            float rs_new = r.squaredNorm();

            if (rs_new / b_sn < t_sq) {
                break;
            }

            float beta = rs_new / rs_old;
            p = r + beta * p;
            rs_old = rs_new;
        }
    }

    template <size_t max_iterations, float tolerance>
    static void solveCR(MpmSolver const& solver, mat3n& x, size_t nb_active_nodes) {
        std::vector<calculate_ar_params> w_ip_gradient(solver.p_current_state.p_position.size());

        const size_t nb_particles = solver.p_current_state.p_position.size();

#pragma omp parallel for
        for (size_t i = 0; i < nb_particles; i++) {
            auto& param = w_ip_gradient[i];
            param.volume = solver.p_current_state.p_volume_0[i];

            {
                Eigen::JacobiSVD<mat3> svd { solver.p_current_state.p_deform_elastic[i],
                    Eigen::ComputeFullU | Eigen::ComputeFullV };

                mat3 tmp;

                mat3 const& U = svd.matrixU();
                mat3 const& V = svd.matrixV();

                mat3& R = tmp;
                R = U * V.transpose();
                if (R.determinant() < static_cast<float>(0)) [[unlikely]] {
                    R = U;
                    R.col(2) *= -1; // flip the last column to ensure R is a rotation matrix
                    R = R * V.transpose();
                }
                param.R = R;
                param.U = U;
                param.V = V;
                mat3& S = tmp;
                S = V * svd.singularValues().asDiagonal() * V.transpose();

#define a00 (S(0, 0) + S(1, 1))
#define a11 (S(0, 0) + S(2, 2))
#define a22 (S(1, 1) + S(2, 2))
#define a01 (S(1, 2))
#define a02 (-S(2, 0))
#define a12 (S(1, 0))
                const float det = 1 / (a00 * a11 * a22 + 2 * a01 * a02 * a12 - a00 * a12 * a12 - a11 * a02 * a02 - a22 * a01 * a01);
#define c00 (a11 * a22 - a12 * a12)
#define c01 (a02 * a12 - a01 * a22)
#define c02 (a01 * a12 - a02 * a11)
#define c11 (a00 * a22 - a02 * a02)
#define c12 (a01 * a02 - a00 * a12)
#define c22 (a00 * a11 - a01 * a01)

                float* p = param.A_inverse.data();
                p[0] = c00 * det;
                p[1] = c01 * det;
                p[2] = c02 * det;
                p[3] = c01 * det;
                p[4] = c11 * det;
                p[5] = c12 * det;
                p[6] = c02 * det;
                p[7] = c12 * det;
                p[8] = c22 * det;

#undef a00
#undef a11
#undef a22
#undef a01
#undef a02
#undef a12
#undef c00
#undef c01
#undef c02
#undef c11
#undef c12
#undef c22
            } // END compute SVD

            const mat3& Fe = solver.p_current_state.p_deform_elastic[i];
            const mat3& Fp = solver.p_current_state.p_deform_plastic[i];

            param.Fe_det = Fe.determinant();
            param.Fe_inverse = Fe.inverse();

            {
                const float Jp = Fp.determinant();
                param.mu_2x = 2 * mu_0 * std::exp(params.hardening_coefficient * (static_cast<float>(1) - Jp));
                param.lambda = lambda_0 * std::exp(params.hardening_coefficient * (static_cast<float>(1) - Jp));
            }

            vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
            vec3i base_position(p_position_rel.x() - 1, p_position_rel.y() - 1, p_position_rel.z() - 1);
            size_t count = 0;
            for (int z = 0; z < 4; ++z) {
                for (int y = 0; y < 4; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth) [[unlikely]]
                            continue;

                        size_t index = get_node_id_from_local(
                            solver.grid, base_position.x() + x, base_position.y() + y,
                            base_position.z() + z);
                        int active_id = solver.global_to_active_map[index];
                        if (active_id < 0) continue;

                        param.gradient[count].active_id = active_id;
                        param.gradient[count].wip_grad = solver.p_weights_gradient[i][x + y * 4 + z * 4 * 4];
                        count += 1;
                    }
                }
            }
        }

        mat3n Ap = x;
        calculate_Ar(solver, Ap, x, w_ip_gradient);
        mat3n r = x - Ap;
        mat3n p = Ap = r;

        calculate_Ar(solver, Ap, r, w_ip_gradient);

        float rAr_old = r.cwiseProduct(Ap).sum();
        const float b_norm = x.norm();
        const float b_sn = b_norm < EPSILON ? static_cast<float>(1) : 1 / (b_norm * b_norm);
        constexpr float t_sq = tolerance * tolerance;

        mat3n Ar;

        for (size_t k = 0; k < max_iterations; ++k) {
            if (r.squaredNorm() * b_sn < t_sq) [[unlikely]] {
                break;
            }

            float alpha = rAr_old / Ap.squaredNorm();

            x += alpha * p;
            r -= alpha * Ap;

            if (r.squaredNorm() * b_sn < t_sq) [[unlikely]] {
                break;
            }
            Ar = r;

            calculate_Ar(solver, Ar, r, w_ip_gradient);
            float rAr_new = (r.cwiseProduct(Ar)).sum();
            float beta = rAr_new / rAr_old;

            p = r + beta * p;
            Ap = Ar + beta * Ap;
            rAr_old = rAr_new;
        }
    }

    template <class Vec, class CalculateA>
    static void solvePCR(CalculateA A, Vec& x, const Vec& b, const Vec& M_inv,
        int max_iterations, float tolerance) {
        Vec r = b - A(x);
        Vec z = r.cwiseProduct(M_inv);
        Vec p = z;

        float rz_old = r.cwiseProduct(z).sum();
        const float b_norm = b.norm();
        const float b_sn = b_norm < EPSILON ? static_cast<float>(1) : b_norm * b_norm;
        const float t_sq = tolerance * tolerance;

        for (int k = 0; k < params.max_iterations_solver; ++k) {
            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            Vec Ap = A(p);
            float alpha = rz_old / Ap.squaredNorm();

            x += alpha * p;
            r -= alpha * Ap;

            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            z = r.cwiseProduct(M_inv);

            float rz_new = (r.cwiseProduct(z)).sum();
            float beta = rz_new / rz_old;

            p = z + beta * p;
            rz_old = rz_new;
        }
    }
} // namespace Solver

static void create_particle_state(MpmParticlesState& state,
    const vec3& position,
    const vec3& initial_velocity,
    const float mass) {
    state.p_position.emplace_back(position);
    state.p_velocity.emplace_back(initial_velocity);
    state.p_mass.push_back(mass);
    state.p_volume_0.push_back(static_cast<float>(0));
    state.p_deform_elastic.emplace_back(mat3::Identity());
    state.p_deform_plastic.emplace_back(mat3::Identity());
    state.p_deform_affine.emplace_back(mat3::Zero());
}

static inline void reset_nodes(MpmSolver& solver) {

#pragma omp parallel
    {
#pragma omp single nowait
        {
            solver.grid.active_nodes.clear();
        }
#pragma omp for schedule(dynamic, 64)
        for (size_t i = 0; i < solver.grid.nodes.size(); ++i) {
            solver.grid.nodes[i].mass = static_cast<float>(0);
            solver.grid.nodes[i].velocity_star.setZero();
            solver.grid.nodes[i].velocity.setZero();
            solver.grid.nodes[i].momentum.setZero();
            solver.grid.nodes[i].force.setZero();
        }
    }
}

// grid basis function to get weights
// dyadic products of one-dimensional cubic B-splines
// x parameter is the position of the particle relative to a given node within
// the eulerian grid
static inline constexpr float N(float x) {
    float a = ((static_cast<float>(2) - std::abs(x)) * (static_cast<float>(2) - std::abs(x)) * (static_cast<float>(2) - std::abs(x))) / static_cast<float>(6);
    if (std::abs(x) < static_cast<float>(1))
        a = (std::abs(x) * std::abs(x) * std::abs(x)) / static_cast<float>(2) - std::abs(x) * std::abs(x) + static_cast<float>(2) / static_cast<float>(3);

    return (std::abs(x) < static_cast<float>(2)) * a;
}

// derivative of the grid basis function
// see [Zhuo Lu 2019] at
// https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf
static inline constexpr float d_N(float x) {
    float sign = (x < static_cast<float>(0)) ? static_cast<float>(-1)
                                             : static_cast<float>(1);
    float a = -sign * (static_cast<float>(0.5) * std::abs(x) * std::abs(x) - std::abs(x) - std::abs(x) + static_cast<float>(2.0));
    if (std::abs(x) < static_cast<float>(1))
        a = sign * (static_cast<float>(1.5) * std::abs(x) * std::abs(x) - std::abs(x) - std::abs(x));

    return (std::abs(x) < static_cast<float>(2)) * a;
}

// float MpmSolver::d_N(const float x) {

// Transfer from particles to grid:
// Transfer mass using the weighing function
// Transfer velocity using normalized weights
static void step1_rasterize_particles_to_grid(MpmSolver& solver) {
#pragma omp parallel for schedule(static)
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {

        const mat3& Fe = solver.p_current_state.p_deform_elastic[i];
        const mat3& Fp = solver.p_current_state.p_deform_plastic[i];

        const mat3 R = fast_polar_decompose_R<mat3, 2>(Fe);

        const auto Jp = Fp.determinant();
        const auto mu = mu_0 * std::exp(params.hardening_coefficient * (static_cast<float>(1.0) - Jp));
        const auto lambda = lambda_0 * std::exp(params.hardening_coefficient * (static_cast<float>(1.0) - Jp));

        const auto Fe_invT = Fe.inverse().transpose();

        const float Je = Fe.determinant();
        const mat3 dPsi = static_cast<float>(2.0) * mu * (Fe - R) + lambda * (Je - static_cast<float>(1.0)) * Je * Fe_invT;
        const mat3 stress_force = solver.p_current_state.p_volume_0[i] * (dPsi * Fe.transpose());

        // find the closest bottom-left node to the current cell
        vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
        vec3i base_position(p_position_rel.x() - 1, p_position_rel.y() - 1, p_position_rel.z() - 1);

        // look at the neighbor 4x4 grid
        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    auto weight_id = x + y * 4 + z * 4 * 4;
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth) [[unlikely]] {
                        solver.p_weights[i][weight_id] = 0.0f;
                        solver.p_weights_gradient[i][weight_id] = vec3::Zero();
                        continue;
                    }

                    // calculate particle offset
                    size_t node_index = get_node_id_from_local(
                        solver.grid, base_position.x() + x, base_position.y() + y,
                        base_position.z() + z);
                    MpmGridNode& node = solver.grid.nodes[node_index];

                    const vec3 p_off = p_position_rel - vec3 {
                        static_cast<float>(base_position.x() + x),
                        static_cast<float>(base_position.y() + y),
                        static_cast<float>(base_position.z() + z),
                    };

                    float Ni_x = N(p_off.x());
                    float Ni_y = N(p_off.y());
                    float Ni_z = N(p_off.z());

                    float dNi_x = d_N(p_off.x());
                    float dNi_y = d_N(p_off.y());
                    float dNi_z = d_N(p_off.z());

                    const float w_ip = solver.p_weights[i][weight_id] = Ni_x * Ni_y * Ni_z;
                    const auto& w_ip_grad = solver.p_weights_gradient[i][weight_id] = solver.grid.one_over_h * vec3(dNi_x * Ni_y * Ni_z, Ni_x * dNi_y * Ni_z, Ni_x * Ni_y * dNi_z);

                    // m_i = sum( m_p * w_ip )
                    // where w_ip = N_i(x_p)
                    float m_i = solver.p_current_state.p_mass[i] * w_ip;

#if USE_APIC
                    vec3 node_pos = get_node_world_coords(solver.grid, base_position.x() + x,
                                        base_position.y() + y,
                                        base_position.z() + z)
                        - solver.p_current_state.p_position[i];
                    vec3 apic = (solver.p_current_state.p_velocity[i] + solver.p_current_state.p_deform_affine[i] * (3 * solver.grid.one_over_h * solver.grid.one_over_h * node_pos));
                    vec3 momentum = m_i * apic;
#else
                    vec3 momentum = m_i * solver.p_current_state.p_velocity[i];
#endif

                    vec3 force = stress_force * w_ip_grad;
#pragma omp atomic
                    node.force.x() -= force.x();
#pragma omp atomic
                    node.force.y() -= force.y();
#pragma omp atomic
                    node.force.z() -= force.z();

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

    for (size_t index = 0; index < solver.grid.nodes.size(); ++index) {
        // v_i = sum( v_p * m_p * w_ip / m_i )
        // p = mv -> v = p/m
        MpmGridNode& node = solver.grid.nodes[index];
        if (node.mass > static_cast<float>(0)) [[unlikely]] {

            vec3 velocity_star = (node.velocity = node.momentum / node.mass) + simulation_dt * (node.force + (node.mass * vec3(params.gravity[0], params.gravity[1], params.gravity[2]))) / node.mass;

            // check for collision with the worlf floor
            if (solver.grid.origin.y() + static_cast<float>((index / solver.grid.width) % solver.grid.height) * solver.grid.spacing <= params.world_floor) [[unlikely]] {
                vec3 v_rel = velocity_star - vec3(params.v_co[0], params.v_co[1], params.v_co[2]);
                float v_n = v_rel.dot(vec3(params.n_co[0], params.n_co[1], params.n_co[2]));

                // if moving towards collider
                if (v_n < static_cast<float>(0.0)) {
                    vec3 v_t = v_rel - (v_n * vec3(params.n_co[0], params.n_co[1], params.n_co[2]));
                    float v_t_norm = v_t.norm();

                    if (v_t_norm > params.mu_surface * v_n) {
                        velocity_star = vec3(params.v_co[0], params.v_co[1], params.v_co[2]);
                    } else {
                        velocity_star = vec3(params.v_co[0], params.v_co[1], params.v_co[2]) + v_t + params.mu_surface * v_n * (v_t / v_t_norm);
                    }
                }
            }
            node.velocity_star = velocity_star;

            solver.grid.active_nodes.push_back(index);
        }
    }
}

// First time step only - initial configuration
static void step2_compute_volumes_and_densities(MpmSolver& solver) {

#pragma omp parallel for
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
        vec3i base_position(p_position_rel.x() - 1, p_position_rel.y() - 1, p_position_rel.z() - 1);

        float rho_p = static_cast<float>(0.0);

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth) [[unlikely]]
                        continue;
                    size_t node_index = get_node_id_from_local(
                        solver.grid, base_position.x() + x, base_position.y() + y,
                        base_position.z() + z);
                    MpmGridNode const& node = solver.grid.nodes[node_index];

                    // rho_p = sum(w_ip * (m_i * / h^3))
                    const float& w_ip = solver.p_weights[i][x + y * 4 + z * 4 * 4];
                    rho_p += node.mass * solver.grid.one_over_h * solver.grid.one_over_h * solver.grid.one_over_h * w_ip;
                }
            }
        }

        // V_p = m_p / rho_p
        if (rho_p > static_cast<float>(0.0)) {
            solver.p_current_state.p_volume_0[i] = solver.p_current_state.p_mass[i] / rho_p;
        }
    }
}

static void step6_solve_linear_system(MpmSolver& solver) {
    if (params.beta_integration == static_cast<float>(0.0) || solver.grid.active_nodes.empty()) [[unlikely]] {
        return;
    }

    const auto& active_nodes = solver.grid.active_nodes;
    const size_t nb_active_nodes = active_nodes.size();
    mat3n b(3, nb_active_nodes);

    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto index = active_nodes[i];
        solver.global_to_active_map[index] = i;
        b.col(i) = solver.grid.nodes[index].velocity_star;
    }

    Solver::solveCR<params.max_iterations_solver, params.tolerance_solver>(solver, b, nb_active_nodes);

#pragma omp parallel
#pragma omp for schedule(static) nowait
    for (size_t i = 0; i < solver.grid.nodes.size(); ++i) {
        solver.global_to_active_map[i] = -1; // reset the map
    }

#pragma omp for nowait
    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto& index = active_nodes[i];
        solver.grid.nodes[index].velocity_star = b.col(i);
    }
}

static void step7_update_deformation_gradient(MpmSolver& solver) {

#pragma omp parallel for
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
        vec3i base_position(p_position_rel.x() - 1, p_position_rel.y() - 1, p_position_rel.z() - 1);

        // 3.23 - velolity gradient
        mat3 velocities_grad = mat3::Zero();

        // look at the neighbor 4x4 grid
        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth) [[unlikely]]
                        continue;
                    const size_t index = get_node_id_from_local(
                        solver.grid, base_position.x() + x, base_position.y() + y,
                        base_position.z() + z);
                    const MpmGridNode& node = solver.grid.nodes[index];

                    const vec3& w_ip_grad = solver.p_weights_gradient[i][x + y * 4 + z * 4 * 4];
                    velocities_grad += node.velocity_star * w_ip_grad.transpose();
                }
            }
        }

        mat3 tmp_FE = (mat3::Identity() + simulation_dt * velocities_grad) * solver.p_current_state.p_deform_elastic[i];
        mat3 const& tmp_FP = solver.p_current_state.p_deform_plastic[i];

        Eigen::JacobiSVD<mat3> svd { tmp_FE,
            Eigen::ComputeFullU | Eigen::ComputeFullV };
        mat3 const& V = svd.matrixV();
        mat3 const& U = svd.matrixU();

        vec3 sigma = svd.singularValues()
                         .cwiseMin(1.f + params.critical_stretch)
                         .cwiseMax(1.f - params.critical_compression);

        solver.p_current_state.p_deform_plastic[i] = V * sigma.cwiseInverse().asDiagonal() * U.transpose() * (tmp_FE * tmp_FP);
        solver.p_current_state.p_deform_elastic[i] = U * sigma.asDiagonal() * V.transpose();
    }
}

static void step8_update_particle_velocities(MpmSolver& solver) {
#pragma omp parallel for
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
        vec3i base_position(p_position_rel.x() - 1, p_position_rel.y() - 1, p_position_rel.z() - 1);

        vec3 v_pic = vec3::Zero();
        vec3 v_flip = vec3::Zero();

#if USE_APIC
        mat3 deform_affine = mat3::Zero();
        // p_current_state.p_deform_affine[i] = mat3::Zero();
#endif

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth) [[unlikely]]
                        continue;
                    const size_t index = get_node_id_from_local(
                        solver.grid, base_position.x() + x, base_position.y() + y,
                        base_position.z() + z);
                    const MpmGridNode& node = solver.grid.nodes[index];

                    vec3 node_pos = get_node_world_coords(solver.grid, base_position.x() + x,
                                        base_position.y() + y,
                                        base_position.z() + z)
                        - solver.p_current_state.p_position[i];
                    float const& w_ip = solver.p_weights[i][x + y * 4 + z * 4 * 4];

                    v_pic += node.velocity_star * w_ip;
                    v_flip += (node.velocity_star - node.velocity) * w_ip;

#if USE_APIC
                    deform_affine += w_ip * node.velocity_star * node_pos.transpose();
#endif
                }
            }
        }

#if USE_APIC
        solver.p_current_state.p_deform_affine[i] = deform_affine;
#endif
        solver.p_current_state.p_velocity[i] = (static_cast<float>(1.0) - params.alpha_blend) * v_pic + params.alpha_blend * (v_flip + solver.p_current_state.p_velocity[i]);
    }
}

static void step9_particle_based_collisions(MpmSolver& solver) {
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        if (solver.p_current_state.p_position[i].y() + simulation_dt * solver.p_current_state.p_velocity[i].y() > params.world_floor) [[unlikely]] {
            continue;
        }

        // velocity relative to collider (ground)
        vec3 v_rel = solver.p_current_state.p_velocity[i] - vec3(params.v_co[0], params.v_co[1], params.v_co[2]);
        float v_n = v_rel.dot(vec3(params.n_co[0], params.n_co[1], params.n_co[2]));

        // if moving towards collider
        if (v_n < static_cast<float>(0.0)) {
            vec3 v_t = v_rel - (v_n * vec3(params.n_co[0], params.n_co[1], params.n_co[2]));
            float v_t_norm = v_t.norm();

            if (v_t_norm > (params.mu_surface * v_n)) {
                solver.p_current_state.p_velocity[i] = vec3(params.v_co[0], params.v_co[1], params.v_co[2]);
            } else {
                solver.p_current_state.p_velocity[i] = v_t + params.mu_surface * v_n * (v_t / v_t_norm) + vec3(params.v_co[0], params.v_co[1], params.v_co[2]);
            }
        }
    }
}

static void step10_update_particle_positions(MpmSolver& solver) {
    std::lock_guard<std::mutex> lock(solver.p_state_mutex);
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        solver.p_current_state.p_position[i] += simulation_dt * solver.p_current_state.p_velocity[i];
    }
}

template <bool first_time>
static void _iterate(MpmSolver& solver) {
    if constexpr (first_time) {
        reset_nodes(solver);
        step1_rasterize_particles_to_grid(solver);
        step2_compute_volumes_and_densities(solver);
        return;
    }
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

    reset_nodes(solver);
    t1 = std::chrono::high_resolution_clock::now();

    step1_rasterize_particles_to_grid(solver);
    t2 = std::chrono::high_resolution_clock::now();

    //  step3_compute_grid_forces();
    t3 = std::chrono::high_resolution_clock::now();

    // step4_update_grid_velocities();
    t4 = std::chrono::high_resolution_clock::now();

    // step5_grid_based_collisions();
    t5 = std::chrono::high_resolution_clock::now();

    step6_solve_linear_system(solver);
    t6 = std::chrono::high_resolution_clock::now();

    //    step6_solve_linear_system_preconditioned<SolverPCR>();
    step7_update_deformation_gradient(solver);
    t7 = std::chrono::high_resolution_clock::now();

    step8_update_particle_velocities(solver);
    t8 = std::chrono::high_resolution_clock::now();

    step9_particle_based_collisions(solver);
    t9 = std::chrono::high_resolution_clock::now();

    step10_update_particle_positions(solver);
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

void MpmSolver::iterate() {
    _iterate<false>(*this);
}

void MpmSolver::initialize() {
    const size_t nb_particles = p_current_state.p_position.size();
    p_weights.resize(nb_particles);
    p_weights_gradient.resize(nb_particles);
    global_to_active_map.assign(grid.nodes.size(), -1);

    _iterate<true>(*this);
}

void MpmSolver::create_particle(vec3 position, vec3 velocity) {
    const float mass = params.initial_density * params.grid_spacing * params.grid_spacing * params.grid_spacing / params.particles_per_cell;
    create_particle_state(p_current_state, position, velocity, mass);
}

#if STEP6_PRECONDITIONED

void MpmSolver::step6_solve_linear_system_preconditioned() {
    if (params.beta_integration == 0.0 || grid.active_nodes.empty())
        [[unlikely]] {
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
    Solver::solvePCR(A, x, b, M_inv, params.max_iterations_solver,
        params.tolerance_solver);

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

#pragma omp parallel for
    for (size_t i = 0; i < p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state.p_position[i] - grid.origin) * grid.one_over_h;
        vec3i base_position(p_position_rel.x() - 1, p_position_rel.y() - 1, p_position_rel.z() - 1);

        const float particle_stiffness = p_current_state.p_volume_0[i] * (static_cast<float>(2.0) * mu_0 + lambda_0) * std::exp(params.hardening_coefficient * (static_cast<float>(1.0) - p_current_state.p_deform_plastic[i].determinant()));

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= grid.depth) [[unlikely]]
                        continue;
                    const size_t index = get_node_id_from_local(
                        grid, base_position.x() + x, base_position.y() + y,
                        base_position.z() + z);

                    int active_id = global_to_active_map[index];
                    if (active_id < 0) continue;

                    const vec3& w_ip_grad = p_weights_gradient[i][x + y * 4 + z * 4 * 4];
                    const float diag_contrib = particle_stiffness * w_ip_grad.squaredNorm();

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

    const float factor = params.beta_integration * simulation_dt * simulation_dt;
    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto index = grid.active_nodes[i];
        float const& node_mass = grid.nodes[i].mass;
        if (node_mass > EPSILON) {
            vec3 A_diag = vec3::Ones() + (factor / node_mass) * P.col(i);
            M_inv.col(i) = A_diag.cwiseInverse();
        } else {
            M_inv.col(i).setOnes();
        }
    }
}

#endif // STEP6_PRECONDITIONED
