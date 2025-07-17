#include "Application.hpp"

#include "../eigen.hpp"

#include <iostream>
#include <random>
#include <thread>

const char* title = "Boreas";

constexpr int width = 640;
constexpr int height = 480;

// Explicit:            dt ~= 10e-5
// Semi-implicit:       dt ~= 0.5e-3
constexpr double simulation_dt = 0.5e-3;

constexpr double DEFAULT_COMPRESSION = 2.5e-2;
constexpr double DEFAULT_STRETCH = 7.5e-3;
constexpr double DEFAULT_HARDENING = 10.0;
constexpr double DEFAULT_DENSITY = 4.0e2;
constexpr double DEFAULT_YOUNGS_MODULUS = 1.4e5;
constexpr double DEFAULT_POISSON_RATIO = 0.2;

Application::Application()
    : m_action_man {}
    , m_main_window(title, width, height, true)
    , m_renderer {}
    , m_mpm_solver() { }

void Application::init() {
    init_keymap();

    MpmSolverParams params {};

    params.particles_per_cell = 32;
    params.grid_spacing = 0.080;
    params.grid_origin = vec3(-2.5, 0.0, -2.5);
    params.grid_size = vec3(5.0, 3.0, 5.0);

    params.critical_compression = DEFAULT_COMPRESSION;
    params.critical_stretch = DEFAULT_STRETCH;
    params.hardening_coefficient = DEFAULT_HARDENING * 1.0;
    params.initial_density = DEFAULT_DENSITY;
    params.initial_youngs_modulus = DEFAULT_YOUNGS_MODULUS * 1.0;
    params.poisson_ratio = DEFAULT_POISSON_RATIO * 1.0;
    params.gravity = vec3(0.0, -20.0, 0.0);

    params.world_floor = 0.0;
    params.v_co = vec3::Zero();
    params.n_co = vec3(0.0, 1.0, 0.0);
    params.mu_surface = 0.5;

    params.max_iterations_solver = 20;
    params.tolerance_solver = 1E-5;

    params.max_iterations_newton = 20;
    params.max_iterations_line_search = 8;
    params.tolerance_newton = 1E-4;
    params.line_search_constant = 1E-4; // armijo constant
    params.line_search_shrink = 0.5; // alpha shrink

    params.beta_integration = 1.0;
    params.alpha_blend = 0.95;

    init_scene();
    m_mpm_solver.initialize(params);

    std::size_t nb_particles = m_mpm_solver.p_current_state->p_position.size();
    std::cout << "INFO: Initialized simulation with " << nb_particles << " particles." << std::endl;

    if (!m_renderer.init(width, height, nb_particles)) {
        std::cerr << "ERROR: Failed to init renderer!" << std::endl;
        return;
    }
    std::cout << "INFO: Initialized renderer." << std::endl;
}

void Application::init_scene() {
    const vec3 velocity = vec3(0.0, -5.0, 0.0);
    const vec3 origin = vec3(0.0, 1.0, 0.0);
    constexpr double radius = 0.5;
    constexpr size_t nb_particles = 2000;
    constexpr std::uint64_t seed = 33ull;

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_real_distribution<double> dist6(-radius, radius);

    generator.seed(seed);
    size_t particle_created = 0;
    do {
        double x = dist6(generator);
        double y = dist6(generator);
        double z = dist6(generator);
        vec3 relative_pos = vec3(x, y, z);

        if (relative_pos.squaredNorm() <= radius * radius) {
            m_mpm_solver.create_particle(origin + relative_pos, velocity);
            ++particle_created;
        }
    } while (particle_created < nb_particles);
}

