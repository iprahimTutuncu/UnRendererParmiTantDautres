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
#include <algorithm>
#include <iterator>

// TODO: add variable particle spacing, etc.
MpmSolver::MpmSolver(vec3 grid_origin, vec3 grid_size, double grid_spacing, double particle_spacing) :
    particle_spacing{particle_spacing},
    grid{grid_origin, grid_size.x(), grid_size.y(), grid_size.z(), grid_spacing},
    is_ready{false}
{
    vec3 center = vec3(0.0, 1.0, 0.0);
    create_particle_sphere(center, 0.10);
    positions.resize(particles.size());
}

void MpmSolver::initialize() {
    this->dt = 0.025;

    grid.reset_nodes();
    compute_weights();
    step1_rasterize_particles_to_grid();
    step2_compute_volumes_and_densities();
}

void MpmSolver::iterate(double dt) {
    this->dt = dt;

    grid.reset_nodes();
    compute_weights();
    step1_rasterize_particles_to_grid();
    step3_compute_grid_forces();
    step4_update_grid_velocities();
    step5_grid_based_collisions();
    step6_solve_linear_system();
    step7_update_deformation_gradient();
    step8_update_particle_velocities();
    step9_particle_based_collisions();
    step10_update_particle_positions();

    if (is_ready == false) {
        for (int i = 0; i < particles.size(); ++i) {
            positions[i] = particles[i].position;
        }

        is_ready = true;
    }
}

void MpmSolver::create_particle_cube(vec3& c, vec3& size) {
    // initialize test particles

    vec3 half_size = size / 2.0;
    vec3 min_c = c - half_size;
    vec3 max_c = c + half_size;

    for (double x = min_c.x(); x <= max_c.x(); x += particle_spacing) {
    for (double y = min_c.y(); y <= max_c.y(); y += particle_spacing) {
    for (double z = min_c.z(); z <= max_c.z(); z += particle_spacing) {
        MpmParticle p{};
        p.position = vec3(x, y, z);
        p.density = initial_density;
        p.mass = initial_density * particle_spacing * particle_spacing * particle_spacing;
        p.volume = p.mass / p.density;
        particles.emplace_back(p);
    }}}
}

void MpmSolver::create_particle_sphere(vec3& c, double r) {
    // Iterate over a bounding box that contains the sphere
    for (double x = c.x() - r; x <= c.x() + r; x += particle_spacing) {
    for (double y = c.y() - r; y <= c.y() + r; y += particle_spacing) {
    for (double z = c.z() - r; z <= c.z() + r; z += particle_spacing) {
        
        vec3 pos(x, y, z);
        
        if ((pos - c).squaredNorm() <= r * r) {
            MpmParticle p{};
            p.position = pos;
            p.density = initial_density;
            p.mass = initial_density * particle_spacing * particle_spacing * particle_spacing;
            p.volume = p.mass / p.density; // V = m / rho
            particles.emplace_back(p);
        }
    }}}
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

void MpmSolver::compute_weights() {
    double inv_h = 1.0 / grid.spacing;

    for (MpmParticle& p : particles) {
        vec3 p_position_rel = (p.position - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.5).floor().cast<int>();

        p.weights.fill(0.0);
        p.weights_gradient.fill(vec3::Zero());

        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            int weight_id = x + y*4 + z*4*4;

            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid.get_node_from_local(node_position_local);
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

            p.weights[weight_id] = w_ip;
            p.weights_gradient[weight_id] = w_ip_grad;
        }}}
    }
}

// Transfer from particles to grid:
// Transfer mass using the weighing function 
// Transfer velocity using normalized weights 
void MpmSolver::step1_rasterize_particles_to_grid() {
    double inv_h = 1.0 / grid.spacing;

    for (const MpmParticle& p : particles) {
        // find the closest bottom-left node to the current cell
        vec3 p_position_rel = (p.position - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.5).floor().cast<int>();

        // look at the neighbor 4x4 grid
        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            // calculate particle offset
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid.get_node_from_local(node_position_local);
            if (!node) continue;

            // m_i = sum( m_p * w_ip )
            // where w_ip = N_i(x_p)
            double w_ip = p.weights[x + y*4 + z*4*4];
            double m_i = p.mass * w_ip;
            node->mass += m_i;

            // p = sum( v_p * m_p * w_ip )
            node->momentum += p.velocity * m_i;
        }}}
    }

    for (MpmGridNode& node : grid.nodes) {
        // v_i = sum( v_p * m_p * w_ip / m_i )
        // p = mv -> v = p/m
        if (node.mass > 0.0) {
            node.velocity = node.momentum / node.mass;
        }
    }
}

