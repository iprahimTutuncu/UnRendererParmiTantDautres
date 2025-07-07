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
#include <memory>
#include <mutex>


struct SolverCG {
    template<class Vec, class CalculateA>
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
    template<class Vec, class CalculateA>
    static void solve(CalculateA A, Vec& x, const Vec& b, int max_iterations, double tolerance) {
        Vec r = b - A(x);
        Vec p = r;
        Vec Ap = A(p);

        double rAr_old = r.cwiseProduct(Ap).sum();
        const double b_norm = b.norm();
        const double b_sn = b_norm < EPSILON ? 1.0 : b_norm * b_norm;
        const double t_sq = tolerance * tolerance;

        for (int k = 0; k < max_iterations ; ++k) {
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

            p  = r  + beta * p;
            Ap = Ar + beta * Ap;
            rAr_old = rAr_new;
        }
    }
};

MpmSolver::MpmSolver() :
    params{}
{
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
    std::vector<vec3> positions;
    positions.resize(p_current_state->p_position.size());

    {
        std::lock_guard<std::mutex> lock(p_state_mutex);
        positions = p_current_state->p_position;
    }

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

void MpmSolver::create_particle_cube(vec3& c, vec3& size, vec3& initial_velocity, double particle_spacing) {
    // initialize test particles

    vec3 half_size = size / 2.0;
    vec3 min_c = c - half_size;
    vec3 max_c = c + half_size;

    for (double x = min_c.x(); x <= max_c.x(); x += particle_spacing) {
    for (double y = min_c.y(); y <= max_c.y(); y += particle_spacing) {
    for (double z = min_c.z(); z <= max_c.z(); z += particle_spacing) {
        MpmParticle p{};
        p.position = vec3(x, y, z);
        p.mass = params.initial_density * params.grid_spacing * params.grid_spacing * params.grid_spacing / params.particles_per_cell;
        p.velocity = initial_velocity;
        particles.emplace_back(p);
    }}}
}

static inline double get_random(double min, double max, unsigned int* seed) {
    return min + (rand_r(seed) / (double)RAND_MAX) * (max - min);
}

void MpmSolver::create_particle_sphere(vec3& c, double r, vec3& initial_velocity, double particle_spacing) {
    // Iterate over a bounding box that contains the sphere
    for (double x = c.x() - r; x <= c.x() + r; x += particle_spacing) {
    for (double y = c.y() - r; y <= c.y() + r; y += particle_spacing) {
    for (double z = c.z() - r; z <= c.z() + r; z += particle_spacing) {
        
        vec3 pos(x, y, z);
        
        if ((pos - c).squaredNorm() <= r * r) {
            MpmParticle p{};
            p.position = pos;
            p.mass = params.initial_density * params.grid_spacing * params.grid_spacing * params.grid_spacing / params.particles_per_cell;
            p.velocity = initial_velocity;
            particles.emplace_back(p);
        }
    }}}
}

void MpmSolver::create_particle_clumpy_sphere(
        vec3& c, double r, vec3& initial_velocity, int num_clumps, double clump_radius_factor, unsigned int* seed, double particle_spacing)
{
    for (int i = 0; i < num_clumps; ++i) {
        vec3 clump_center;
        do {
            double offset_x = get_random(-r, r, seed);
            double offset_y = get_random(-r, r, seed);
            double offset_z = get_random(-r, r, seed);
            clump_center = c + vec3(offset_x, offset_y, offset_z);
        } while ((clump_center - c).squaredNorm() > r * r);

        double r1 = get_random(0.5 * r, r, seed) * clump_radius_factor;

        create_particle_sphere(clump_center, r1, initial_velocity, particle_spacing);
    }
}


void MpmSolver::create_particle_sphere_seeded(vec3& c, double r, vec3& initial_velocity, int nb_points, unsigned int* seed) {
    const double mass = params.initial_density * params.grid_spacing * params.grid_spacing * params.grid_spacing / params.particles_per_cell;

    int i = 0;
    do {
        double x = get_random(-r, r, seed);
        double y = get_random(-r, r, seed);
        double z = get_random(-r, r, seed);
        vec3 pos = c + vec3(x, y, z);

        if ((pos - c).squaredNorm() <= r * r) {
            p_current_state->create_particle(pos, initial_velocity, mass);
            p_next_state->create_particle(pos, initial_velocity, mass);
            ++i;
        }
    } while (i < nb_points);
}

// grid basis function to get weights
// dyadic products of one-dimensional cubic B-splines
// x parameter is the position of the particle relative to a given node within the eulerian grid
double MpmSolver::N(const double x) {
    double x_abs = std::abs(x);
    if (x_abs < 1.0) {
        return 1.0 / 2.0 * std::pow(x_abs, 3) - std::pow(x_abs, 2) + 2.0 / 3.0;
    }
    else if (x_abs < 2.0) {
        return -1.0 / 6.0 * std::pow(x_abs, 3) + std::pow(x_abs, 2) - 2.0 * x_abs + 4.0 / 3.0;
    }
    else {
        return 0.0;
    }
}

// derivative of the grid basis function
// see [Zhuo Lu 2019] at https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf
double MpmSolver::d_N(const double x) {
    double x_abs = std::abs(x);
    double sign = (x < 0.0) ? -1.0 : 1.0;
    if (x_abs < 1.0) {
        return sign * (1.5 * x_abs * x_abs - 2.0 * x_abs);
    }
    else if (x_abs < 2.0) {
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

            vec3 w_ip_grad = inv_h * vec3(
                    dNi_x * Ni_y * Ni_z,
                    Ni_x * dNi_y * Ni_z,
                    Ni_x * Ni_y * dNi_z);

            int weight_id = x + y*4 + z*4*4;
            p_weights[i][weight_id] = w_ip;
            p_weights_gradient[i][weight_id] = w_ip_grad;

            // m_i = sum( m_p * w_ip )
            // where w_ip = N_i(x_p)
            double m_i = p_current_state->p_mass[i] * w_ip;
            node->mass += m_i;
            // p = sum( v_p * m_p * w_ip )
            node->momentum += p_current_state->p_velocity[i] * m_i;
        }}}
    }

    for (MpmGridNode& node : grid->nodes) {
        // v_i = sum( v_p * m_p * w_ip / m_i )
        // p = mv -> v = p/m
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
            double w_ip = p_weights[i][x + y*4 + z*4*4];
            rho_p += w_ip * node->mass * inv_h3;
        }}}

        // V_p = m_p / rho_p
        if (rho_p > 0.0) {
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

    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        // Polar decomposition
        // SVD -> A = W S V*
        // Polar -> A = U P
        //      P = V S V*
        //      U = W V*
        Eigen::JacobiSVD<mat3> svd{p_current_state->p_deform_elastic[i], Eigen::ComputeFullU | Eigen::ComputeFullV};
        mat3 U = svd.matrixU();
        mat3 V = svd.matrixV();
        mat3 R = U * V.transpose();
        if (R.determinant() < 0.0) {
            U.col(2) *= -1.0;
            R = U * V.transpose();
        }

        mat3 Fe = p_current_state->p_deform_elastic[i];
        mat3& Fp = p_current_state->p_deform_plastic[i];

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
        mat3 sigma = (1 / J) * dPsi * Fe_T;

        // add force to nodes
        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid->get_node_from_local(node_position_local);
            if (!node) continue;

            vec3 w_ip_grad = p_weights_gradient[i][x + y*4 + z*4*4];
            double volume = Jp * p_current_state->p_volume_0[i];

            node->force -= volume * sigma * w_ip_grad;
        }}}
    }
}

