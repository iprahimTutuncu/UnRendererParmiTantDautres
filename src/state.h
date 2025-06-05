#pragma once

#include <cstdint>

struct SDL_GPUDevice;
struct SDL_Window;
struct SDL_Thread;
struct GPUStruct;

struct AppState {
    SDL_GPUDevice *device;
    SDL_Window *window;

    GPUStruct *gpuSTruc;


    // some timing variables
    std::uint64_t lastTime, currentTime;
    std::uint32_t numFrames;
};
