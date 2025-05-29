#include <olaf/platform/system/window_sdl3.h>

#include <olaf/graphics/graphic_api.h>
#include <olaf/system/event.h>
#include <olaf/system/log.h>

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>

namespace Olaf {
    bool WindowSDL3::init(const int width, const int height, const char* title) {
        this->width = width;
        this->height = height;
        this->title = title;

        windowAPI = WindowAPI::SDL3;

        if (get_graphic_API() == GraphicAPI::SDL3) {
            if (!SDL_Init(SDL_INIT_VIDEO)) {
                OLAF_ERROR("SDL_Init failed: {}", SDL_GetError());
                return false;
            }

            sdlGPU = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");

#ifdef DEBUG
            const char* drivers = SDL_GetGPUDeviceDriver(sdlGPU);
            OLAF_INFO("Available GPU driver: {}", drivers);
#endif

            if (!sdlGPU) {
                OLAF_ERROR("Failed to create SDL GPU Device: {}", SDL_GetError());
                return false;
            }

            sdlWindow = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
            if (!sdlWindow) {
                OLAF_ERROR("Failed to create SDL Window: {}", SDL_GetError());
                SDL_DestroyGPUDevice(sdlGPU);
                return false;
            }

            if (!SDL_ClaimWindowForGPUDevice(sdlGPU, sdlWindow)) {
                OLAF_ERROR("Failed to claim SDL_Window for GPU Device: {}", SDL_GetError());
                SDL_DestroyGPUDevice(sdlGPU);
                SDL_DestroyWindow(sdlWindow);
                return false;
            }

            OLAF_INFO("SDL3 GPU Window created using Vulkan backend.");
        } else {
            OLAF_ERROR("I don't think any graphicAPI was specified");
            return false;
        }

        OLAF_INFO("SDL3 Window created: {}, (X: {}, Y: {})", title, width, height);
        return true;
    }

    void WindowSDL3::setTitle(const char* title) {
        this->title = title;
        SDL_SetWindowTitle(sdlWindow, title);
    }

    void WindowSDL3::setSize(const int width, const int height) {
        this->width = width;
        this->height = height;
        SDL_SetWindowSize(sdlWindow, width, height);
    }

    void WindowSDL3::close() {
        running = false;

        if (sdlWindow && sdlGPU) {
            SDL_ReleaseWindowFromGPUDevice(sdlGPU, sdlWindow);
            SDL_DestroyWindow(sdlWindow);
            SDL_DestroyGPUDevice(sdlGPU);
        }
    }

    bool WindowSDL3::isRunning() {
        return running;
    }

    WindowHandle WindowSDL3::getWindow() {
        if (windowAPI == WindowAPI::SDL3) {
            return WindowHandle { sdlWindow };
        }
        return WindowHandle();
    }

    std::vector<Event> WindowSDL3::pollEvent() {
        std::vector<Olaf::Event> events;
        SDL_Event e;

        while (SDL_PollEvent(&e)) {

            Event tmp_event;

            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }

            if (e.type == SDL_EVENT_KEY_UP) {
                tmp_event.key.code = static_cast<Key>(e.key.scancode);
                tmp_event.type = Event::KeyReleased;

            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                tmp_event.key.code = static_cast<Key>(e.key.scancode);
                tmp_event.type = Event::KeyPressed;
            }

            if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                int width = e.window.data1;
                int height = e.window.data2;
                if (resizeCallback) {
                    resizeCallback(width, height);
                }
            }

            events.push_back(tmp_event);
        }

        return events;
    }

    void WindowSDL3::setResizeCallback(std::function<void(int, int)> callback) {
        resizeCallback = callback;
    }

    GpuHandle WindowSDL3::getGpuDevice() {
        if (get_graphic_API() == GraphicAPI::SDL3) {
            return GpuHandle { sdlGPU };
        }

        return GpuHandle {};
    }

}
