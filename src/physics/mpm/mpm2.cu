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

#define USE_APIC 1

#include <Eigen/Dense>
#include <Eigen/SVD>
#include <cmath>
#include <cstddef>

using vec3i = Eigen::Vector3i;

using mat3n = Eigen::Matrix<float, 3, Eigen::Dynamic, Eigen::ColMajor>;
using vec3 = Eigen::aligned_allocator<Eigen::Vector3f>::value_type;
using mat3 = Eigen::Matrix3f;

#include <mutex>
#include <vector>

#define STEP6_PRECONDITIONED 0

const float EPSILON = 1E-12f;

// Explicit:            dt ~= 10e-5
// Semi-implicit:       dt ~= 0.5e-3
static inline constexpr float simulation_dt = 0.5e-3f;

static inline constexpr float DEFAULT_COMPRESSION = 2.5e-2f;
static inline constexpr float DEFAULT_STRETCH = 7.5e-3f;
static inline constexpr float DEFAULT_HARDENING = 10.0f;
static inline constexpr float DEFAULT_DENSITY = 4.0e2f;
static inline constexpr float DEFAULT_YOUNGS_MODULUS = 1.4e5f;
static inline constexpr float DEFAULT_POISSON_RATIO = 0.2f;

#define HARDWARE_DESTRUCTIVE_INTERFERENCE_SIZE 128

struct alignas(HARDWARE_DESTRUCTIVE_INTERFERENCE_SIZE) MpmGridNode {
    float mass { 0.0f }; // m
    vec3 momentum = vec3::Zero(); // v*
    alignas(16) vec3 velocity_star = vec3::Zero(); // v
    alignas(16) vec3 velocity = vec3::Zero(); // v*
    alignas(16) vec3 force = vec3::Zero(); // F (stress)
};

struct MpmParticlesState {
    std::vector<vec3> p_position; // p
    std::vector<vec3> p_velocity; // v
    std::vector<float> p_mass; // m
    std::vector<float> p_volume_0; // V
    std::vector<mat3> p_deform_elastic; // F_E
    std::vector<mat3> p_deform_plastic; // F_P
    std::vector<mat3> p_deform_affine; // B

    void ensure_capacity(size_t new_size) {
        p_position.reserve(new_size);
        p_velocity.reserve(new_size);
        p_mass.reserve(new_size);
        p_volume_0.reserve(new_size);
        p_deform_elastic.reserve(new_size);
        p_deform_plastic.reserve(new_size);
        p_deform_affine.reserve(new_size);
    }
};

struct MpmGrid {
    const float spacing; // h
    const float one_over_h; // 1 / h
    int width;
    int height;
    int depth;
    vec3 origin; // world space origin of the grid
    std::vector<MpmGridNode> nodes;
    std::vector<std::uint32_t> active_nodes;