// First time step only - initial configuration
void MpmSolver::step2_compute_volumes_and_densities() {
    double inv_h = 1.0 / grid.spacing;
    double inv_h3 = inv_h * inv_h * inv_h;

    for (MpmParticle& p : particles) {
        vec3 p_position_rel = (p.position - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.5).floor().cast<int>();

        double rho_p = 0.0;

        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid.get_node_from_local(node_position_local);
            if (!node) continue;

            // rho_p = sum(w_ip * (m_i * / h^3))
            double w_ip = p.weights[x + y*4 + z*4*4];
            rho_p += w_ip * node->mass * inv_h3;
        }}}

        p.density = rho_p;
        // V_p = m_p / rho_p
        if (p.density > 0.0) {
            p.volume = p.mass / p.density;
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

    for (MpmParticle& p : particles) {
        vec3 p_position_rel = (p.position - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.5).floor().cast<int>();

        // prepare values for elasto-plastic enery density function psi
        double J_P = p.deform_plastic.determinant();
        double J_E = p.deform_elastic.determinant();
        double mu = mu_0 * expf(hardening_coefficient * (1.0 - J_P));
        double lambda = lambda_0 * expf(hardening_coefficient * (1.0 - J_P));

        // Polar decomposition
        // SVD -> A = W S V*
        // Polar -> A = U P
        //      P = V S V*
        //      U = W V*
        Eigen::JacobiSVD<mat3> svd{p.deform_elastic, Eigen::ComputeFullU | Eigen::ComputeFullV};
        mat3 U = svd.matrixU();
        mat3 V = svd.matrixV();
        // ensure right-handedness
        if (U.determinant() * V.determinant() < 0.0) {
            V.col(2) *= -1.0;
        }
        mat3 R = U * V.transpose(); // check if proper?
        // mat3 S = V * svd.singularValues().asDiagonal() * V.transpose();

        // compute internal force resulting from elastic stress

        mat3 sigma = -p.volume *
            (2.0f * mu * (p.deform_elastic - R) * p.deform_elastic.transpose()
            + lambda * (J_E - 1.0f) * J_E * mat3::Identity());

        /*
        mat3 Fe = (2.0 * mu * (p.deform_elastic - R) * p.deform_elastic.transpose()
            + lambda * (J_E - 1.0) * J_E * mat3::Identity());
        mat3 cauchy_stress = (1.0 / J_E) * Fe;
        mat3 force = -p.volume * cauchy_stress;
        */

        // add force to nodes
        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid.get_node_from_local(node_position_local);
            if (!node) continue;

            vec3 w_ip_grad = p.weights_gradient[x + y*4 + z*4*4];
            node->force += sigma * w_ip_grad;
        }}}
    }
}

// update velocities using explicit Euler integration
// vi{*} = vi{n} + delta_t * forces_i / mass_i
// forces include internal and external (gravity)
// this will then be used in step 6 in euler semi-implicite integration as the right side of the linear system
void MpmSolver::step4_update_grid_velocities() {
    for (MpmGridNode& node : grid.nodes) {
        if (node.mass > 0.0) {
            vec3 f_ext = node.mass * gravity;
            vec3 f_i = node.force + f_ext;
            node.velocity_star = node.velocity + dt * f_i / node.mass;
        }
    }
}

// collisions are inelastic
// collisions are processed twice each time step, once here, and again before updating positions
// see section 8 of Stomakhin
void MpmSolver::step5_grid_based_collisions() {
    for (int z = 0; z < grid.depth; ++z) {
    for (int y = 0; y < grid.height; ++y) {
    for (int x = 0; x < grid.width; ++x) {
        vec3i node_position_local = vec3i(x, y, z);
        vec3 node_position_world = grid.get_node_world_coords(node_position_local);
        MpmGridNode* node = grid.get_node_from_local(node_position_local);

        if (!node || (node_position_world.y() - world_floor) > EPSILON) {
            continue;
        }

        // velocity relative to collider (ground)
        vec3 v_rel = node->velocity_star - v_co;
        double v_n = v_rel.dot(n_co);

        // if moving towards collider 
        if (v_n < 0.0) {
            vec3 v_t = v_rel - (v_n * n_co);
            double v_t_norm = v_t.norm();

            if (v_t_norm <= (-mu_surface * v_n)) {
                v_rel = vec3::Zero();
            }
            else {
                v_rel = v_t + mu_surface * v_n * (v_t / v_t_norm);
            }

            node->velocity_star = v_rel + v_co;
        }
    }}}
}

