#pragma once

#include <cstdint>

struct SDL_GPUDevice;
struct SDL_Window;
struct SDL_Thread;
struct CameraPerspective;
struct MouseControl;
struct SDL_GPUShader;

struct AppState {
    SDL_GPUDevice *device;
    SDL_Window *window;

    CameraPerspective *camera;
    MouseControl *mouseControl;

    SDL_GPUShader *floorShader;

    // some timing variables
    std::uint64_t lastTime, currentTime;
    std::uint32_t numFrames;
    float delta_time = 1.f;
};