    MpmGrid(vec3 origin, float size_x, float size_y, float size_z,
        float spacing)
        : spacing { spacing }
        , one_over_h { 1 / spacing }
        , width { static_cast<int>(std::ceil(size_x / spacing)) + 1 }
        , height { static_cast<int>(std::ceil(size_y / spacing)) + 1 }
        , depth { static_cast<int>(std::ceil(size_z / spacing)) + 1 }
        , origin { origin } {
        size_t nb_nodes = static_cast<size_t>(width * height * depth);
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
    float particles_per_cell;
    float particle_spacing;
    float grid_spacing;
    float3 grid_origin;
    float3 grid_size;

    float critical_compression; // theta_c
    float critical_stretch; // theta_s
    float hardening_coefficient; // xi
    float initial_density; // rho_0
    float initial_youngs_modulus; // E_0
    float poisson_ratio; // nu
    float3 gravity; // g

    float world_floor;
    float3 v_co; // collider velocity
    float3 n_co; // collider normal
    float mu_surface; // Coulomb friction coefficient

    size_t max_iterations_solver;
    float tolerance_solver;

    int max_iterations_newton;
    int max_iterations_line_search;
    float tolerance_newton;
    float line_search_constant; // armijo constant
    float line_search_shrink; // alpha shrink

    float beta_integration; // 0 for explicit, 1/2 for trapezoidal, 1 for
                            // backward euler
    float alpha_blend; // PIC/FLIP blend

    float mu_0;
    float lambda_0;
};

__constant__ MpmSolverParams static_params;

void defineparams() {
    MpmSolverParams params;

    params.particles_per_cell = 32,
    params.grid_spacing = 0.080f,
    params.grid_origin = make_float3(-2.5, 0.0, -2.5),
    params.grid_size = make_float3(5.0, 3.0, 5.0),

    params.critical_compression = DEFAULT_COMPRESSION,
    params.critical_stretch = DEFAULT_STRETCH,
    params.hardening_coefficient = DEFAULT_HARDENING * 1.0,
    params.initial_density = DEFAULT_DENSITY,
    params.initial_youngs_modulus = DEFAULT_YOUNGS_MODULUS * 1.0,
    params.poisson_ratio = DEFAULT_POISSON_RATIO * 1.0,
    params.gravity = make_float3(0.0, -20.0, 0.0),

    params.world_floor = 0.0,
    params.v_co = make_float3(0, 0, 0),
    params.n_co = make_float3(0.0, 1.0, 0.0),
    params.mu_surface = 0.5,

    params.max_iterations_solver = 20,
    params.tolerance_solver = 1E-5f,

    params.max_iterations_newton = 20,
    params.max_iterations_line_search = 8,
    params.tolerance_newton = 1E-4f,
    params.line_search_constant = 1E-4f, // armijo constant
        params.line_search_shrink = 0.5, // alpha shrink

        params.beta_integration = 1.0,
    params.alpha_blend = 0.95f,
    params.mu_0 = params.initial_youngs_modulus / (static_cast<float>(2) * (static_cast<float>(1) + params.poisson_ratio));
    params.lambda_0 = (params.initial_youngs_modulus * params.poisson_ratio) / ((1.0f + params.poisson_ratio) * (static_cast<float>(1) - static_cast<float>(2) * params.poisson_ratio));
};

struct MpmSolver {
    MpmGrid grid = MpmGrid(vec3(0), 0, 0, 0, 0.080f);
    std::vector<int> global_to_active_map;

    // Particles
    MpmParticlesState p_current_state;
    std::mutex p_state_mutex;

    std::vector<std::array<float, 64>> p_weights;
    std::vector<std::array<vec3, 64>> p_weights_gradient;

    void create_particle(vec3 position, vec3 velocity);
    void initialize();
    void iterate();

