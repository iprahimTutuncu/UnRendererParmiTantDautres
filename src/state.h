#pragma once

#include <cstdint>

#define INITIAL_WINDOW_WIDTH 1280u
#define INITIAL_WINDOW_HEIGHT 768u

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
    std::uint64_t lastPerformanceCounter;
    std::uint32_t lastTick;
    std::uint32_t numFrames;
    float deltaTime;
};
