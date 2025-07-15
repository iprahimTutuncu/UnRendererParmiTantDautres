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

#include "MpmSolver.hpp"
#include "MpmMath.hpp"

#include <memory>
#include <mutex>

#define USE_APIC 1

MpmSolver::MpmSolver()
    : params {} {
    p_current_state = &p_states[0];
    p_next_state = &p_states[1];
}

void MpmSolver::initialize() {
    this->dt = 4.0E-4;

    grid = std::make_unique<MpmGrid>(
        params.grid_origin,
        params.grid_size.x(), params.grid_size.y(), params.grid_size.z(),
        params.grid_spacing);

    const size_t nb_particles = p_current_state->p_position.size();
    p_weights.resize(nb_particles);
    p_weights_gradient.resize(nb_particles);

    update_lame_params();

    grid->reset_nodes();
    step1_rasterize_particles_to_grid();
    step2_compute_volumes_and_densities();
}

void MpmSolver::swap_buffers() {
    std::lock_guard<std::mutex> lock(p_state_mutex);
    std::swap(p_current_state, p_next_state);
}

std::vector<vec3> MpmSolver::get_positions() {

    std::lock_guard<std::mutex> lock(p_state_mutex);
    std::vector<vec3> positions(p_current_state->p_position.size());
    std::memcpy(positions.data(), p_current_state->p_position.data(), sizeof(vec3) * p_current_state->p_position.size());

    return positions;
}

void MpmSolver::iterate(double dt) {
    this->dt = dt;

    grid->reset_nodes();
    step1_rasterize_particles_to_grid();
    step3_compute_grid_forces();
    step4_update_grid_velocities();
    step5_grid_based_collisions();
    step6_solve_linear_system<SolverCR>();
    //    step6_solve_linear_system_preconditioned<SolverPCR>();
    step7_update_deformation_gradient();
    step8_update_particle_velocities();
    step9_particle_based_collisions();
    step10_update_particle_positions();
}

void MpmSolver::update_lame_params() {
    mu_0 = params.initial_youngs_modulus
        / (2.0 * (1.0 + params.poisson_ratio));
    lambda_0 = (params.initial_youngs_modulus * params.poisson_ratio)
        / ((1.0 + params.poisson_ratio) * (1.0 - 2.0 * params.poisson_ratio));
}

void MpmSolver::create_particle(vec3 position, vec3 velocity) {
    const double mass = params.initial_density * params.grid_spacing * params.grid_spacing * params.grid_spacing / params.particles_per_cell;
    p_current_state->create_particle(position, velocity, mass);
    p_next_state->create_particle(position, velocity, mass);
}

// grid basis function to get weights
// dyadic products of one-dimensional cubic B-splines
// x parameter is the position of the particle relative to a given node within the eulerian grid
double MpmSolver::N(double x) {
    double x_abs = std::abs(x);
    if (x_abs < 1.0) {
        return 1.0 / 2.0 * std::pow(x_abs, 3) - std::pow(x_abs, 2) + 2.0 / 3.0;
    } else if (x_abs < 2.0) {
        return -1.0 / 6.0 * std::pow(x_abs, 3) + std::pow(x_abs, 2) - 2.0 * x_abs + 4.0 / 3.0;
    } else {
        return 0.0;
    }
}

// derivative of the grid basis function
// see [Zhuo Lu 2019] at https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf
double MpmSolver::d_N(double x) {
    double x_abs = std::abs(x);
    double sign = (x < 0.0) ? -1.0 : 1.0;
    if (x_abs < 1.0) {
        return sign * (1.5 * x_abs * x_abs - 2.0 * x_abs);
    } else if (x_abs < 2.0) {
        return -sign * (0.5 * x_abs * x_abs - 2.0 * x_abs + 2.0);
    } else {
        return 0.0;
    }
}