// update velocities using explicit Euler integration
// vi{*} = vi{n} + delta_t * forces_i / mass_i
// forces include internal and external (gravity)
// this will then be used in step 6 in euler semi-implicite integration as the right side of the linear system
void MpmSolver::step4_update_grid_velocities() {
    for (MpmGridNode* node : grid->active_nodes) {
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
    for (MpmGridNode* node : grid->active_nodes) {
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
            }
            else {
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

            vec3 w_ip_grad = p_weights_gradient[i][x + y*4 + z*4*4];
            velocities_grad += v_next.col(active_id) * w_ip_grad.transpose();
        }}}

        // 3.24 - dFEp
        mat3 dFEp = dt * velocities_grad * p_current_state->p_deform_elastic[i];

        // 3.30 - RTdR
        Eigen::JacobiSVD<mat3> svd{p_current_state->p_deform_elastic[i], Eigen::ComputeFullU | Eigen::ComputeFullV};
        mat3 U = svd.matrixU();
        mat3 V = svd.matrixV();
        mat3 R = U * V.transpose();
        if (R.determinant() < 0.0) {
            U.col(2) *= -1.0;
            R = U * V.transpose();
        }

        mat3 S = V * svd.singularValues().asDiagonal() * V.transpose();

        mat3 RTdF = R.transpose() * dFEp - dFEp.transpose() * R;

        vec3 b = vec3(RTdF(1,0), RTdF(2,0), RTdF(2,1));

        mat3 A;
        A << S(0,0)+S(1,1),      S(1,2),         -S(2,0),
             S(2,1),             S(0,0)+S(2,2),  S(1,0),
             -S(2,0),            S(0,1),         S(1,1)+S(2,2);

        // vec3 xyz = A.inverse() * b;
        vec3 xyz = A.fullPivLu().solve(b);

        mat3 RTdR;
        RTdR << 0.0,       xyz.x(),        xyz.y(),
                -xyz.x(),   0.0,           xyz.z(),
                -xyz.y(),   -xyz.z(),       0.0;

        // 3.31 - dR
        mat3 dR = R * RTdR;

        // JFinvT 
        const mat3& Fe = p_current_state->p_deform_elastic[i];
        double Je = Fe.determinant();
        const mat3& Fp = p_current_state->p_deform_plastic[i];
        double Jp = Fp.determinant();

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

        mat3 Ap = p_current_state->p_volume_0[i] * (2.0 * mu * (dFEp - dR) 
                + lambda * JFinvT * JFinvT_dF
                + lambda * (Je - 1.0) * dJFinvT) * Fe.transpose();

        // 3.25 - df
        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid->get_node_from_local(node_position_local);
            if (!node) continue;

            int active_id = global_to_active_map[node->index];
            if (active_id < 0) continue;

            vec3 w_ip_grad = p_weights_gradient[i][x + y*4 + z*4*4];
            df.col(active_id) -= Ap * w_ip_grad;
        }}}
    }

    for (size_t i = 0; i < grid->active_nodes.size(); ++i) {
        Av_next.col(i) = v_next.col(i);
        double& node_mass = grid->active_nodes[i]->mass;
        if (node_mass > 0.0) {
            Av_next.col(i) -= params.beta_integration * dt * df.col(i) / node_mass;
        }
    }
}

