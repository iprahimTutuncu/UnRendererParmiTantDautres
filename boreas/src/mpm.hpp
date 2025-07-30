#pragma once

#include <Eigen/Dense>
#include <Eigen/SVD>

#include <atomic>
#include <vector>

const float EPSILON = 1E-12;

// Explicit:            dt ~= 10e-5
// Semi-implicit:       dt ~= 0.5e-3
static inline constexpr float simulation_dt = 0.5e-3f;

#define HARDWARE_DESTRUCTIVE_INTERFERENCE_SIZE 64

struct alignas(HARDWARE_DESTRUCTIVE_INTERFERENCE_SIZE) MpmGridNode {
    std::atomic_flag atomic_flag;
    float mass { 0.0f }; // m
    Eigen::Vector3f momentum = Eigen::Vector3f::Zero(); // v*
    Eigen::Vector3f velocity_star = Eigen::Vector3f::Zero(); // v
    Eigen::Vector3f force = Eigen::Vector3f::Zero(); // F (stress)
    float one_over_mass { 0.0f }; // 1 / m

    MpmGridNode(const MpmGridNode&) = delete;
    MpmGridNode& operator=(const MpmGridNode&) = delete;
    MpmGridNode(MpmGridNode&&) = delete;
    MpmGridNode& operator=(MpmGridNode&&) = delete;
};

struct MpmParticlesState {
    std::vector<Eigen::Vector3f> p_position; // p
    std::vector<Eigen::Vector3f> p_velocity; // v
    std::vector<float> p_mass; // m
    std::vector<float> p_volume_0; // V
    std::vector<Eigen::Matrix3f> p_deform_elastic; // F_E
    std::vector<Eigen::Matrix3f> p_deform_plastic; // F_P
    std::vector<Eigen::Matrix3f> p_deform_affine; // B

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
    const float one_over_h; // 1 / h
    const float spacing; // h
    int width;
    int height;
    int depth;
    Eigen::Vector3f origin; // world space origin of the grid
    MpmGridNode* nodes;
    std::vector<std::uint32_t> active_nodes;

    ~MpmGrid() {
        std::free(nodes);
    }

    MpmGrid(Eigen::Vector3f origin, float size_x, float size_y, float size_z,
        float spacing)
        : one_over_h { 1 / spacing }
        , spacing { spacing }
        , width { static_cast<int>(std::ceil(size_x / spacing)) + 1 }
        , height { static_cast<int>(std::ceil(size_y / spacing)) + 1 }
        , depth { static_cast<int>(std::ceil(size_z / spacing)) + 1 }
        , origin { origin } {
        size_t nb_nodes = width * height * depth;
        nodes = static_cast<MpmGridNode*>(std::calloc(nb_nodes, sizeof(MpmGridNode)));
    }

    size_t size() const {
        return width * height * depth;
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
    const unsigned int particles_per_cell;
    const float grid_spacing;
    const float grid_origin[3];
    const float grid_size[3];

    const float critical_compression; // theta_c
    const float critical_stretch; // theta_s
    const float hardening_coefficient; // xi
    const float initial_density; // rho_0
    const float initial_youngs_modulus; // E_0
    const float poisson_ratio; // nu
    const float gravity[3]; // g

    const float world_floor;
    const float v_co[3]; // collider velocity
    const float n_co[3]; // collider normal
    const float mu_surface; // Coulomb friction coefficient

    const size_t max_iterations_solver;
    const float tolerance_solver;

    const int max_iterations_newton;
    const int max_iterations_line_search;
    const float tolerance_newton;
    const float line_search_constant; // armijo constant
    const float line_search_shrink; // alpha shrink

    const float beta_integration; // 0 for explicit, 1/2 for trapezoidal, 1 for
                                  // backward euler
    const float alpha_blend; // PIC/FLIP blend
};

static constexpr MpmSolverParams params {
    .particles_per_cell = 32,
    .grid_spacing = 0.080,
    .grid_origin = { -2.5, 0.0, -2.5 },
    .grid_size = { 5.0, 3.0, 5.0 },

    .critical_compression = 2.5e-2f,
    .critical_stretch = 7.5e-3f,
    .hardening_coefficient = 10.0f * 1.0f,
    .initial_density = 4.0e2f,
    .initial_youngs_modulus = 1.4e5f * 1.0,
    .poisson_ratio = 0.2f * 1.0,
    .gravity = { 0.0, -20.0, 0.0 },

    .world_floor = 0.0,
    .v_co = { 0, 0, 0 },
    .n_co = { 0.0, 1.0, 0.0 },
    .mu_surface = 0.5,

    .max_iterations_solver = 20,
    .tolerance_solver = 1E-5,

    .max_iterations_newton = 20,
    .max_iterations_line_search = 8,
    .tolerance_newton = 1E-4,
    .line_search_constant = 1E-4, // armijo constant
    .line_search_shrink = 0.5, // alpha shrink

    .beta_integration = 1.0,
    .alpha_blend = 0.95,
};

static constexpr float mu_0 = params.initial_youngs_modulus / (static_cast<float>(2) * (static_cast<float>(1) + params.poisson_ratio));
static constexpr float lambda_0 = (params.initial_youngs_modulus * params.poisson_ratio) / ((1.0 + params.poisson_ratio) * (static_cast<float>(1) - static_cast<float>(2) * params.poisson_ratio));

struct alignas(16) solveCR_params {
    Eigen::Vector3f wip_grad;
    int active_id;
};
struct params_car {
    float svd_det_invt;
    float Fe_det;
    float mu_2x;
    float lambda;
    float volume;
    Eigen::Matrix3f Fe_inverse;
    Eigen::Matrix3f R;
    Eigen::Matrix3f U;
    Eigen::Matrix3f V;
    Eigen::Matrix3f A_inverse;
    std::array<solveCR_params, 64> gradient;
};

struct MpmSolver {
    MpmGrid grid = MpmGrid(Eigen::Vector3f(params.grid_origin[0], params.grid_origin[1],
                               params.grid_origin[2]),
        params.grid_size[0], params.grid_size[1],
        params.grid_size[2], params.grid_spacing);
    size_t min_index = 0;
    size_t max_index = 0;
    size_t nb_particles = 0;
    std::vector<int> global_to_active_map;
    params_car* ar_params;

    ~MpmSolver() {
        std::free(ar_params);
    }

    // Particles
    MpmParticlesState p_current_state;

    std::vector<std::array<float, 64>> p_weights;
    std::vector<std::array<Eigen::Vector3f, 64>> p_weights_gradient;

    void create_particle(Eigen::Vector3f position, Eigen::Vector3f velocity);
    void initialize();
    void iterate();

    inline std::vector<Eigen::Vector3f> get_positions() {
        std::vector<Eigen::Vector3f> positions(p_current_state.p_position.size());

        std::memcpy(static_cast<void*>(positions.data()), p_current_state.p_position.data(),
            p_current_state.p_position.size() * sizeof(decltype(positions[0])));

        return positions;
    }

    // DEBUG ONLY
    static constexpr size_t MAX_ITERATION = 1024;
};