    inline std::vector<vec3> get_positions() {
        std::vector<vec3> positions(p_current_state.p_position.size());

        {
            std::lock_guard<std::mutex> lock(p_state_mutex);
            std::memcpy(static_cast<void*>(positions.data()), p_current_state.p_position.data(),
                p_current_state.p_position.size() * sizeof(decltype(positions[0])));
        }

        return positions;
    }
};

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
    return vec3(grid.origin.x() + static_cast<float>(x) * grid.spacing,
        grid.origin.y() + static_cast<float>(y) * grid.spacing,
        grid.origin.z() + static_cast<float>(z) * grid.spacing);
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
            mat3n df(3, v_next.cols());
            df.setZero();

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
                // const mat3& Fp = solver.p_current_state.p_deform_plastic[i];
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
                    df.col(d.active_id) -= Ap * d.wip_grad;
                }
            }

            for (size_t i = 0; i < actives_nodes_size; ++i) {
                if (df == vec3::Zero()) continue;

                const auto index = solver.grid.active_nodes[i];
                const float& node_mass = solver.grid.nodes[index].mass;
                if (node_mass > EPSILON) {
                    vec3 df_res = static_params.beta_integration * simulation_dt * df.col(static_cast<long>(i)) / node_mass;
#pragma omp atomic
                    Av_next.col(static_cast<long>(i)).x() -= df_res.x();
#pragma omp atomic
                    Av_next.col(static_cast<long>(i)).y() -= df_res.y();
#pragma omp atomic
                    Av_next.col(static_cast<long>(i)).z() -= df_res.z();
                }
            }
        }
    }

    static void solveCR(MpmSolver const& solver, mat3n& xb) {
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
                if (R.determinant() < static_cast<float>(0)) {
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
                param.mu_2x = 2 * static_params.mu_0 * std::exp(static_params.hardening_coefficient * (static_cast<float>(1) - Jp));
                param.lambda = static_params.lambda_0 * std::exp(static_params.hardening_coefficient * (static_cast<float>(1) - Jp));
            }

            vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
            vec3i base_position(static_cast<int>(p_position_rel.x()) - 1, static_cast<int>(p_position_rel.y()) - 1, static_cast<int>(p_position_rel.z()) - 1);
            size_t count = 0;
            for (int z = 0; z < 4; ++z) {
                for (int y = 0; y < 4; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth)
                            continue;

                        size_t index = get_node_id_from_local(
                            solver.grid, base_position.x() + x, base_position.y() + y,
                            base_position.z() + z);
                        int active_id = solver.global_to_active_map[index];
                        if (active_id < 0) continue;

                        param.gradient[count].active_id = active_id;
                        param.gradient[count].wip_grad = solver.p_weights_gradient[i][static_cast<size_t>(x + y * 4 + z * 4 * 4)];
                        count += 1;
                    }
                }
            }
        }

        mat3n Ap(3, xb.cols());
        mat3n r(3, xb.cols());
        mat3n p(3, xb.cols());
        for (long i = 0; i < xb.cols(); i++) {
            Ap.col(i) = xb.col(i); // copy xb to Ap
        }

        calculate_Ar(solver, Ap, xb, w_ip_gradient);
        for (long i = 0; i < xb.cols(); i++) {
            p.col(i) = r.col(i) = xb.col(i) - Ap.col(i); // copy xb to Ap
        }

        calculate_Ar(solver, Ap, r, w_ip_gradient);

        float rAr_old = r.cwiseProduct(Ap).sum();
        const float b_norm = xb.norm();
        const float b_sn = b_norm < EPSILON ? static_cast<float>(1) : 1 / (b_norm * b_norm);
        const float t_sq = static_params.tolerance_solver * static_params.tolerance_solver;

        mat3n Ar;

        for (size_t k = 0; k < static_params.max_iterations_solver; ++k) {
            if (r.squaredNorm() * b_sn < t_sq) {
                break;
            }

            float alpha = rAr_old / Ap.squaredNorm();

            xb += alpha * p;
            r -= alpha * Ap;

            if (r.squaredNorm() * b_sn < t_sq) {
                break;
            }
            for (long i = 0; i < r.cols(); i++) {
                Ar.col(i) = r.col(i); // copy xb to Ap
            }

            calculate_Ar(solver, Ar, r, w_ip_gradient);
            float rAr_new = (r.cwiseProduct(Ar)).sum();
            float beta = rAr_new / rAr_old;

            p = r + beta * p;
            Ap = Ar + beta * Ap;
            rAr_old = rAr_new;
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

#pragma omp parallel num_threads(32)
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
        const auto mu = static_params.mu_0 * std::exp(static_params.hardening_coefficient * (static_cast<float>(1) - Jp));
        const auto lambda = static_params.lambda_0 * std::exp(static_params.hardening_coefficient * (static_cast<float>(1) - Jp));

        const auto Fe_invT = Fe.inverse().transpose();

        const float Je = Fe.determinant();
        const mat3 dPsi = static_cast<float>(2.0) * mu * (Fe - R) + lambda * (Je - static_cast<float>(1)) * Je * Fe_invT;
        const mat3 stress_force = solver.p_current_state.p_volume_0[i] * (dPsi * Fe.transpose());

        // find the closest bottom-left node to the current cell
        vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
        vec3i base_position(static_cast<int>(p_position_rel.x() - 1.f), static_cast<int>(p_position_rel.y() - 1.f), static_cast<int>(p_position_rel.z() - 1.f));

        // look at the neighbor 4x4 grid
        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    size_t weight_id = static_cast<size_t>(x + y * 4 + z * 4 * 4);
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth) {
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
        if (node.mass > static_cast<float>(0)) {

            vec3 velocity_star = (node.velocity = node.momentum / node.mass) + simulation_dt * (node.force + (node.mass * vec3(static_params.gravity.x, static_params.gravity.y, static_params.gravity.z))) / node.mass;

            // check for collision with the worlf floor
            if (solver.grid.origin.y() + static_cast<float>((index / static_cast<size_t>(solver.grid.width)) % static_cast<size_t>(solver.grid.height)) * solver.grid.spacing <= static_params.world_floor) {
                vec3 v_rel = velocity_star - vec3(static_params.v_co.x, static_params.v_co.y, static_params.v_co.z);
                float v_n = v_rel.dot(vec3(static_params.n_co.x, static_params.n_co.y, static_params.n_co.z));

                // if moving towards collider
                if (v_n < static_cast<float>(0.0)) {
                    vec3 v_t = v_rel - (v_n * vec3(static_params.n_co.x, static_params.n_co.y, static_params.n_co.z));
                    float v_t_norm = v_t.norm();

                    if (v_t_norm > static_params.mu_surface * v_n) {
                        velocity_star = vec3(static_params.v_co.x, static_params.v_co.y, static_params.v_co.z);
                    } else {
                        velocity_star = vec3(static_params.v_co.x, static_params.v_co.y, static_params.v_co.z) + v_t + static_params.mu_surface * v_n * (v_t / v_t_norm);
                    }
                }
            }
            node.velocity_star = velocity_star;

            solver.grid.active_nodes.push_back(static_cast<std::uint32_t>(index));
        }
    }
}