// Transfer from particles to grid:
// Transfer mass using the weighing function
// Transfer velocity using normalized weights
void MpmSolver::step1_rasterize_particles_to_grid() {
    double inv_h = 1.0 / grid->spacing;
    const double D_inv = 3.0 * inv_h * inv_h;

#pragma omp parallel
#pragma omp for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        // find the closest bottom-left node to the current cell
        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        p_weights[i].fill(0.0);
        p_weights_gradient[i].fill(vec3::Zero());

        // look at the neighbor 4x4 grid
        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                for (int z = 0; z < 4; ++z) {
                    // calculate particle offset
                    vec3i node_position_local = base_position + vec3i(x, y, z);
                    MpmGridNode* node = grid->get_node_from_local(node_position_local);
                    if (!node) continue;

                    vec3 p_off = p_position_rel - node_position_local.cast<double>();

                    double Ni_x = N(p_off.x());
                    double Ni_y = N(p_off.y());
                    double Ni_z = N(p_off.z());

                    double dNi_x = d_N(p_off.x());
                    double dNi_y = d_N(p_off.y());
                    double dNi_z = d_N(p_off.z());

                    double w_ip = Ni_x * Ni_y * Ni_z;

                    vec3 w_ip_grad = inv_h * vec3(dNi_x * Ni_y * Ni_z, Ni_x * dNi_y * Ni_z, Ni_x * Ni_y * dNi_z);

                    int weight_id = x + y * 4 + z * 4 * 4;
                    p_weights[i][weight_id] = w_ip;
                    p_weights_gradient[i][weight_id] = w_ip_grad;

                    // m_i = sum( m_p * w_ip )
                    // where w_ip = N_i(x_p)
                    double m_i = p_current_state->p_mass[i] * w_ip;

#pragma omp atomic
                    node->mass += m_i;

#if USE_APIC
                    vec3 node_pos = grid->get_node_world_coords(node->local_pos) - p_current_state->p_position[i];
                    vec3 apic = (p_current_state->p_velocity[i] + p_current_state->p_deform_affine[i] * D_inv * node_pos);
                    vec3 momentum = m_i * apic;
#else
                    vec3 momentum = m_i * p_current_state->p_velocity[i];
#endif
#pragma omp atomic
                    node->momentum.x() += momentum.x();
#pragma omp atomic
                    node->momentum.y() += momentum.y();
#pragma omp atomic
                    node->momentum.z() += momentum.z();
                }
            }
        }
    }

#pragma omp single
    for (int i = 0; i < grid->nodes.size(); ++i) {
        // v_i = sum( v_p * m_p * w_ip / m_i )
        // p = mv -> v = p/m
        MpmGridNode& node = grid->nodes[i];
        if (node.mass > EPSILON) {
            if (!node.is_active) {
                node.is_active = true;
                grid->active_nodes.push_back(&node);
            }

            node.velocity = node.momentum / node.mass;
        }
    }
}

