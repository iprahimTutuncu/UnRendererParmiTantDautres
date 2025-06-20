#include "state.h"

#include "camera.h"
#include "controls/controls.h"
#include "graphics/api.h"
#include "physics/api.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include <cstdint>

static inline void setFPSinTitle(std::uint32_t i, char *title) {
    if (i > 999) i = 999;
    title[0] = static_cast<char>(i / 100 ? i / 100 + '0' : ' ');
    title[1] = static_cast<char>(i / 10 % 10 ? i / 10 % 10 + '0' : ' ');
    title[2] = static_cast<char>(i % 10 + '0');
}

static void updateTiming(AppState &state) {
    std::uint64_t now = SDL_GetPerformanceCounter();

    state.numFrames++;
    state.delta_time = (now - state.last) / (double)SDL_GetPerformanceFrequency();
    state.last = now;
    std::uint32_t timeDelta = (state.currentTick = SDL_GetTicks()) - state.lastTick;
    if (timeDelta >= 1000ull) [[unlikely]] {
        static char title[] = "Running at XXX fps.";
        constexpr int indexFirstX = 11;
        setFPSinTitle(state.numFrames, title + indexFirstX);
        SDL_SetWindowTitle(state.window, title);
        state.lastTick = state.currentTick;
        state.numFrames = 0u;
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    if (!SDL_SetAppMetadata("Olaf engine renderer", "0.1.1", "ca.etsmtl.olaf")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set app metadata: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialise SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
    if (device == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create SDL GPU Device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow("Olaf Engine", 1280, 768, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create SDL Window: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to claim SDL_Window for GPU Device: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        return SDL_APP_FAILURE;
    }

    *appstate = new AppState;
    AppState &state = *static_cast<AppState *>(*appstate);
    state.device = device;
    state.window = window;
    state.lastTick = 0ull;
    state.currentTick = 0ull;
    state.numFrames = 0u;
    state.delta_time = 0.f;

    state.camera = new CameraPerspective {};
    CameraPerspective &camera = *state.camera;
    camera.near = 0.1f;
    camera.far = 100.0f;
    camera.fov = radians(90.0f);

    SDL_AppResult result;
    result = physics_init(state, argc, argv);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;
    result = graphics_init(state, argc, argv);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;
    result = controls_init(state, argc, argv);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState &app = *static_cast<AppState *>(appstate);
    updateTiming(app);

    SDL_AppResult result;
    result = controls_iterate(app);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    result = graphics_iterate(app);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    result = physics_iterate(app);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState &app = *static_cast<AppState *>(appstate);

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
    result = controls_event(app, *event);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;
    result = physics_event(app, *event);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;
    result = graphics_event(app, *event);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result;

    if (appstate != nullptr) [[likely]] {
        AppState &app = *static_cast<AppState *>(appstate);
        controls_quit(app);
        physics_quit(app);
        graphics_quit(app);

        if (app.device && app.window)
            SDL_ReleaseWindowFromGPUDevice(app.device, app.window);
        if (app.window)
            SDL_DestroyWindow(app.window);
        if (app.device)
            SDL_DestroyGPUDevice(app.device);

        delete app.camera;
        delete &app;
    }
}
