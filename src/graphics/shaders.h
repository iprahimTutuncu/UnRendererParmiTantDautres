#pragma once
#include <cstdint>

struct SDL_GPUShader;
struct SDL_GPUDevice;
struct SDL_GPUComputePipeline;
struct SDL_GPUComputePipelineCreateInfo;

#define SHADER_PATH(name) SHADERS_DIR name ".spv"

SDL_GPUShader* loadShader(
    SDL_GPUDevice* device,
    const char* shaderPath,
    std::uint32_t samplerCount,
    std::uint32_t uniformBufferCount,
    std::uint32_t storageBufferCount,
    std::uint32_t storageTextureCount);

SDL_GPUComputePipeline* createComputePipelineFromShader(
    SDL_GPUDevice* device,
    const char* shaderPath,
    SDL_GPUComputePipelineCreateInfo* createInfo);