template<class Solver>
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
    auto A = [&](const mat3n& v){
        mat3n Av(3, v.cols());
        calculate_Ar(Av, v, df);
        return Av;
    };

    Solver::solve(A, x, b, params.max_iterations, params.tolerance);

    for (int i = 0; i < nb_active_nodes; ++i) {
        grid->active_nodes[i]->velocity_star = x.col(i);
    }
}

void MpmSolver::step7_update_deformation_gradient() {
    double inv_h = 1.0 / grid->spacing;
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

            vec3 w_ip_grad = p_weights_gradient[i][x + y*4 + z*4*4];
            velocities_grad += node->velocity_star * w_ip_grad.transpose();
        }}}

        mat3 tmp_FE = (mat3::Identity() + dt * velocities_grad) * p_current_state->p_deform_elastic[i];
        mat3 tmp_FP = p_current_state->p_deform_plastic[i];
        mat3 F = tmp_FE * tmp_FP;

        Eigen::JacobiSVD<mat3> svd{tmp_FE, Eigen::ComputeFullU | Eigen::ComputeFullV};
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
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        vec3 p_position_rel = (p_current_state->p_position[i] - grid->origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.0).floor().cast<int>();

        // 3.23 - velolity gradient
        vec3 v_pic = vec3::Zero();
        vec3 v_flip = vec3::Zero();

        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid->get_node_from_local(node_position_local);
            if (!node) continue;

            double w_ip = p_weights[i][x + y*4 + z*4*4];
            v_pic += node->velocity_star * w_ip;
            v_flip += (node->velocity_star - node->velocity) * w_ip;
        }}}
        
        v_flip += p_current_state->p_velocity[i];

        double alpha = 0.95;
        p_next_state->p_velocity[i] = (1.0 - alpha) * v_pic + alpha * v_flip;
    }
}

void MpmSolver::step9_particle_based_collisions() {
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        if (p_current_state->p_position[i].y() > params.world_floor) {
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
            }
            else {
                v_rel = v_t + params.mu_surface * v_n * (v_t / v_t_norm);
            }

            p_next_state->p_velocity[i] = v_rel + params.v_co;
        }
    }
}

void MpmSolver::step10_update_particle_positions() {
    for (int i = 0; i < p_current_state->p_position.size(); ++i) {
        p_next_state->p_position[i] += dt * p_next_state->p_velocity[i];
    }
}
