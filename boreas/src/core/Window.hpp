#pragma once

#include <GL/glew.h>
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <SDL.h>
#include <SDL_opengl>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#endif

#include <atomic>

class Window {

public:
    Window(const char* title, int width, int height, bool vsync);
    ~Window();

    static constexpr char const* glsl_version = "#version 460 core";

private:
    std::atomic<bool> m_active = true;
    SDL_Window* _window = nullptr;
    SDL_Cursor* _cursor = nullptr;
    SDL_GLContext _gl_context = nullptr;

public:
    inline std::atomic<bool> const& is_active() const {
        return m_active;
    };

    void resize(int width, int height);
    void close();

    inline SDL_Window* get_handle() const {
        return _window;
    }
    SDL_GLContext get_gl_context() const;

private:
    void init_sdl(const char* title, int width, int height, bool vsync);
    void init_opengl();
};
