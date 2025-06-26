#include "graphics.h"

#include "../camera.h"
#include "imguisdl.h"
#include "shaders.h"
#include "texture.h"
#include "deferred_lighting_renderer.h"
#include "deferred_gbuffer_renderer.h"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include <stddef.h>
#include <SDL3/SDL_hints.h>

SDL_AppResult graphics_init(AppState& state, [[maybe_unused]] int argc, [[maybe_unused]] char** argv) {

    state.graphics = new GraphicState {}; // Freed in graphics_quit()

    SDL_GetHintBoolean(SDL_HINT_RENDER_VULKAN_DEBUG, true);


    int w, h;
    if (!SDL_GetWindowSize(state.window, &w, &h))
        return SDL_APP_FAILURE;


    createRenderTarget(state, GeometryPosition, w, h, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    createRenderTarget(state, GeometryNormal, w, h, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    createRenderTarget(state, GeometryAlbedo, w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    createRenderTarget(state, GeometryDepth, w, h, SDL_GPU_TEXTUREFORMAT_D24_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);

    imgui_init(state);
    init_sampler_presets(state);

    int gridSizeX = 10;
    int gridSizeY = 10;
    int gridSizeZ = 10;
    float spacing = 1.0f; // distance between particles

    state.graphics->particles.clear();
    state.graphics->particles.reserve(gridSizeX * gridSizeY * gridSizeZ);

    // Offset to center the grid at origin
    float offsetX = (gridSizeX - 1) * spacing * 0.5f;
    float offsetY = (gridSizeY - 1) * spacing * 0.5f;
    float offsetZ = (gridSizeZ - 1) * spacing * 0.5f;

    for (int z = 0; z < gridSizeZ; ++z) {
        for (int y = 0; y < gridSizeY; ++y) {
            for (int x = 0; x < gridSizeX; ++x) {
                Particle p = {};
                p.position[0] = x * spacing - offsetX;
                p.position[1] = y * spacing - offsetY;
                p.position[2] = z * spacing - offsetZ;
                p.position[3] = 1.0f;

                p.color[0] = 1.0f;
                p.color[1] = 1.0f;
                p.color[2] = 1.0f;
                p.color[3] = 1.0f;

                state.graphics->particles.push_back(p);
            }
        }
    }

    state.graphics->particleCount = static_cast<int>(state.graphics->particles.size());

    if (SDL_AppResult result = deferred_lighting_init(state); result != SDL_APP_CONTINUE)
        return result;

    if (SDL_AppResult result = deferred_gbuffer_init(state); result != SDL_APP_CONTINUE)
        return result;

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_iterate(AppState& state) {
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(state.device);
    if (cmdbuf == nullptr) [[unlikely]] {
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

    deferred_gbuffer_render(state, cmdbuf);

    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = swapchainTexture;
    colorTarget.clear_color = SDL_FColor { 0.0f, 0.0f, 0.0f, 1.0f };
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTarget, 1, nullptr);

    deferred_lighting_render_to_texture(state, renderPass, cmdbuf, DisplayMode::Normal);
    imgui_iterate(state, renderPass, cmdbuf);

    SDL_EndGPURenderPass(renderPass);

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_event(AppState& state, SDL_Event& event) {
    imgui_event(state, event);

    return SDL_APP_CONTINUE;
}

void graphics_quit(AppState& state) {
    imgui_quit(state);
    if (state.graphics) {
        for (std::size_t i = 0; i < NumGraphicPipelines; i++) {
            if (state.graphics->graphicPipeline[i])
                SDL_ReleaseGPUGraphicsPipeline(state.device, state.graphics->graphicPipeline[i]);
        }

        for (std::size_t i = 0; i < NumComputePipelines; i++) {
            if (state.graphics->computePipeline[i])
                SDL_ReleaseGPUComputePipeline(state.device, state.graphics->computePipeline[i]);
        }

        for (std::size_t i = 0; i < NumBuffers; i++) {
            if (state.graphics->buffers[i])
                SDL_ReleaseGPUBuffer(state.device, state.graphics->buffers[i]);
        }

        for (std::size_t i = 0; i < NumTextures; i++) {
            if (state.graphics->textures[i])
                SDL_ReleaseGPUTexture(state.device, state.graphics->textures[i]);
        }

        delete state.graphics;
    }
}
