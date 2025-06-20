#pragma once
#include "../../system/event.h"
#include "../../system/window.h"

#include <functional>

struct SDL_Window;
struct SDL_GPUDevice;

namespace GTS {
    class GpuDevice;

    class WindowSDL3 : public Window {
    public:
        WindowSDL3() = default;
        ~WindowSDL3() = default;

        bool init(const int width, const int height, const char* title) override;
        void setTitle(const char* title) override;
        void setSize(const int width, const int height) override;
        void close() override;
        bool isRunning() override;
        WindowHandle getWindow() override;
        std::vector<Event> pollEvent() override;
        void setResizeCallback(std::function<void(int, int)> callback) override;

        GpuHandle getGpuDevice() override;

    private:
        SDL_Window* sdlWindow { nullptr };
        SDL_GPUDevice* sdlGPU { nullptr };

        std::function<void(int, int)> resizeCallback { nullptr };
    };
}
