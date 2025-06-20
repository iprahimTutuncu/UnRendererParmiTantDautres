#include "api.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

static SDL_GPUShader* loadShader(
    SDL_GPUDevice* device,
    const char* shaderPath,
    Uint32 samplerCount,
    Uint32 uniformBufferCount,
    Uint32 storageBufferCount,
    Uint32 storageTextureCount) {
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
        SDL_Log("Failed to load shader from disk! %s", shaderPath);
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
    if (shader == nullptr) {
        SDL_Log("Failed to create shader!");
        SDL_free(code);
        return nullptr;
    }

    SDL_free(code);
    return shader;
}

SDL_AppResult graphics_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    // create shaders

    std::size_t codeSize;
    void* code = SDL_LoadFile(SHADERS_DIR "floorShader.frag.spv", &codeSize);
    if (code == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Fail to read file: %s", SHADERS_DIR "floorShader.frag.spv");
        return SDL_APP_FAILURE;
    }

    SDL_GPUShaderCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.code = reinterpret_cast<const Uint8*>(code);
    shaderCreateInfo.entrypoint = "main";
    shaderCreateInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shaderCreateInfo.num_samplers = 0;
    shaderCreateInfo.num_storage_textures = 0;
    shaderCreateInfo.num_storage_buffers = 1;
    shaderCreateInfo.num_uniform_buffers = 0;

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_iterate(AppState& state) {
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(state.device);
    if (cmdbuf == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to acquired GPU Command Buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture* swapchainTexture;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmdbuf, state.window, &swapchainTexture, nullptr, nullptr)) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to acquire GPU Swapchain Texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (swapchainTexture == nullptr) [[unlikely]] {
        // the window is minimized
        if (!SDL_CancelGPUCommandBuffer(cmdbuf)) [[unlikely]] {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to cancel GPU Command Buffer: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        return SDL_APP_CONTINUE;
    }

    SDL_GPUColorTargetInfo colorTargetInfo = {};
    colorTargetInfo.texture = swapchainTexture;
    colorTargetInfo.clear_color = SDL_FColor { 0.3f, 0.4f, 0.5f, 1.0f };
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, nullptr);

    SDL_EndGPURenderPass(renderPass);

    if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to submit GPU Command Buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_event(AppState& state, SDL_Event& event) {
    (void)state;
    (void)event;
    return SDL_APP_CONTINUE;
}

void graphics_quit(AppState& state) {
    delete state.graphics;
}
