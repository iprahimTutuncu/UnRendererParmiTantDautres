#include "shaders.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

SDL_GPUShader* loadShader(
    SDL_GPUDevice* device,
    const char* shaderPath,
    std::uint32_t samplerCount,
    std::uint32_t uniformBufferCount,
    std::uint32_t storageBufferCount,
    std::uint32_t storageTextureCount) {
    // Auto-detect the shader stage from the file name for convenience
    SDL_GPUShaderStage stage;
    if (SDL_strstr(shaderPath, ".vert")) {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else if (SDL_strstr(shaderPath, ".frag")) {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    } else {
        SDL_Log("Invalid shader stage!");
        return nullptr;
    }

    size_t codeSize;
    void* code = SDL_LoadFile(shaderPath, &codeSize);
    if (code == nullptr) [[unlikely]] {
        SDL_Log("Failed to load %s from disk: %s", shaderPath, SDL_GetError());
        return nullptr;
    }

    SDL_GPUShaderCreateInfo shaderInfo = {};
    shaderInfo.code = reinterpret_cast<const Uint8*>(code);
    shaderInfo.code_size = codeSize;
    shaderInfo.entrypoint = "main";
    shaderInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    shaderInfo.stage = stage;
    shaderInfo.num_samplers = samplerCount;
    shaderInfo.num_uniform_buffers = uniformBufferCount;
    shaderInfo.num_storage_buffers = storageBufferCount;
    shaderInfo.num_storage_textures = storageTextureCount;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
    if (shader == nullptr) [[unlikely]] {
        SDL_Log("Failed to create shader from %s: %s", shaderPath, SDL_GetError());
        SDL_free(code);
        return nullptr;
    }

    SDL_free(code);
    return shader;
}

SDL_GPUComputePipeline* CreateComputePipelineFromShader(SDL_GPUDevice* device,
    const char* shaderPath,
    SDL_GPUComputePipelineCreateInfo* createInfo) {
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_SPIRV;
    const char* entrypoint = "main";

    size_t codeSize = 0;
    void* code = SDL_LoadFile(shaderPath, &codeSize);
    if (code == nullptr) {
        SDL_Log("Failed to load compute shader %s from disk: %s", shaderPath, SDL_GetError());
        return nullptr;
    }

    SDL_GPUComputePipelineCreateInfo newCreateInfo = *createInfo;
    newCreateInfo.code = static_cast<const Uint8*>(code);
    newCreateInfo.code_size = codeSize;
    newCreateInfo.entrypoint = entrypoint;
    newCreateInfo.format = format;

    SDL_Log("Creating compute pipeline with shader: %s (format: %d, entrypoint: %s)",
        shaderPath, format, entrypoint);

    SDL_GPUComputePipeline* pipeline = SDL_CreateGPUComputePipeline(device, &newCreateInfo);
    if (pipeline == nullptr) {
        SDL_Log("Pipeline creation failed for shader: %s, format: %d, size: %zu",
            shaderPath, format, codeSize);
        SDL_free(code);
        return nullptr;
    }

    SDL_free(code);
    return pipeline;
}
