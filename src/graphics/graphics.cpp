#include "graphics.h"

#include "components.h"
#include "shaders.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include <stddef.h>

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
    SDL_GPUShader* vertShader = loadShader(state.device, SHADER_PATH("PositionColor.vert"), 0, 0, 0, 0);
    if (vertShader == nullptr) [[unlikely]] {
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader* fragShader = loadShader(state.device, SHADER_PATH("SolidColor.frag"), 0, 0, 0, 0);
    if (fragShader == nullptr) [[unlikely]] {
        SDL_ReleaseGPUShader(state.device, vertShader);
        return SDL_APP_FAILURE;
    }

    // create the graphics pipeline
    {
        SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo {};
        pipelineCreateInfo.vertex_shader = vertShader;
        pipelineCreateInfo.fragment_shader = fragShader;

        // Vertex input state
        SDL_GPUVertexBufferDescription vertexBufferDescriptions[NumBuffers] {};
        vertexBufferDescriptions[VertexBuffer].slot = 0;
        vertexBufferDescriptions[VertexBuffer].pitch = sizeof(PositionColorVertex);
        vertexBufferDescriptions[VertexBuffer].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute vertexAttributes[NumVertexAttributes] {};
        vertexAttributes[Position].location = VertexAttributeLocation::Position;
        vertexAttributes[Position].buffer_slot = 0;
        vertexAttributes[Position].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[Position].offset = offsetof(PositionColorVertex, x);
        vertexAttributes[Color].location = VertexAttributeLocation::Color;
        vertexAttributes[Color].buffer_slot = 0;
        vertexAttributes[Color].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        vertexAttributes[Color].offset = offsetof(PositionColorVertex, r);

        pipelineCreateInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescriptions;
        pipelineCreateInfo.vertex_input_state.num_vertex_buffers = NumBuffers;
        pipelineCreateInfo.vertex_input_state.vertex_attributes = vertexAttributes;
        pipelineCreateInfo.vertex_input_state.num_vertex_attributes = NumVertexAttributes;

        // Depth stencil state
        pipelineCreateInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pipelineCreateInfo.depth_stencil_state.write_mask = 0xFF;
        pipelineCreateInfo.depth_stencil_state.enable_depth_write = true;
        pipelineCreateInfo.depth_stencil_state.enable_stencil_test = true;

        // Target info
        SDL_GPUColorTargetDescription colorTargetDescription[1] {};
        colorTargetDescription[0].format = SDL_GetGPUSwapchainTextureFormat(state.device, state.window);
        pipelineCreateInfo.target_info.color_target_descriptions = colorTargetDescription;
        pipelineCreateInfo.target_info.num_color_targets = 1;
        pipelineCreateInfo.target_info.depth_stencil_format = depthStencilFormat;
        pipelineCreateInfo.target_info.has_depth_stencil_target = false;
        graphics.pipeline[Pipeline] = SDL_CreateGPUGraphicsPipeline(state.device, &pipelineCreateInfo);
    }

    SDL_ReleaseGPUShader(state.device, fragShader);
    SDL_ReleaseGPUShader(state.device, vertShader);
    if (graphics.pipeline[Pipeline] == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create the render pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // ======= Floor =======

    static constexpr PositionColorVertex positions[] {
        { -1, -1, 0, 255, 0, 0, 255 },
        { 1, -1, 0, 0, 255, 0, 255 },
        { 0, 1, 0, 0, 0, 255, 255 },
    };

    SDL_GPUBufferCreateInfo bufferCreateInfo {};
    bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferCreateInfo.size = sizeof(positions);

    graphics.buffers[VertexBuffer] = SDL_CreateGPUBuffer(state.device, &bufferCreateInfo);
    if (graphics.buffers[VertexBuffer] == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a vertex buffers: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {};
    transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferBufferCreateInfo.size = sizeof(positions);

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(state.device, &transferBufferCreateInfo);
    if (transferBuffer == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a GPU transfer buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    PositionColorVertex* transferData = reinterpret_cast<PositionColorVertex*>(SDL_MapGPUTransferBuffer(state.device, transferBuffer, false));
    if (transferData == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map GPU transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        return SDL_APP_FAILURE;
    }

    // Copy the vertex data to the transfer buffer
    for (size_t i = 0; i < sizeof(positions) / sizeof(PositionColorVertex); i++) {
        transferData[i] = positions[i];
    }

    SDL_UnmapGPUTransferBuffer(state.device, transferBuffer);

    SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
    if (uploadCmdBuf == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to acquired GPU Command Buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        return SDL_APP_FAILURE;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
    SDL_GPUTransferBufferLocation transfertLocation {};
    transfertLocation.transfer_buffer = transferBuffer;

    SDL_GPUBufferRegion bufferRegion {};
    bufferRegion.buffer = graphics.buffers[VertexBuffer];
    bufferRegion.size = transferBufferCreateInfo.size;

    SDL_UploadToGPUBuffer(copyPass, &transfertLocation, &bufferRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    if (!SDL_SubmitGPUCommandBuffer(uploadCmdBuf)) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to submit GPU Command Buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        return SDL_APP_FAILURE;
    }

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

    SDL_GPUColorTargetInfo colorTargetInfo;
    colorTargetInfo.texture = swapchainTexture;
    colorTargetInfo.clear_color = SDL_FColor { 0.f, 0.f, 0.f, 1.0f };
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
        cmdbuf,
        &colorTargetInfo,
        1u,
        nullptr);

    SDL_GPUBufferBinding vertexBinding[] {
        {
            .buffer = state.graphics->buffers[VertexBuffer],
            .offset = 0,
        }
    };
    SDL_BindGPUGraphicsPipeline(renderPass, state.graphics->pipeline[Pipeline]);
    SDL_BindGPUVertexBuffers(renderPass, 0, vertexBinding, sizeof(vertexBinding) / sizeof(SDL_GPUBufferBinding));
    SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

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
