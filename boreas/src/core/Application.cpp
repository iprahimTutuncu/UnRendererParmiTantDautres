#include "Application.hpp"
#include "../libs/eigen.hpp"
#include "../libs/sdl.hpp"
#include <iostream>
#include <thread>
#include <random>

const char* title = "Boreas";

const int width = 640;
const int height = 480;

// Explicit:            dt ~= 10e-5
// Semi-implicit:       dt ~= 0.5e-3
const double simulation_dt = 0.5e-3;

const double DEFAULT_COMPRESSION = 2.5e-2;
const double DEFAULT_STRETCH = 7.5e-3;
const double DEFAULT_HARDENING = 10.0;
const double DEFAULT_DENSITY = 4.0e2;
const double DEFAULT_YOUNGS_MODULUS = 1.4e5;
const double DEFAULT_POISSON_RATIO = 0.2;

Application::Application()
    : m_action_man {}
    , m_main_window { title, width, height }
    , m_renderer {}
    , m_mpm_solver() { }

void Application::init() {
    init_keymap();

    m_mpm_solver.params.particles_per_cell = 32;
    m_mpm_solver.params.grid_spacing = 0.080;
    m_mpm_solver.params.grid_origin = vec3(-2.5, 0.0, -2.5);
    m_mpm_solver.params.grid_size = vec3(5.0, 3.0, 5.0);

    m_mpm_solver.params.critical_compression = DEFAULT_COMPRESSION;
    m_mpm_solver.params.critical_stretch = DEFAULT_STRETCH;
    m_mpm_solver.params.hardening_coefficient = DEFAULT_HARDENING * 1.0;
    m_mpm_solver.params.initial_density = DEFAULT_DENSITY;
    m_mpm_solver.params.initial_youngs_modulus = DEFAULT_YOUNGS_MODULUS * 1.0;
    m_mpm_solver.params.poisson_ratio = DEFAULT_POISSON_RATIO * 1.0;
    m_mpm_solver.params.gravity = vec3(0.0, -20.0, 0.0);

    m_mpm_solver.params.world_floor = 0.0;
    m_mpm_solver.params.v_co = vec3::Zero();
    m_mpm_solver.params.n_co = vec3(0.0, 1.0, 0.0);
    m_mpm_solver.params.mu_surface = 0.5;

    m_mpm_solver.params.max_iterations_solver = 20;
    m_mpm_solver.params.tolerance_solver = 1E-5;

    m_mpm_solver.params.max_iterations_newton = 20;
    m_mpm_solver.params.max_iterations_line_search = 8;
    m_mpm_solver.params.tolerance_newton = 1E-4;
    m_mpm_solver.params.line_search_constant = 1E-4; // armijo constant
    m_mpm_solver.params.line_search_shrink = 0.5; // alpha shrink

    m_mpm_solver.params.beta_integration = 1.0;
    m_mpm_solver.params.alpha_blend = 0.95;

    init_scene();
    m_mpm_solver.initialize();

    int nb_particles = m_mpm_solver.p_current_state->p_position.size();
    std::cout << "INFO: Initialized simulation with " << nb_particles << " particles." << std::endl;

    if (!m_renderer.init(m_main_window.get_width(), m_main_window.get_height(), nb_particles)) {
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
    constexpr std::uint64_t seed = 33;

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

void Application::run() {
    std::thread simulation(&Application::iterate_particles, this);

    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    double old_time = SDL_GetPerformanceCounter();

    while (m_main_window.is_active()) {
        double new_time = SDL_GetPerformanceCounter();
        double delta_time = (new_time - old_time) / frequency;
        old_time = new_time;

        process_events();

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

void Application::process_events() {
    while (SDL_PollEvent(&m_event)) {
        m_action_man.do_action(m_event.type);
    }
}

void Application::iterate_particles() {
    using clock = std::chrono::high_resolution_clock;
    double delays[512] {};
    size_t i = 0;

    double total_delay = 0.0;

    auto last_time = clock::now();
    while (m_main_window.is_active()) {
        auto start_time = clock::now();

        m_mpm_solver.iterate(simulation_dt);
        auto end_time = clock::now();
        this->iteration_count++;

        std::chrono::duration<double, std::milli> delay = end_time - start_time;
        delays[i++ % (sizeof(delays) / sizeof(delays[0]))] = delay.count();
        total_delay += delay.count();
        total_delay -= delays[i % (sizeof(delays) / sizeof(delays[0]))];

        std::cout << "Iteration delay: " << delay.count() << " ms, "
                  << "Average: " << (total_delay / i) << " ms"
                  << " (count: " << i << ")\n\n"
                  << std::endl;
        if (i > sizeof(delays) / sizeof(delays[0])) [[unlikely]] {
            m_main_window.close();
            std::cout << "INFO: Simulation finished after " << this->iteration_count - 1 << " iterations." << std::endl;
            return;
        }
    }
}

void Application::resize(int width, int height) {
    m_main_window.resize(width, height);
    m_renderer.resize(m_main_window.get_width(), m_main_window.get_height());
}

void Application::escape_mouse() {
    m_capture_mouse = !m_capture_mouse;
    SDL_CaptureMouse((SDL_bool)m_capture_mouse);
    SDL_SetRelativeMouseMode((SDL_bool)m_capture_mouse);
}
