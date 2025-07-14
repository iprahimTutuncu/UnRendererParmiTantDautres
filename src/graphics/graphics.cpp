#include "graphics.h"

#include "deferred_gbuffer_renderer.h"
#include "deferred_lighting_renderer.h"
#include "imguisdl.h"
#include "texture.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>

#include <imgui_impl_sdlgpu3.h>

#include <stddef.h>

static SDL_AppResult graphics_create_render_targets(AppState& state);

SDL_AppResult graphics_init(AppState& state, [[maybe_unused]] int argc, [[maybe_unused]] char** argv) {

    state.graphics = new GraphicState {}; // Freed in graphics_quit()
    GraphicState& graphics = *state.graphics;

    SDL_GetHintBoolean(SDL_HINT_RENDER_VULKAN_DEBUG, true);

    if (SDL_AppResult result = graphics_create_render_targets(state); result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    imgui_init(state);
    init_sampler_presets(state);

    static constexpr std::size_t gridSizeX = 10;
    static constexpr std::size_t gridSizeY = 10;
    static constexpr std::size_t gridSizeZ = 10;
    static constexpr float spacing = 1.0f; // distance between particles

    // Offset to center the grid at origin
    static constexpr float offsetX = static_cast<float>(gridSizeX - 1) * spacing * 0.5f;
    static constexpr float offsetY = static_cast<float>(gridSizeY - 1) * spacing * 0.5f;
    static constexpr float offsetZ = static_cast<float>(gridSizeZ - 1) * spacing * 0.5f;

    graphics.particles.resize(gridSizeX * gridSizeY * gridSizeZ);
    Particle* p = graphics.particles.data();
    for (std::size_t z = 0; z < gridSizeZ; ++z) {
        for (std::size_t y = 0; y < gridSizeY; ++y) {
            for (std::size_t x = 0; x < gridSizeX; ++x) {
                p->position[0] = static_cast<float>(x) * spacing - offsetX;
                p->position[1] = static_cast<float>(y) * spacing - offsetY;
                p->position[2] = static_cast<float>(z) * spacing - offsetZ;
                p->position[3] = 1.0f;

                // Set all colors to white
                p->color[0] = 1.0f;
                p->color[1] = 1.0f;
                p->color[2] = 1.0f;
                p->color[3] = 1.0f;

                p += 1;
            }
        }
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

    return SDL_APP_CONTINUE;
}

static SDL_AppResult graphics_create_render_targets(AppState& state) {
    int width, height;
    if (!SDL_GetWindowSize(state.window, &width, &height))
        return SDL_APP_FAILURE;

    for (SDL_GPUTexture*& texture : state.graphics->textures) {
        if (texture) {
            SDL_ReleaseGPUTexture(state.device, texture);
            texture = nullptr;
        }
    }

    SDL_AppResult result;
    result = createRenderTarget(state, GeometryPosition, width, height, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    result = createRenderTarget(state, GeometryNormal, width, height, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    result = createRenderTarget(state, GeometryAlbedo, width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    result = createRenderTarget(state, GeometryDepth, width, height, SDL_GPU_TEXTUREFORMAT_D24_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    if (result != SDL_APP_CONTINUE) [[unlikely]]
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
    deferred_lighting_render_to_texture(state, renderPass, cmdbuf, state.graphics->displayMode);
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, renderPass);

    SDL_EndGPURenderPass(renderPass);

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_event(AppState& state, SDL_Event& event) {
    imgui_event(state, event);

    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
        if (SDL_AppResult result = graphics_create_render_targets(state); result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
    }

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