void Application::init_keymap() {
    m_action_man.set_action(SDL_QUIT,
        [&]() {
            m_main_window.close();
        });
    m_action_man.set_action(SDL_MOUSEWHEEL,
        [&]() {
            m_renderer.process_wheel(m_event.wheel.y);
        });
    m_action_man.set_action(SDL_MOUSEMOTION,
        [&]() {
            if (m_capture_mouse) m_renderer.process_mouse(m_event.motion.xrel, m_event.motion.yrel);
        });

    m_action_man.set_action(SDL_WINDOWEVENT,
        [&]() {
            m_action_man.do_action(m_event.window.event);
        });
    m_action_man.set_action(SDL_WINDOWEVENT_DISPLAY_CHANGED,
        [&]() {
            resize(m_event.window.data1, m_event.window.data2);
        });
    m_action_man.set_action(SDL_WINDOWEVENT_RESIZED,
        [&]() {
            resize(m_event.window.data1, m_event.window.data2);
        });
    m_action_man.set_action(SDL_WINDOWEVENT_SIZE_CHANGED,
        [&]() {
            resize(m_event.window.data1, m_event.window.data2);
        });

    m_action_man.set_action(SDL_KEYDOWN,
        [&]() {
            m_action_man.do_action((int)m_event.key.keysym.sym);
        });
    m_action_man.set_action(SDLK_ESCAPE,
        [&]() {
            m_main_window.close();
        });
    m_action_man.set_action(SDLK_q,
        [&]() {
            escape_mouse();
        });
}

static inline void setFPSinTitle(std::uint32_t i, char* title) {
    if (i > 999) i = 999;
    title[0] = static_cast<char>(i / 100 ? i / 100 + '0' : ' ');
    title[1] = static_cast<char>(i / 10 % 10 ? i / 10 % 10 + '0' : ' ');
    title[2] = static_cast<char>(i % 10 + '0');
}

void Application::run() {
    std::thread simulation(&Application::iterate_particles, this);

    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    double old_time = SDL_GetPerformanceCounter();

    std::uint_fast16_t numFrames = 0;
    std::uint32_t currentTick = SDL_GetTicks();

    while (m_main_window.is_active().load()) {
        numFrames += 1;
        if (SDL_GetTicks() - currentTick >= 1000ull) [[unlikely]] {
            currentTick = SDL_GetTicks();
            static char title[] = "Running at XXX fps - XXX iter/s - XXX ms/iter.";
            constexpr int indexFirstX = 11;
            constexpr int indexSecondX = 21;
            constexpr int indexThirdX = 34;
            setFPSinTitle(numFrames, title + indexFirstX);

            std::uint32_t iteration_count = this->iteration_count;
            this->iteration_count = 0;

            setFPSinTitle(iteration_count, title + indexSecondX);
            if (iteration_count > 0) iteration_count = 1000u / iteration_count;
            setFPSinTitle(iteration_count, title + indexThirdX);
            SDL_SetWindowTitle(m_main_window.get_handle(), title);
            numFrames = 0;
        }
        double new_time = SDL_GetPerformanceCounter();
        double delta_time = (new_time - old_time) / frequency;
        old_time = new_time;

        while (SDL_PollEvent(&m_event)) {
            m_action_man.do_action(m_event.type);
        }

        m_renderer.process_input(delta_time);
        m_renderer.clear();
        m_renderer.render_scene();
        m_renderer.render_particles();

        SDL_GL_SwapWindow(m_main_window.get_handle());

        std::vector<vec3> positions = m_mpm_solver.get_positions();
        m_renderer.update_particles(positions);
    }

    simulation.join();
}

void Application::iterate_particles() {
    using clock = std::chrono::high_resolution_clock;
    std::vector<double> delays;
    delays.reserve(16); // Reserve space for efficiency

    double total_delay = 0.0;
    size_t count = 0;

    auto last_time = clock::now();
    while (m_main_window.is_active()) {
        auto start_time = clock::now();

        m_mpm_solver.iterate(simulation_dt);
        auto end_time = clock::now();
        this->iteration_count++;

        m_mpm_solver.swap_buffers();
        std::chrono::duration<double, std::milli> delay = end_time - start_time;
        delays.push_back(delay.count());
        total_delay += delay.count();
        count++;

        std::cout << "Iteration delay: " << delay.count() << " ms, "
        << "Average: " << (total_delay / count) << " ms" << std::endl;
    }
}

void Application::resize(int width, int height) {
    m_main_window.resize(width, height);
    m_renderer.resize(width, height);
}

void Application::escape_mouse() {
    m_capture_mouse = !m_capture_mouse;
    SDL_CaptureMouse((SDL_bool)m_capture_mouse);
    SDL_SetRelativeMouseMode((SDL_bool)m_capture_mouse);
}
