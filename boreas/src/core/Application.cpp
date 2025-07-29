#include "Application.hpp"

#include <glad/glad.h>

#include <Eigen/Dense>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <iostream>
#include <random>
#include <thread>

const char* title = "Boreas";

const int width = 640;
const int height = 480;

Application::Application()
    : m_action_man {}
    , m_main_window { title, width, height }
    , m_renderer {}
    , m_mpm_solver() { }

void Application::init() {
    init_keymap();

    init_scene();
    m_mpm_solver.initialize();

    int nb_particles = m_mpm_solver.p_current_state.p_position.size();
    std::cout << "INFO: Initialized simulation with " << nb_particles << " particles." << std::endl;

    if (!m_renderer.init(m_main_window.get_width(), m_main_window.get_height(), nb_particles)) {
        std::cerr << "ERROR: Failed to init renderer!" << std::endl;
        return;
    }
    std::cout << "INFO: Initialized renderer." << std::endl;
}

void Application::init_scene() {
    const Eigen::Vector3f velocity = Eigen::Vector3f(0.0, -5.0, 0.0);
    const Eigen::Vector3f origin = Eigen::Vector3f(0.0, 1.0, 0.0);
    constexpr double radius = 0.5;
    constexpr size_t nb_particles = 2048;
    constexpr std::uint64_t seed = 33;

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_real_distribution<double> dist6(-radius, radius);

    m_mpm_solver.p_current_state.ensure_capacity(nb_particles);

    generator.seed(seed);
    size_t particle_created = 0;
    do {
        double x = dist6(generator);
        double y = dist6(generator);
        double z = dist6(generator);
        Eigen::Vector3f relative_pos = Eigen::Vector3f(x, y, z);

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

        std::vector<Eigen::Vector3f> positions = m_mpm_solver.get_positions();
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
    double delays[MpmSolver::MAX_ITERATION] {};
    size_t i = 0;

    double total_delay = 0.0;

    auto last_time = clock::now();
    while (m_main_window.is_active()) {
        auto start_time = clock::now();

        m_mpm_solver.iterate();
        auto end_time = clock::now();
        this->iteration_count++;

        std::chrono::duration<double, std::milli> delay = end_time - start_time;
        delays[i++ % (sizeof(delays) / sizeof(delays[0]))] = delay.count();
        total_delay += delay.count();
        total_delay -= delays[i % (sizeof(delays) / sizeof(delays[0]))];

        if (i > sizeof(delays) / sizeof(delays[0])) [[unlikely]] {
            std::cout << "Iteration delay: " << delay.count() << " ms, "
                      << "Average: " << (total_delay / i) << " ms"
                      << " (count: " << i << ")\n\n"
                      << std::endl;
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
