#include "api.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

// C'est seulement un exemple d'implementation

SDL_AppResult graphics_init(AppState& state, int argc, char** argv) {
    (void)state;
    (void)argc;
    (void)argv;

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
    (void)state;
}
