#include "core/Application.hpp"
#include "libs/sdl.hpp"
#include "libs/eigen.hpp"
#include <iostream>
#include <thread>

const char* title = "Boreas";

const int width = 640;
const int height = 480;

// Explicit:            dt ~= 10e-5
// Semi-implicit:       dt ~= 0.5e-3
const double simulation_dt = 0.5e-3;

Application::Application() :
     m_action_man{},
     m_main_window{title, width, height}, 
     m_renderer{},
     m_mpm_solver()
{}

void Application::init() {
    init_keymap();

    m_mpm_solver.params.particles_per_cell = 2;
    m_mpm_solver.params.grid_spacing = 0.080;
    m_mpm_solver.params.grid_origin = vec3(-1.5, -1.50, -1.5);
    m_mpm_solver.params.grid_size = vec3(2.0, 5.0, 2.0);

    m_mpm_solver.params.critical_compression = 2.5E-2;
    m_mpm_solver.params.critical_stretch = 7.5E-3;
    m_mpm_solver.params.hardening_coefficient = 10.0;
    m_mpm_solver.params.initial_density = 4.0E2;
    m_mpm_solver.params.initial_youngs_modulus = 1.4E5;
    m_mpm_solver.params.poisson_ratio = 0.2;
    m_mpm_solver.params.gravity = vec3(0.0, -20.0, 0.0);

    m_mpm_solver.params.world_floor = 0.0;
    m_mpm_solver.params.v_co = vec3::Zero();
    m_mpm_solver.params.n_co = vec3(0.0, 1.0, 0.0);
    m_mpm_solver.params.mu_surface = 0.5;

    m_mpm_solver.params.max_iterations_solver = 30;
    m_mpm_solver.params.tolerance_solver = 1E-5;

    m_mpm_solver.params.max_iterations_newton = 20;
    m_mpm_solver.params.max_iterations_line_search = 8;
    m_mpm_solver.params.tolerance_newton = 1E-4;
    m_mpm_solver.params.line_search_constant = 1E-4;    // armijo constant
    m_mpm_solver.params.line_search_shrink = 0.5;      // alpha shrink
    m_mpm_solver.params.beta_integration = 1.0;

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
    vec3 velocity = vec3(0.0, -10.0, 0.0);
    vec3 origin = vec3(0.0, 1.0, 0.0);
    double radius = 0.10;
    int nb_particles = 500;
    unsigned int seed = 33;

    m_mpm_solver.create_particle_sphere_seeded(origin, radius, velocity, nb_particles, &seed);
}

void Application::init_keymap() {
    m_action_man.set_action(SDL_QUIT,
            [&]() { m_main_window.close(); });
    m_action_man.set_action(SDL_MOUSEWHEEL,
            [&]() { m_renderer.process_wheel(m_event.wheel.y); });
    m_action_man.set_action(SDL_MOUSEMOTION, 
            [&]() { if (m_capture_mouse) m_renderer.process_mouse(m_event.motion.xrel, m_event.motion.yrel); });

    m_action_man.set_action(SDL_WINDOWEVENT, 
            [&]() { m_action_man.do_action(m_event.window.event); });
    m_action_man.set_action(SDL_WINDOWEVENT_DISPLAY_CHANGED, 
            [&]() { resize(m_event.window.data1, m_event.window.data2); });
    m_action_man.set_action(SDL_WINDOWEVENT_RESIZED,
            [&]() { resize(m_event.window.data1, m_event.window.data2); });
    m_action_man.set_action(SDL_WINDOWEVENT_SIZE_CHANGED, 
            [&]() { resize(m_event.window.data1, m_event.window.data2); });

    m_action_man.set_action(SDL_KEYDOWN, 
            [&]() { m_action_man.do_action((int)m_event.key.keysym.sym); });
    m_action_man.set_action(SDLK_ESCAPE, 
            [&]() { m_main_window.close(); });
    m_action_man.set_action(SDLK_q, 
            [&]() { escape_mouse(); });
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
    double old_time, new_time, delta_time;

    unsigned int iteration_count = 0;
    double total_time = 0.0;

    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while (m_main_window.is_active()) {
        old_time = SDL_GetPerformanceCounter();
        m_mpm_solver.iterate(simulation_dt);
        new_time = SDL_GetPerformanceCounter();

        delta_time = (new_time - old_time) / frequency;
        old_time = new_time;
        total_time += delta_time;

//        std::cout << "Ieration " << iteration_count << " time: " << delta_time * 1000.0 << "ms" << std::endl;
        ++iteration_count;
    }

    double average_time = (total_time / iteration_count) * 1000.0;
    std::cout << "Average time: " <<  average_time << "ms" << std::endl;
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

