#include "graphics.h"

#include "deferred_gbuffer_renderer.h"
#include "deferred_lighting_renderer.h"
#include "imguisdl.h"
#include "texture.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_log.h>

#include <stddef.h>
#include <imgui_impl_sdlgpu3.h>
#include <ctime>

SDL_AppResult graphics_init(AppState& state, [[maybe_unused]] int argc, [[maybe_unused]] char** argv) {

    state.graphics = new GraphicState {}; // Freed in graphics_quit()
    GraphicState& graphics = *state.graphics;

    SDL_GetHintBoolean(SDL_HINT_RENDER_VULKAN_DEBUG, true);

    int w, h;
    if (!SDL_GetWindowSize(state.window, &w, &h))
        return SDL_APP_FAILURE;

    createRenderTarget(state, GeometryPosition, w, h, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE);
    createRenderTarget(state, GeometryNormal, w, h, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE);
    createRenderTarget(state, GeometryAlbedo, w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE);
    createRenderTarget(state, GeometryDepth, w, h, SDL_GPU_TEXTUREFORMAT_D24_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    createRenderTarget(state, GeometryDepthModified, w, h, SDL_GPU_TEXTUREFORMAT_R32_FLOAT, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE);

    imgui_init(state);
    init_sampler_presets(state);

    // Initialize random seed
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    static constexpr std::size_t numParticles = 5000; // Total number of random particles
    static constexpr float sizeX = 10.0f;
    static constexpr float sizeY = 10.0f;
    static constexpr float sizeZ = 10.0f;

    // Half-extents to center the cloud at origin
    static constexpr float offsetX = sizeX * 0.5f;
    static constexpr float offsetY = sizeY * 0.5f;
    static constexpr float offsetZ = sizeZ * 0.5f;

    graphics.particles.resize(numParticles);
    Particle* p = graphics.particles.data();

    for (std::size_t i = 0; i < numParticles; ++i) {
        // Random position within a centered cube
        p->position[0] = ((std::rand() % 1000) / 1000.0f) * sizeX - offsetX;
        p->position[1] = ((std::rand() % 1000) / 1000.0f) * sizeY - offsetY;
        p->position[2] = ((std::rand() % 1000) / 1000.0f) * sizeZ - offsetZ;
        p->position[3] = 1.0f;

        // Random color
        p->color[0] = (std::rand() % 256) / 255.0f;
        p->color[1] = (std::rand() % 256) / 255.0f;
        p->color[2] = (std::rand() % 256) / 255.0f;
        p->color[3] = 1.0f;

        ++p;
    }

    graphics.boxes.resize(1);

    graphics.boxes[0].min[0] = -0.5f;
    graphics.boxes[0].min[1] = -0.5f;
    graphics.boxes[0].min[2] = -0.5f;

    graphics.boxes[0].max[0] = 0.5f;
    graphics.boxes[0].max[1] = 0.5f;
    graphics.boxes[0].max[2] = 0.5f;


    if (SDL_AppResult result = deferred_lighting_init(state); result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    if (SDL_AppResult result = deferred_gbuffer_init(state); result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    //if (SDL_AppResult result = deferred_ssao_init(state); result != SDL_APP_CONTINUE) [[unlikely]]
    //    return result;


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


    // deferred_ssao_render(state, cmdbuf);

    // ou

    /*
    * 
    * 
    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = state.textures[ssaoTexture];
    colorTarget.clear_color = SDL_FColor { 0.0f, 0.0f, 0.0f, 1.0f };
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTarget, 1, nullptr);
    deferred_ssao_render(state, renderPass, cmdbuf);
    SDL_EndGPURenderPass(renderPass);
    */

    imgui_iterate(state);
    ImDrawData* draw_data = ImGui::GetDrawData();
    Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);

    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = swapchainTexture;
    colorTarget.clear_color = SDL_FColor { 0.0f, 0.0f, 0.0f, 1.0f };
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTarget, 1, nullptr);

    state.graphics->displayMode = DisplayMode::Final;
    deferred_lighting_render_to_texture(state, renderPass, cmdbuf, state.graphics->displayMode); // le "to_texture" dans le nom je vais retirer
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, renderPass);

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