// First time step only - initial configuration
void MpmSolver::step2_compute_volumes_and_densities() {
    double inv_h = 1.0 / grid->spacing;
    double inv_h3 = inv_h * inv_h * inv_h;

#pragma omp parallel for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        double rho_p = 0.0;

        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                for (int z = 0; z < 4; ++z) {
                    vec3i node_position_local = base_position + vec3i(x, y, z);
                    MpmGridNode* node = grid->get_node_from_local(node_position_local);
                    if (!node) continue;

                    // rho_p = sum(w_ip * (m_i * / h^3))
                    double w_ip = p_weights[i][x + y * 4 + z * 4 * 4];
                    rho_p += w_ip * node->mass * inv_h3;
                }
            }
        }

        // V_p = m_p / rho_p
        if (rho_p > 0.0) {
            p_current_state->p_volume_0[i] = p_current_state->p_mass[i] / rho_p;
            p_next_state->p_volume_0[i] = p_current_state->p_mass[i] / rho_p;
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
    double inv_h = 1.0 / grid->spacing;

#pragma omp parallel for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        mat3 Fe = p_current_state->p_deform_elastic[i];
        mat3& Fp = p_current_state->p_deform_plastic[i];

        mat3 R = fast_polar_decompose_R(Fe, 2);

        double Jp = p_current_state->p_deform_plastic[i].determinant();
        double Je = p_current_state->p_deform_elastic[i].determinant();
        double J = (p_current_state->p_deform_plastic[i] * p_current_state->p_deform_elastic[i]).determinant();

        double mu = mu_0 * std::exp(params.hardening_coefficient * (1.0 - Jp));
        double lambda = lambda_0 * std::exp(params.hardening_coefficient * (1.0 - Jp));

        mat3 Fe_T = Fe.transpose();
        mat3 Fp_T = Fp.transpose();
        mat3 Fe_invT = Fe.inverse().transpose();
        mat3 Fp_invT = Fp.inverse().transpose();

        mat3 dPsi = 2.0 * mu * (Fe - R) + lambda * (Je - 1.0) * Je * Fe_invT;
        mat3 sigma = dPsi * Fe_T;
        mat3 stress_force = p_current_state->p_volume_0[i] * sigma;

        // add force to nodes
        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                for (int z = 0; z < 4; ++z) {
                    vec3i node_position_local = base_position + vec3i(x, y, z);
                    MpmGridNode* node = grid->get_node_from_local(node_position_local);
                    if (!node) continue;

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
    for (int i = 0; i < grid->active_nodes.size(); ++i) {
        MpmGridNode* node = grid->active_nodes[i];
        if (node->mass > 0.0) {
            vec3 f_ext = node->mass * params.gravity;
            vec3 f_i = node->force + f_ext;
            node->velocity_star = node->velocity + dt * f_i / node->mass;
        }
    }
}

// collisions are inelastic
// collisions are processed twice each time step, once here, and again before updating positions
// see section 8 of Stomakhin
void MpmSolver::step5_grid_based_collisions() {
#pragma omp parallel for
    for (int i = 0; i < grid->active_nodes.size(); ++i) {
        MpmGridNode* node = grid->active_nodes[i];
        vec3 node_position_world = grid->get_node_world_coords(node->local_pos);

        if (!node || node_position_world.y() > params.world_floor) {
            continue;
        }

        // velocity relative to collider (ground)
        vec3 v_rel = node->velocity_star - params.v_co;
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

            node->velocity_star = v_rel + params.v_co;
        }
    }
}

