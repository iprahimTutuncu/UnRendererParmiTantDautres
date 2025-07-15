#pragma once

#include "../gfx/Renderer.hpp"
#include "../mpm/MpmSolver.hpp"
#include "ActionManager.hpp"
#include "Window.hpp"

class Application {
public:
    Application();
    ~Application() = default;

    void run();
    void init();

private:
    void init_keymap();
    void init_scene();
    void process_events();
    void iterate_particles();
    void resize(int width, int height);
    void escape_mouse();

private:
    Window m_main_window;
    Renderer m_renderer;
    MpmSolver m_mpm_solver;
    ActionManager m_action_man;
    SDL_Event m_event;
    std::atomic_uint32_t iteration_count = 0;
    bool m_capture_mouse = true;
};