// First time step only - initial configuration
static void step2_compute_volumes_and_densities(MpmSolver& solver) {

#pragma omp parallel for
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
        vec3i base_position(static_cast<int>(p_position_rel.x() - 1.f), static_cast<int>(p_position_rel.y() - 1.f), static_cast<int>(p_position_rel.z() - 1.f));

        float rho_p = static_cast<float>(0.0);

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth)
                        continue;
                    size_t node_index = get_node_id_from_local(
                        solver.grid, base_position.x() + x, base_position.y() + y,
                        base_position.z() + z);
                    MpmGridNode const& node = solver.grid.nodes[node_index];

                    // rho_p = sum(w_ip * (m_i * / h^3))
                    const float& w_ip = solver.p_weights[i][static_cast<size_t>(x + y * 4 + z * 4 * 4)];
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
    if (static_params.beta_integration == static_cast<float>(0.0) || solver.grid.active_nodes.empty()) {
        return;
    }

    const auto& active_nodes = solver.grid.active_nodes;
    const size_t nb_active_nodes = active_nodes.size();
    mat3n b(3, nb_active_nodes);

    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto index = active_nodes[i];
        solver.global_to_active_map[index] = static_cast<int>(i);
        b.col(static_cast<long>(i)) = solver.grid.nodes[index].velocity_star;
    }

    Solver::solveCR(solver, b);

#pragma omp parallel
#pragma omp for schedule(static) nowait
    for (size_t i = 0; i < solver.grid.nodes.size(); ++i) {
        solver.global_to_active_map[i] = -1; // reset the map
    }

#pragma omp for nowait
    for (size_t i = 0; i < nb_active_nodes; ++i) {
        const auto& index = active_nodes[i];
        solver.grid.nodes[index].velocity_star = b.col(static_cast<long>(i));
    }
}

static void step7_update_deformation_gradient(MpmSolver& solver) {

#pragma omp parallel for
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
        vec3i base_position(static_cast<int>(p_position_rel.x() - 1.f), static_cast<int>(p_position_rel.y() - 1.f), static_cast<int>(p_position_rel.z() - 1.f));

        // 3.23 - velolity gradient
        mat3 velocities_grad = mat3::Zero();

        // look at the neighbor 4x4 grid
        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth)
                        continue;
                    const size_t index = get_node_id_from_local(
                        solver.grid, base_position.x() + x, base_position.y() + y,
                        base_position.z() + z);
                    const MpmGridNode& node = solver.grid.nodes[index];

                    const vec3& w_ip_grad = solver.p_weights_gradient[i][static_cast<size_t>(x + y * 4 + z * 4 * 4)];
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
                         .cwiseMin(1.f + static_params.critical_stretch)
                         .cwiseMax(1.f - static_params.critical_compression);

        solver.p_current_state.p_deform_plastic[i] = V * sigma.cwiseInverse().asDiagonal() * U.transpose() * (tmp_FE * tmp_FP);
        solver.p_current_state.p_deform_elastic[i] = U * sigma.asDiagonal() * V.transpose();
    }
}

