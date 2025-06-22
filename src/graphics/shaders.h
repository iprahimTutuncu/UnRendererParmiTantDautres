#pragma once
#include <cstdint>

struct SDL_GPUShader;
struct SDL_GPUDevice;

SDL_GPUShader* loadShader(
    SDL_GPUDevice* device,
    const char* shaderPath,
    std::uint32_t samplerCount,
    std::uint32_t uniformBufferCount,
    std::uint32_t storageBufferCount,
    std::uint32_t storageTextureCount);