// see https://berkeley.mintkit.net/cs284b-projects/mpm-snow/assets/files/docs.pdf
void MpmSolver::calculate_Ar(
        mat3n& Av_next,
        mat3n& v_next, 
        mat3n& df)
{
    df.setZero();
    double inv_h = 1.0 / grid.spacing;

    // calculate Ar
    for (MpmParticle& p : particles) {

        // 3.23 - velocity gradient
        mat3 velocities_grad = mat3::Zero();

        vec3 p_position_rel = (p.position - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.5).floor().cast<int>();

        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid.get_node_from_local(node_position_local);
            if (!node) continue;

            vec3 w_ip_grad = p.weights_gradient[x + y*4 + z*4*4];
            size_t node_id = grid.get_node_id_from_local(node_position_local);
            velocities_grad += v_next.col(node_id) * w_ip_grad.transpose();
        }}}

        // 3.24 - dFEp
        mat3 dFEp = dt * velocities_grad * p.deform_elastic;

        // 3.30 - RTdR
        Eigen::JacobiSVD<mat3> svd{p.deform_elastic, Eigen::ComputeFullU | Eigen::ComputeFullV};
        mat3 U = svd.matrixU();
        mat3 V = svd.matrixV();
        mat3 R = U * V.transpose();

        mat3 S = V * svd.singularValues().asDiagonal() * V.transpose();

        mat3 RTdF = R.transpose() * dFEp - dFEp.transpose() * R;

        vec3 b = vec3(RTdF(1,0), RTdF(2,0), RTdF(2,1));

        mat3 A;
        A << S(0,0)+S(1,1),      S(2,1),         -S(2,0),
             S(1,2),             S(0,0)+S(2,2),  S(0,1),
             -S(2,0),            S(1,0),         S(1,1)+S(2,2);

        vec3 xyz = A.inverse() * b;
//        vec3 xyz = A.fullPivLu().solve(b);

        mat3 RTdR;
        RTdR << 0.0,       -xyz.x(),        -xyz.y(),
                xyz.x(),   0.0,           -xyz.z(),
                xyz.y(),   xyz.z(),       0.0;

        // 3.31 - dR
        mat3 dR = R * RTdR;

        // JFinvT 
        mat3& Fe = p.deform_elastic;
        double Je = Fe.determinant();
        mat3& Fp = p.deform_plastic;
        double Jp = Fp.determinant();

        mat3 Finv = Fe.inverse();
        mat3 FinvT = Finv.transpose();
        mat3 JFinvT = Je * FinvT;

        // Frobenius inner product
        double JFinvT_dF = (JFinvT.array() * dFEp.array()).sum();
        //mat3& CO = JFinvT;

        // d(JFinvT)
        //mat3 tmp = mat3::Zero();
        //mat3 dJFinvT = mat3::Zero();

        //tmp << 
        //    0, 0, 0,
        //    0, CO(2,2), -CO(1,2),
        //    0, -CO(2,1), CO(1,1);
        //dJFinvT(0,0) = (tmp.array() * dFEp.array()).sum();

        //tmp << 
        //    0, 0, 0,
        //    -CO(2,2), 0, CO(0,2),
        //    CO(2,1), 0, -CO(0,1);
        //dJFinvT(0,1) = (tmp.array() * dFEp.array()).sum();
 
        //tmp << 
        //    0, 0, 0,
        //    CO(1,2), -CO(0,2), 0,
        //    -CO(1,1), CO(0,1), 0;         
        //dJFinvT(0,2) = (tmp.array() * dFEp.array()).sum();

        //tmp << 
        //    0, -CO(2,2), CO(1,2),
        //    0, 0, 0,
        //    0, CO(2,0), -CO(1,0);  
        //dJFinvT(1,0) = (tmp.array() * dFEp.array()).sum();

        //tmp << 
        //    CO(2,2), 0, -CO(0,2),
        //    0, 0, 0,
        //    -CO(2,0), 0, CO(0,0);  
        //dJFinvT(1,1) = (tmp.array() * dFEp.array()).sum();

        //tmp << 
        //    -CO(1,2), CO(0,2), 0,
        //    0, 0, 0,
        //    CO(1,0), -CO(0,0), 0; 
        //dJFinvT(1,2) = (tmp.array() * dFEp.array()).sum();

        //tmp << 
        //    0, CO(2,1), -CO(1,1),
        //    0, -CO(2,0), CO(1,0),
        //    0, 0, 0; 
        //dJFinvT(2,0) = (tmp.array() * dFEp.array()).sum();

        //tmp << 
        //    -CO(2,1), 0, CO(0,1),
        //    CO(2,0), 0, -CO(0,0),
        //    0, 0, 0; 
        //dJFinvT(2,1) = (tmp.array() * dFEp.array()).sum();

        //tmp << 
        //    CO(1,1), -CO(0,1), 0,
        //    -CO(0,1), CO(0,0), 0,
        //    0, 0, 0; 
        //dJFinvT(2,2) = (tmp.array() * dFEp.array()).sum();

        double tr_Finv_dF = (Finv * dFEp).trace();
        mat3 dFinvT = -FinvT * dFEp.transpose() * FinvT;
        mat3 dJFinvT = tr_Finv_dF * JFinvT + Je * dFinvT;
