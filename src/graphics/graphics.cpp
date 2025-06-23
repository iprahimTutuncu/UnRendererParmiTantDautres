#include "graphics.h"

#include "../camera.h"
#include "components.h"
#include "pipeline.h"
#include "shaders.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

struct UniformBufferObject {
    mat4 uProjMatrix;
    mat4 uViewMatrix;
    mat4 uModelMatrix;
};

SDL_AppResult graphics_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    state.graphics = new GraphicState {}; // will be free in graphics_quit()
    GraphicState& graphics = *state.graphics;

    SDL_GPUTextureFormat depthStencilFormat;
    if (SDL_GPUTextureSupportsFormat(
            state.device,
            SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    } else if (SDL_GPUTextureSupportsFormat(
                   state.device,
                   SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
                   SDL_GPU_TEXTURETYPE_2D,
                   SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    } else [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No suitable depth stencil format found for the GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // create the shaders
    SDL_GPUShader* vertShader = loadShader(state.device, SHADERS_DIR "floorShader.vert.spv", 0, 1, 1, 0);
    if (vertShader == nullptr) [[unlikely]] {
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader* fragShader = loadShader(state.device, SHADERS_DIR "floorShader.frag.spv", 0, 0, 1, 0);
    if (fragShader == nullptr) [[unlikely]] {
        SDL_ReleaseGPUShader(state.device, vertShader);
        return SDL_APP_FAILURE;
    }

    graphics.pipeline[ScenePipeline] = createScenePipeline(state.device, state.window, depthStencilFormat, vertShader, fragShader);
    SDL_ReleaseGPUShader(state.device, fragShader);
    SDL_ReleaseGPUShader(state.device, vertShader);
    if (graphics.pipeline[ScenePipeline] == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create the render pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // ======= Floor =======

    constexpr float size = 50.f;
    static PositionVertex positions[] {
        { -1.f, 0.f, -1.f },
        { 1.f, 0.f, 1.f },
        { 1.f, 0.f, -1.f },
        { 1.f, 0.f, 1.f },
        { -1.f, 0.f, -1.f },
        { -1.f, 0.f, 1.f },
    };

    for (std::size_t i = 0; i < sizeof(positions) / sizeof(PositionVertex); ++i) {
        positions[i].x *= size / 2.f;
        positions[i].y *= size / 2.f;
        positions[i].z *= size / 2.f;
    }

    SDL_GPUBufferCreateInfo bufferCreateInfo {};
    bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferCreateInfo.size = sizeof(positions);

    graphics.buffers[SceneBuffer] = SDL_CreateGPUBuffer(state.device, &bufferCreateInfo);
    if (graphics.buffers[SceneBuffer] == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a vertex buffers: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create the floor texture
    {
        int width, height;
        SDL_GetWindowSizeInPixels(state.window, &width, &height);

        SDL_GPUTextureCreateInfo gpuTextureCreateInfo {};
        gpuTextureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
        gpuTextureCreateInfo.format = depthStencilFormat;
        gpuTextureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        gpuTextureCreateInfo.width = static_cast<Uint32>(width);
        gpuTextureCreateInfo.height = static_cast<Uint32>(height);
        gpuTextureCreateInfo.layer_count_or_depth = 1;
        gpuTextureCreateInfo.num_levels = 1;
        gpuTextureCreateInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        graphics.textures[SceneDepthTexture] = SDL_CreateGPUTexture(state.device, &gpuTextureCreateInfo);
        if (graphics.textures[SceneDepthTexture] == nullptr) [[unlikely]] {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a GPU Texture: %s", SDL_GetError());

            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = sizeof(positions),
        .props = 0,
    };
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(state.device, &transferBufferCreateInfo);
    if (transferBuffer == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a GPU transfer buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    PositionVertex* transferData = reinterpret_cast<PositionVertex*>(SDL_MapGPUTransferBuffer(state.device, transferBuffer, false));
    if (transferData == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map GPU transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        return SDL_APP_FAILURE;
    }

    for (size_t i = 0; i < sizeof(positions) / sizeof(PositionVertex); i++) {
        transferData[i] = positions[i];
    }

    SDL_UnmapGPUTransferBuffer(state.device, transferBuffer);

    SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
    if (uploadCmdBuf == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to acquired GPU Command Buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
    const SDL_GPUTransferBufferLocation transfertLocation {
        .transfer_buffer = transferBuffer,
        .offset = 0,
    };
    const SDL_GPUBufferRegion bufferRegion {
        .buffer = graphics.buffers[SceneBuffer],
        .offset = 0,
        .size = transferBufferCreateInfo.size,
    };
    SDL_UploadToGPUBuffer(copyPass, &transfertLocation, &bufferRegion, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
    SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);

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

    static SDL_GPUColorTargetInfo colorTargetInfo {
        .texture = nullptr,
        .mip_level = 0,
        .layer_or_depth_plane = 0,
        .clear_color = SDL_FColor { 0.3f, 0.4f, 0.5f, 1.0f },
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .resolve_texture = nullptr,
        .resolve_mip_level = 0,
        .resolve_layer = 0,
        .cycle = false,
        .cycle_resolve_texture = false,
        .padding1 = 0,
        .padding2 = 0,
    };

    colorTargetInfo.texture = swapchainTexture;
    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, nullptr);

    UniformBufferObject uniformBlob;
    uniformBlob.uProjMatrix = state.camera->projection_matrix();
    uniformBlob.uViewMatrix = state.camera->view_matrix();
    uniformBlob.uModelMatrix = mat4::identity();

    SDL_PushGPUVertexUniformData(cmdbuf, 0, &uniformBlob, sizeof(uniformBlob));

    SDL_GPUBufferBinding vertexBinding[] {
        {
            .buffer = state.graphics->buffers[SceneBuffer],
            .offset = 0,
        }
    };
    SDL_BindGPUVertexBuffers(renderPass, 0, vertexBinding, 1);

    SDL_SetGPUStencilReference(renderPass, 1);
    SDL_BindGPUGraphicsPipeline(renderPass, state.graphics->pipeline[ScenePipeline]);
    SDL_DrawGPUPrimitives(renderPass, 6, 1, 0, 0);
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
    if (state.graphics) {
        for (std::size_t i = 0; i < NumPipelines; i++) {
            if (state.graphics->pipeline[i])
                SDL_ReleaseGPUGraphicsPipeline(state.device, state.graphics->pipeline[i]);
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
