#include "state.h"

#include "camera.h"
#include "controls/controls.h"
#include "graphics/graphics.h"
#include "physics/physics.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>
#include <UTL/profiler.hpp>

#include <cstdint>

static inline void setFPSinTitle(std::uint32_t i, char *title) {
    if (i > 999) i = 999;
    title[0] = static_cast<char>(i / 100 ? i / 100 + '0' : ' ');
    title[1] = static_cast<char>(i / 10 % 10 ? i / 10 % 10 + '0' : ' ');
    title[2] = static_cast<char>(i % 10 + '0');
}

static void updateTiming(AppState &state) {
    state.numFrames++;

    std::uint64_t performanceCounter = SDL_GetPerformanceCounter();
    state.deltaTime = static_cast<float>(performanceCounter - state.lastPerformanceCounter) / static_cast<float>(SDL_GetPerformanceFrequency());
    state.lastPerformanceCounter = performanceCounter;
    if (SDL_GetTicks() - state.lastTick >= 1000ull) [[unlikely]] {
        static char title[] = "Running at XXX fps.";
        constexpr int indexFirstX = 11;
        setFPSinTitle(state.numFrames, title + indexFirstX);
        SDL_SetWindowTitle(state.window, title);
        state.lastTick = static_cast<std::uint32_t>(SDL_GetTicks());
        state.numFrames = 0u;
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    UTL_PROFILER("SDL_AppInit") {
        if (!SDL_SetAppMetadata("Olaf engine renderer", "0.1.1", "ca.etsmtl.olaf")) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set app metadata: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        UTL_PROFILER("SDL_InitSubSystem")
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialise SDL: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        // enabling debug mode on linux crash the app at SDL_CreateGPUGraphicsPipeline
        // desabling it for nowUTL_PROFILER("SDL_CreateGPUDevice")
        SDL_GPUDevice *device;
        UTL_PROFILER("SDL_CreateGPUDevice")
        device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
        if (device == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create SDL GPU Device: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        SDL_Window *window;
        UTL_PROFILER("SDL_CreateWindow")
        window = SDL_CreateWindow(nullptr, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
        if (window == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create SDL Window: %s", SDL_GetError());
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
        UTL_PROFILER("SDL_ClaimWindowForGPUDevice")
        if (!SDL_ClaimWindowForGPUDevice(device, window)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to claim SDL_Window for GPU Device: %s", SDL_GetError());
            SDL_DestroyGPUDevice(device);
            SDL_DestroyWindow(window);
            return SDL_APP_FAILURE;
        }

        *appstate = new AppState {};
        AppState &state = *static_cast<AppState *>(*appstate);
        state.device = device;
        state.window = window;
        state.lastTick = static_cast<std::uint32_t>(SDL_GetTicks());
        state.lastPerformanceCounter = SDL_GetPerformanceCounter();
        state.numFrames = 0u;
        state.deltaTime = 0.f;

        static const vec3 position { 0, 10, 20 };
        static const vec3 worldUp { 0.f, 1.f, 0.f };
        const vec3 front = normalize(position);
        const vec3 right = normalize(cross(worldUp, front));
        const vec3 up = cross(front, right);
        state.camera = new CameraPerspective {
            .front = front,
            .right = right,
            .up = up,
            .position = position,
            .aspectRatio = static_cast<float>(INITIAL_WINDOW_WIDTH) / static_cast<float>(INITIAL_WINDOW_HEIGHT),
            .fov = radians(75.f),
            .near = .1f,
            .far = 100.0f,
        };

        SDL_AppResult result;
        UTL_PROFILER("graphics_init")
        result = graphics_init(state, argc, argv);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;

        UTL_PROFILER("physics_init")
        result = physics_init(state, argc, argv);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;

        UTL_PROFILER("controls_init")
        result = controls_init(state, argc, argv);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    UTL_PROFILER("SDL_AppIterate") {
        AppState &app = *static_cast<AppState *>(appstate);
        UTL_PROFILER("updateTiming")
        updateTiming(app);

        SDL_AppResult result;
        UTL_PROFILER("controls_iterate")
        result = controls_iterate(app);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
        UTL_PROFILER("physics_iterate")
        result = physics_iterate(app);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
        UTL_PROFILER("graphics_iterate")
        result = graphics_iterate(app);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState &app = *static_cast<AppState *>(appstate);

    UTL_PROFILER("SDL_AppEvent") {
        switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_WINDOW_RESIZED: {
            int &width = event->window.data1;
            int &height = event->window.data2;
            SDL_SetWindowSize(app.window, width, height);
        } break;
        default:
            break;
        }

        SDL_AppResult result;
        UTL_PROFILER("controls_event")
        result = controls_event(app, *event);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
        UTL_PROFILER("physics_event")
        result = physics_event(app, *event);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
        UTL_PROFILER("graphics_event")
        result = graphics_event(app, *event);
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result;
    UTL_PROFILER("SDL_AppQuit")
    if (appstate != nullptr) [[likely]] {
        AppState &app = *static_cast<AppState *>(appstate);

        UTL_PROFILER("physics_quit")
        physics_quit(app);

        UTL_PROFILER("controls_quit")
        controls_quit(app);
        UTL_PROFILER("graphics_quit")
        graphics_quit(app);

        UTL_PROFILER("SDL_ReleaseWindowFromGPUDevice")
        if (app.device && app.window)
            SDL_ReleaseWindowFromGPUDevice(app.device, app.window);
        UTL_PROFILER("SDL_DestroyWindow")
        if (app.window)
            SDL_DestroyWindow(app.window);
        UTL_PROFILER("SDL_DestroyGPUDevice")
        if (app.device)
            SDL_DestroyGPUDevice(app.device);

        delete app.camera;
        delete &app;
    }
}