//        mat3 dJFinvT = tr_Finv_dF * FinvT + Je * dFinvT;

        // 3.26 - Ap
        double mu = mu_0 * expf(hardening_coefficient * (1.0 - Jp));
        double lambda = lambda_0 * expf(hardening_coefficient * (1.0 - Jp));

        mat3 Ap = -p.volume
            * (2.0 * mu * (dFEp - dR) 
                + lambda * JFinvT * JFinvT_dF
                + lambda * (Je - 1.0) * dJFinvT)
            * p.deform_elastic.transpose();


        // 3.25 - df
        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid.get_node_from_local(node_position_local);
            if (!node) continue;

            vec3 w_ip_grad = p.weights_gradient[x + y*4 + z*4*4];
            size_t node_id = grid.get_node_id_from_local(node_position_local);
            df.col(node_id) += Ap * w_ip_grad;
        }}}
    }

    for (size_t i = 0; i < grid.nodes.size(); ++i) {
        Av_next.col(i) = v_next.col(i);
        double& node_mass = grid.nodes[i].mass;
        if (node_mass > 0.0) {
            Av_next.col(i) -= beta_integration * dt * df.col(i) / node_mass;
        }
    }
}

// We apply the conjugate residual method to solve equation 9 from Stomakhin, solving for v_i{n+1}
// See https://nccastaff.bournemouth.ac.uk/jmacey/MastersProject/MSc15/05Esther/thesisEMdeJong.pdf
// Zhuo Lo's implementation re-calculates Ar and Ap every iteration, but Jong does not.
void MpmSolver::step6_solve_linear_system() {
    if (beta_integration == 0.0) {
        return;
    }

    int nb_nodes = grid.nodes.size();

    mat3n velocity_star(3, nb_nodes);
    mat3n velocity_next(3, nb_nodes);

    mat3n Ax(3, nb_nodes);
    mat3n Ar(3, nb_nodes);
    mat3n Ap(3, nb_nodes);
    mat3n residuals(3, nb_nodes);

    mat3n df(3, nb_nodes);
    mat3n search_dir(3, nb_nodes);

    for (int i = 0; i < nb_nodes; ++i) {
        velocity_next.col(i) = grid.nodes[i].velocity_star; 
        velocity_star.col(i) = grid.nodes[i].velocity_star; 
    }

    calculate_Ar(Ax, velocity_next, df);

    residuals = velocity_star - Ax;
    search_dir = residuals;

    calculate_Ar(Ar, residuals, df);

    double rAr = residuals.cwiseProduct(Ar).sum();
    Ap = Ar;

    for (int k = 0; k < max_iterations && residuals.squaredNorm() >= tolerance; ++k) {

        double rAr_k = rAr;
        double alpha = rAr_k / Ap.cwiseProduct(Ap).sum();
        
        if (abs(alpha) < EPSILON) {
            break;
        }

        velocity_next = velocity_next + alpha * search_dir;
        residuals = residuals - alpha * Ap;

        rAr = residuals.cwiseProduct(Ar).sum();
        double beta = rAr / rAr_k;

        if (abs(beta) < EPSILON) {
            break;
        }

        search_dir = residuals + beta * search_dir;

        Ap = Ar + beta * Ap;
    }

    for (int i = 0; i < nb_nodes; ++i) {
        grid.nodes[i].velocity_star = velocity_next.col(i);
    }
}