// see https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf
void MpmSolver::calculate_Ar(mat3n& Av_next, const mat3n& v_next, mat3n& df) const {
    df.setZero();
    double inv_h = 1.0 / grid->spacing;

    // calculate Ar
#pragma omp parallel
#pragma omp for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {

        // 3.23 - velocity gradient
        mat3 velocities_grad = mat3::Zero();

        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                for (int z = 0; z < 4; ++z) {
                    vec3i node_position_local = base_position + vec3i(x, y, z);
                    MpmGridNode* node = grid->get_node_from_local(node_position_local);
                    if (!node) continue;

                    int active_id = global_to_active_map[node->index];
                    if (active_id < 0) continue;

                    vec3 w_ip_grad = p_weights_gradient[i][x + y * 4 + z * 4 * 4];
                    velocities_grad += v_next.col(active_id) * w_ip_grad.transpose();
                }
            }
        }

        // 3.24 - dFEp
        mat3 dFEp = dt * velocities_grad * p_current_state->p_deform_elastic[i];

        const mat3& Fe = p_current_state->p_deform_elastic[i];
        double Je = Fe.determinant();
        const mat3& Fp = p_current_state->p_deform_plastic[i];
        double Jp = Fp.determinant();

        // 3.30 - RTdR
        Eigen::JacobiSVD<mat3> svd { p_current_state->p_deform_elastic[i], Eigen::ComputeFullU | Eigen::ComputeFullV };
        mat3 U = svd.matrixU();
        mat3 V = svd.matrixV();
        mat3 R = U * V.transpose();
        if (R.determinant() < 0.0) {
            U.col(2) *= -1.0;
            R = U * V.transpose();
        }
        mat3 S = V * svd.singularValues().asDiagonal() * V.transpose();

        mat3 RTdF = R.transpose() * dFEp - dFEp.transpose() * R;

        vec3 b = vec3(RTdF(1, 0), RTdF(2, 0), RTdF(2, 1));

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
        vec3 xyz;
        xyz.x() = (c00 * b.x() + c01 * b.y() + c02 * b.z()) * det;
        xyz.y() = (c01 * b.x() + c11 * b.y() + c12 * b.z()) * det;
        xyz.z() = (c02 * b.x() + c12 * b.y() + c22 * b.z()) * det;

        mat3 RTdR;
        RTdR << 0.0, xyz.x(), xyz.y(),
            -xyz.x(), 0.0, xyz.z(),
            -xyz.y(), -xyz.z(), 0.0;

        // 3.31 - dR
        mat3 dR = R * RTdR;

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

        mat3 Ap = p_current_state->p_volume_0[i] * (2.0 * mu * (dFEp - dR) + lambda * JFinvT * JFinvT_dF + lambda * (Je - 1.0) * dJFinvT) * Fe.transpose();

        // 3.25 - df
        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                for (int z = 0; z < 4; ++z) {
                    vec3i node_position_local = base_position + vec3i(x, y, z);
                    MpmGridNode* node = grid->get_node_from_local(node_position_local);
                    if (!node) continue;

                    int active_id = global_to_active_map[node->index];
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
    for (size_t i = 0; i < grid->active_nodes.size(); ++i) {
        Av_next.col(i) = v_next.col(i);
        double& node_mass = grid->active_nodes[i]->mass;
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
    if (params.beta_integration == 0.0 || grid->active_nodes.empty()) {
        return;
    }

    int nb_active_nodes = grid->active_nodes.size();
    global_to_active_map.assign(grid->nodes.size(), -1);
    for (int i = 0; i < nb_active_nodes; ++i) {
        global_to_active_map[grid->active_nodes[i]->index] = i;
    }

    mat3n b(3, nb_active_nodes);
    for (int i = 0; i < nb_active_nodes; ++i) {
        b.col(i) = grid->active_nodes[i]->velocity_star;
    }

    mat3n x = b;

    mat3n df(3, nb_active_nodes);
    auto A = [&](const mat3n& v) {
        mat3n Av(3, v.cols());
        calculate_Ar(Av, v, df);
        return Av;
    };

    Solver::solve(A, x, b, params.max_iterations_solver, params.tolerance_solver);

    for (int i = 0; i < nb_active_nodes; ++i) {
        grid->active_nodes[i]->velocity_star = x.col(i);
    }
}

template <class Solver>
void MpmSolver::step6_solve_linear_system_preconditioned() {
    if (params.beta_integration == 0.0 || grid->active_nodes.empty()) {
        return;
    }

    int nb_active_nodes = grid->active_nodes.size();
    global_to_active_map.assign(grid->nodes.size(), -1);
    for (int i = 0; i < nb_active_nodes; ++i) {
        global_to_active_map[grid->active_nodes[i]->index] = i;
    }

    mat3n M_inv;
    compute_preconditioner(M_inv);

    mat3n b(3, nb_active_nodes);
    for (int i = 0; i < nb_active_nodes; ++i) {
        b.col(i) = grid->active_nodes[i]->velocity_star;
    }

    mat3n x = b;

    mat3n df(3, nb_active_nodes);
    auto A = [&](const mat3n& v) {
        mat3n Av(3, v.cols());
        calculate_Ar(Av, v, df);
        return Av;
    };

    Solver::solve(A, x, b, M_inv, params.max_iterations_solver, params.tolerance_solver);

    for (int i = 0; i < nb_active_nodes; ++i) {
        grid->active_nodes[i]->velocity_star = x.col(i);
    }
}

void MpmSolver::compute_preconditioner(mat3n& M_inv) const {
    int nb_active_nodes = grid->active_nodes.size();
    M_inv.resize(3, nb_active_nodes);

    mat3n P(3, nb_active_nodes);
    P.setZero();

    const double h = grid->spacing;

#pragma omp parallel for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) / h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        double Jp = p_current_state->p_deform_plastic[i].determinant();
        double mu = mu_0 * std::exp(params.hardening_coefficient * (1.0 - Jp));
        double lambda = lambda_0 * std::exp(params.hardening_coefficient * (1.0 - Jp));

        double particle_stiffness = p_current_state->p_volume_0[i] * (2.0 * mu + lambda);

        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                for (int z = 0; z < 4; ++z) {
                    vec3i node_position_local = base_position + vec3i(x, y, z);
                    MpmGridNode* node = grid->get_node_from_local(node_position_local);
                    if (!node || !node->is_active) continue;

                    int active_id = global_to_active_map[node->index];
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
    for (int i = 0; i < nb_active_nodes; ++i) {
        double node_mass = grid->active_nodes[i]->mass;
        if (node_mass > EPSILON) {
            vec3 A_diag = vec3::Ones() + (factor / node_mass) * P.col(i);
            M_inv.col(i) = A_diag.cwiseInverse();
        } else {
            M_inv.col(i).setOnes();
        }
    }
}

void MpmSolver::step7_update_deformation_gradient() {
    double inv_h = 1.0 / grid->spacing;

#pragma omp parallel for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        // 3.23 - velolity gradient
        mat3 velocities_grad = mat3::Zero();

        // look at the neighbor 4x4 grid
        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                for (int z = 0; z < 4; ++z) {
                    vec3i node_position_local = base_position + vec3i(x, y, z);
                    MpmGridNode* node = grid->get_node_from_local(node_position_local);
                    if (!node) continue;

                    vec3 w_ip_grad = p_weights_gradient[i][x + y * 4 + z * 4 * 4];
                    velocities_grad += node->velocity_star * w_ip_grad.transpose();
                }
            }
        }

        mat3 tmp_FE = (mat3::Identity() + dt * velocities_grad) * p_current_state->p_deform_elastic[i];
        mat3 tmp_FP = p_current_state->p_deform_plastic[i];
        mat3 F = tmp_FE * tmp_FP;

        Eigen::JacobiSVD<mat3> svd { tmp_FE, Eigen::ComputeFullU | Eigen::ComputeFullV };
        mat3 V = svd.matrixV();
        mat3 U = svd.matrixU();

        vec3 sigma = svd.singularValues();
        sigma = sigma.cwiseMin(1.f + params.critical_stretch).cwiseMax(1.f - params.critical_compression);

        p_next_state->p_deform_plastic[i] = V * sigma.cwiseInverse().asDiagonal() * U.transpose() * F;
        p_next_state->p_deform_elastic[i] = U * sigma.asDiagonal() * V.transpose();
    }
}

