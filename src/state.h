#pragma once

#include <cstdint>

struct SDL_GPUDevice;
struct SDL_Window;
struct SDL_Thread;

struct CameraPerspective;
struct ControlState;
struct GraphicState;

struct AppState {
    SDL_GPUDevice *device;
    SDL_Window *window;

    CameraPerspective *camera;
    ControlState *controls;
    GraphicState *graphics;

    // some timing variables
    std::uint32_t lastTick, currentTick;
    std::uint64_t last;
    std::uint32_t numFrames;
    float delta_time;
};