void MpmSolver::step7_update_deformation_gradient() {
    double inv_h = 1.0 / grid.spacing;
    for (MpmParticle& p : particles) {
        vec3 p_position_rel = (p.position - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.5).floor().cast<int>();

        // 3.23 - velolity gradient
        mat3 velocities_grad = mat3::Zero();

        // look at the neighbor 4x4 grid
        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid.get_node_from_local(node_position_local);
            if (!node) continue;

            vec3 w_ip_grad = p.weights_gradient[x + y*4 + z*4*4];
            velocities_grad += node->velocity_star * w_ip_grad.transpose();
        }}}

        mat3 tmp_FE = (mat3::Identity() + dt * velocities_grad) * p.deform_elastic;
        mat3 tmp_FP = p.deform_plastic;
        mat3 F = tmp_FE * tmp_FP;

        Eigen::JacobiSVD<mat3> svd{tmp_FE, Eigen::ComputeFullU | Eigen::ComputeFullV};
        vec3 sigma = svd.singularValues();
        mat3 V = svd.matrixV();
        mat3 U = svd.matrixU();

        for (int i = 0; i < 3; ++i) {
            sigma(i) = std::clamp(sigma(i), 1.0f - critical_compression , 1.0f + critical_stretch);
        }

        mat3 sigma_elastic;
        sigma_elastic <<
            sigma.x(), 0.0, 0.0,
            0.0, sigma.y(), 0.0,
            0.0, 0.0, sigma.z();

        mat3 sigma_plastic;
        sigma_plastic <<
            1.0/sigma.x(), 0.0, 0.0,
            0.0, 1.0/sigma.y(), 0.0,
            0.0, 0.0, 1.0/sigma.z();

        p.deform_elastic = U * sigma_elastic * V.transpose();
        p.deform_plastic = V * sigma_plastic * U.transpose() * F;
    }
}

void MpmSolver::step8_update_particle_velocities() {
    double inv_h = 1.0 / grid.spacing;
    for (MpmParticle& p : particles) {
        vec3 p_position_rel = (p.position - grid.origin) * inv_h;
        vec3i base_position = (p_position_rel.array() - 1.5).floor().cast<int>();

        // 3.23 - velolity gradient
        vec3 v_pic = vec3::Zero();
        vec3 v_flip = vec3::Zero();

        for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
        for (int z = 0; z < 4; ++z) {
            vec3i node_position_local = base_position + vec3i(x, y, z);
            MpmGridNode* node = grid.get_node_from_local(node_position_local);
            if (!node) continue;

            double w_ip = p.weights[x + y*4 + z*4*4];
            v_pic += node->velocity_star * w_ip;
            v_flip += (node->velocity_star - node->velocity) * w_ip;
        }}}
        
        v_flip += p.velocity;

        double alpha = 0.95;
        p.velocity = (1.0 - alpha) * v_pic + alpha * v_flip;
    }
}

void MpmSolver::step9_particle_based_collisions() {
    for (MpmParticle& p : particles) {
        if ((p.position.y() - world_floor) > EPSILON) {
            continue;
        }

        // velocity relative to collider (ground)
        vec3 v_rel = p.velocity - v_co;
        double v_n = v_rel.dot(n_co);

        // if moving towards collider 
        if (v_n < 0.0) {
            vec3 v_t = v_rel - (v_n * n_co);
            double v_t_norm = v_t.norm();

            if (v_t_norm <= (-mu_surface * v_n)) {
                v_rel = vec3::Zero();
            }
            else {
                v_rel = v_t + mu_surface * v_n * (v_t / v_t_norm);
            }

            p.velocity = v_rel + v_co;
        }
    }
}

void MpmSolver::step10_update_particle_positions() {
    for (MpmParticle& p : particles) {
        p.position += dt * p.velocity;
    }
}
