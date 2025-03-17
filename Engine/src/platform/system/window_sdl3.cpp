#include "pch.h"
#include "platform/vulkan/vk_gpu_device.h"
#include "platform/system/window_sdl3.h"
#include "graphics/gpu_device.h"
#include "graphics/graphic_api.h"
#include "system/event.h"
#include "system/log.h"

#include <SDL3/SDL_init.h>

namespace Thsan
{
    bool WindowSDL3::init(const int width, const int height, const char* title)
    {
        bool success = false;

        this->width = width;
        this->height = height;
        this->title = title;

        windowAPI = WindowAPI::SDL3;
        if (get_graphic_API() == GraphicAPI::Vulkan)
        {
            SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
            sdlWindow = SDL_CreateWindow("Vulkan Engine", width, height, window_flags);
        }
        else
        {
            TS_ERROR("I don't think any graphicAPI was specified");
            return false;
        }

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            std::string error_msg = std::string(SDL_GetError());
            TS_ERROR("SDL_Init failed: {}", error_msg);
            return false;
        }

        DeviceCreation deviceCreationInfo;
        deviceCreationInfo.set_window(width, height, sdlWindow, windowAPI);

        gpu = GpuDevice::create();
        gpu->init(deviceCreationInfo);

        TS_INFO("SDL3 Window created: {}, (X: {}, Y: {})", title, width, height);
        return true;
    }

    void WindowSDL3::setTitle(const char* title)
    {
        this->title = title;
        SDL_SetWindowTitle(sdlWindow, title);
    }

    void WindowSDL3::setSize(const int width, const int height)
    {
        this->width = width;
        this->height = height;
        SDL_SetWindowSize(sdlWindow, width, height);
    }

    void WindowSDL3::close()
    {
        running = false;

        if (sdlRenderer)
            SDL_DestroyRenderer(sdlRenderer);

        if (sdlWindow)
        {
            gpu->shutdown();
        }
    }

    bool WindowSDL3::isRunning()
    {
        return running;
    }

    void WindowSDL3::swapBuffers() 
    {
        SDL_RenderPresent(sdlRenderer);
    }

    std::vector<Event> WindowSDL3::pollEvent()
    {
        std::vector<Thsan::Event> events;
        SDL_Event e;

        while (SDL_PollEvent(&e)) {
            if (isEventEnableForHUD)
                ; // ImGui_ImplSDL3_ProcessEvent(&e); // SDL3 equivalent if available

            Event tmp_event;

            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }

            if (e.type == SDL_EVENT_KEY_UP) 
            {
                tmp_event.key.code = static_cast<Key>(e.key.scancode);
                tmp_event.type = Event::KeyReleased;

            }
            else if (e.type == SDL_EVENT_KEY_DOWN) 
            {
                tmp_event.key.code = static_cast<Key>(e.key.scancode);
                tmp_event.type = Event::KeyPressed;
            }

            if (e.type == SDL_EVENT_WINDOW_RESIZED) 
            {
                int width = e.window.data1;
                int height = e.window.data2;
                if (resizeCallback) 
                {
                    resizeCallback(width, height);
                }
            }

            events.push_back(tmp_event);
        }

        return events;
    }

    void WindowSDL3::setResizeCallback(std::function<void(int, int)> callback) 
    {
        resizeCallback = callback;
    }

    std::shared_ptr<GpuDevice> WindowSDL3::getGpuDevice()
    {
        return gpu;
    }
}