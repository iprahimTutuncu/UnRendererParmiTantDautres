#include "Window.hpp"

#include <stdexcept>
#include <format>

Window::Window(const char* title, int width, int height, bool vsync, bool windowed)
    : title{title}, m_width{width}, m_height{height}, m_vsync(vsync), m_windowed(windowed)
{
    try {
        init_sdl();
        init_opengl();
        m_active = true;
    }
    catch (std::exception e) {
        throw e;
    }
}

Window::~Window()
{
    SDL_GL_DeleteContext(_gl_context);
    _gl_context = nullptr;

    SDL_DestroyWindow(_window);
    _window = nullptr;

    SDL_Quit();
}

SDL_Window* Window::get_handle() const
{
    return _window;
}

SDL_GLContext Window::get_gl_context() const
{
    return _gl_context;
}

void Window::init_sdl()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
        throw std::runtime_error(std::format(
                    "ERROR: Failed to initialize SDL! SDL_Error: {}\n",
                    SDL_GetError()));
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);

    _window = SDL_CreateWindow(
            title,
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            m_width,
            m_height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    SDL_SetWindowResizable(_window, SDL_TRUE);
    if (_window == NULL) {
        throw std::runtime_error(std::format(
                    "ERROR: Failed to create SDL window! SDL_Error: {}\n",
                    SDL_GetError()));
    }

    _gl_context = SDL_GL_CreateContext(_window);
    if (_gl_context == NULL) {
        throw std::runtime_error(std::format(
                    "ERROR: Failed to create GL context! SDL_Error: {}\n",
                    SDL_GetError()));
    }

    SDL_GL_SetSwapInterval(m_vsync);

    glewExperimental = GL_TRUE;
    if (GLenum glewError = glewInit(); glewError != GLEW_OK) {
        throw std::runtime_error(std::format(
                    "ERROR: Failed to initialize GLEW! GLEW_Error: {}\n",
                    glewError));
    }
}

static void GLAPIENTRY message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ), type, severity, message);
}

void Window::init_opengl()
{
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(message_callback, 0);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_FALSE);
    glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, NULL, GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void Window::resize(int width, int height)
{
    m_width = width;
    m_height = height;
    glViewport(0, 0, m_width, m_height);
}

void Window::close()
{
    m_active = false;
}
