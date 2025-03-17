#pragma once

#include "platform/vulkan/vk_gpu_device.h"
#include "platform/system/window_sdl3.h"
#include "graphics/gpu_device.h"
#include "graphics/graphic_api.h"
#include "system/event.h"
#include "system/log.h"
#include "system/window.h"

#include <functional>
#include <SDL3/SDL_render.h>

namespace Thsan
{
    class WindowSDL3 : public Window
    {
    public:
        WindowSDL3() = default;
        ~WindowSDL3() = default;

        bool init(const int width, const int height, const char* title) override;
        void setTitle(const char* title) override;
        void setSize(const int width, const int height) override;
        void close() override;
        bool isRunning() override;
        void swapBuffers() override;
        std::vector<Event> pollEvent() override;
        void setResizeCallback(std::function<void(int, int)> callback) override;

        std::shared_ptr<GpuDevice> getGpuDevice() override;

    private:
        SDL_Window* sdlWindow{ nullptr };
        SDL_Renderer* sdlRenderer{ nullptr };
        std::shared_ptr<GpuDevice> gpu{ nullptr };
        std::function<void(int, int)> resizeCallback{ nullptr };
    };
}