static void step8_update_particle_velocities(MpmSolver& solver) {
#pragma omp parallel for
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        vec3 p_position_rel = (solver.p_current_state.p_position[i] - solver.grid.origin) * solver.grid.one_over_h;
        vec3i base_position(static_cast<int>(p_position_rel.x() - 1.f), static_cast<int>(p_position_rel.y() - 1.f), static_cast<int>(p_position_rel.z() - 1.f));

        vec3 v_pic = vec3::Zero();
        vec3 v_flip = vec3::Zero();

#if USE_APIC
        mat3 deform_affine = mat3::Zero();
        // p_current_state.p_deform_affine[i] = mat3::Zero();
#endif

        for (int z = 0; z < 4; ++z) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if ((base_position.x() + x) < 0 || (base_position.x() + x) >= solver.grid.width || (base_position.y() + y) < 0 || (base_position.y() + y) >= solver.grid.height || (base_position.z() + z) < 0 || (base_position.z() + z) >= solver.grid.depth)
                        continue;
                    const size_t index = get_node_id_from_local(
                        solver.grid, base_position.x() + x, base_position.y() + y,
                        base_position.z() + z);
                    const MpmGridNode& node = solver.grid.nodes[index];

                    vec3 node_pos = get_node_world_coords(solver.grid, base_position.x() + x,
                                        base_position.y() + y,
                                        base_position.z() + z)
                        - solver.p_current_state.p_position[i];
                    float const& w_ip = solver.p_weights[i][static_cast<size_t>(x + y * 4 + z * 4 * 4)];

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
        solver.p_current_state.p_velocity[i] = (static_cast<float>(1.0) - static_params.alpha_blend) * v_pic + static_params.alpha_blend * (v_flip + solver.p_current_state.p_velocity[i]);
    }
}

static void step9_particle_based_collisions(MpmSolver& solver) {
    for (size_t i = 0; i < solver.p_current_state.p_position.size(); ++i) {
        if (solver.p_current_state.p_position[i].y() + simulation_dt * solver.p_current_state.p_velocity[i].y() > static_params.world_floor) {
            continue;
        }

        // velocity relative to collider (ground)
        vec3 v_rel = solver.p_current_state.p_velocity[i] - vec3(static_params.v_co.x, static_params.v_co.y, static_params.v_co.z);
        float v_n = v_rel.dot(vec3(static_params.n_co.x, static_params.n_co.y, static_params.n_co.z));

        // if moving towards collider
        if (v_n < static_cast<float>(0.0)) {
            vec3 v_t = v_rel - (v_n * vec3(static_params.n_co.x, static_params.n_co.y, static_params.n_co.z));
            float v_t_norm = v_t.norm();

            if (v_t_norm > (static_params.mu_surface * v_n)) {
                solver.p_current_state.p_velocity[i] = vec3(static_params.v_co.x, static_params.v_co.y, static_params.v_co.z);
            } else {
                solver.p_current_state.p_velocity[i] = v_t + static_params.mu_surface * v_n * (v_t / v_t_norm) + vec3(static_params.v_co.x, static_params.v_co.y, static_params.v_co.z);
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

    reset_nodes(solver);

    step1_rasterize_particles_to_grid(solver);
    step6_solve_linear_system(solver);
    step7_update_deformation_gradient(solver);
    step8_update_particle_velocities(solver);
    step9_particle_based_collisions(solver);
    step10_update_particle_positions(solver);
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
    const float mass = static_params.initial_density * static_params.grid_spacing * static_params.grid_spacing * static_params.grid_spacing / static_params.particles_per_cell;
    create_particle_state(p_current_state, position, velocity, mass);
}