void MpmSolver::step8_update_particle_velocities() {
    double inv_h = 1.0 / grid->spacing;

#pragma omp parallel for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        vec3 v_pic = vec3::Zero();
        vec3 v_flip = vec3::Zero();

#if USE_APIC
        // APIC
        p_next_state->p_deform_affine[i] = mat3::Zero();
#endif

        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                for (int z = 0; z < 4; ++z) {
                    vec3i node_position_local = base_position + vec3i(x, y, z);
                    MpmGridNode* node = grid->get_node_from_local(node_position_local);
                    if (!node) continue;

                    vec3 node_pos = grid->get_node_world_coords(node->local_pos) - p_current_state->p_position[i];
                    double w_ip = p_weights[i][x + y * 4 + z * 4 * 4];
                    v_pic += node->velocity_star * w_ip;
                    v_flip += (node->velocity_star - node->velocity) * w_ip;

#if USE_APIC
                    p_next_state->p_deform_affine[i] += w_ip * node->velocity_star * node_pos.transpose();
#endif
                }
            }
        }

        v_flip += p_current_state->p_velocity[i];
        p_next_state->p_velocity[i] = (1.0 - params.alpha_blend) * v_pic + params.alpha_blend * v_flip;
    }
}

void MpmSolver::step9_particle_based_collisions() {
#pragma omp parallel for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        if (p_current_state->p_position[i].y() + dt * p_next_state->p_velocity[i].y() > params.world_floor) {
            continue;
        }

        // velocity relative to collider (ground)
        vec3 v_rel = p_next_state->p_velocity[i] - params.v_co;
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

            p_next_state->p_velocity[i] = v_rel + params.v_co;
        }
    }
}

void MpmSolver::step10_update_particle_positions() {
#pragma omp parallel for
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        p_next_state->p_position[i] = p_current_state->p_position[i] + dt * p_next_state->p_velocity[i];
    }
}
