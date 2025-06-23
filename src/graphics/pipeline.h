#pragma once

#include <SDL3/SDL_gpu.h>

SDL_GPUGraphicsPipeline* createGPUPipeline(
    SDL_GPUDevice* device,
    SDL_Window* window,
    SDL_GPUTextureFormat depthStencilFormat,
    SDL_GPUShader* vertShader,
    SDL_GPUShader* fragShader);
