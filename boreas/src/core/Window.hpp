#pragma once

#include "../libs/sdl.hpp"
#include <atomic>

class Window {

public:
    Window(const char* title, int width, int height, bool vsync = true, bool windowed = true);
    ~Window();

    const char* glsl_version = "#version 460 core";
    const char* title;

private:
    SDL_Window* _window = nullptr;
    SDL_Cursor* _cursor = nullptr;
    SDL_GLContext _gl_context = nullptr;

    int m_width;
    int m_height;

    bool m_active = true;
    bool m_vsync = false;
    bool m_windowed = true;

public:
    int get_width() const {
        return m_width;
    };
    int get_height() const {
        return m_height;
    };
    std::atomic<bool> is_active() const {
        return m_active;
    };

    void resize(int width, int height);
    void close();

    SDL_Window* get_handle() const;
    SDL_GLContext get_gl_context() const;

private:
    void init_sdl();
    void init_opengl();
};
